/*
  ==============================================================================

    PluginProcessor.cpp
    Created: 01 Nov 2025 9:46:12pm
    Author:  doare

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RhythmicGateAudioProcessor::RhythmicGateAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Main",   juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Aux",    juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Get atomic pointers to all parameters for fast, thread-safe access in processBlock.
    // This is safe to do here because the `apvts` member has already been
    // fully constructed in the member initializer list above.
    metricParam = apvts.getRawParameterValue("METRIC");

    attackParam = apvts.getRawParameterValue("ATTACK");
    releaseParam = apvts.getRawParameterValue("RELEASE");
    stepsParam = apvts.getRawParameterValue("STEPS");
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        onOffParams[step]     = apvts.getRawParameterValue(ParameterID::get(step, "ON"));
        durationParams[step]  = apvts.getRawParameterValue(ParameterID::get(step, "DUR"));
        levelParams[step]     = apvts.getRawParameterValue(ParameterID::get(step, "LVL"));
        auxSendParams[step]   = apvts.getRawParameterValue(ParameterID::get(step, "AUX_LVL"));
        panParams[step]       = apvts.getRawParameterValue(ParameterID::get(step, "PAN"));
        linkParams[step]      = apvts.getRawParameterValue(ParameterID::get(step, "LINK"));

        // Cache parameter objects and initial values for linking logic
        onOffParamObjects[step]    = apvts.getParameter(ParameterID::get(step, "ON"));
        durationParamObjects[step] = apvts.getParameter(ParameterID::get(step, "DUR"));
        levelParamObjects[step]    = apvts.getParameter(ParameterID::get(step, "LVL"));
        auxSendParamObjects[step]  = apvts.getParameter(ParameterID::get(step, "AUX_LVL"));
        panParamObjects[step]      = apvts.getParameter(ParameterID::get(step, "PAN"));

        lastOnOffValues[step]    = onOffParamObjects[step]->getValue();
        lastDurationValues[step] = durationParamObjects[step]->getValue();
        lastLevelValues[step]    = levelParamObjects[step]->getValue();
        lastAuxSendValues[step]  = auxSendParamObjects[step]->getValue();
        lastPanValues[step]      = panParamObjects[step]->getValue();
    }
    previousTargetGain = -1.0f; // Initialize with a value that guarantees the first check will trigger
    internalPpq = 0.0;
}

RhythmicGateAudioProcessor::~RhythmicGateAudioProcessor()
{
    // No need to remove the listener anymore
}

//==============================================================================
const juce::String RhythmicGateAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void RhythmicGateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    gateSmoother.reset(sampleRate, 0.0); // Reset smoother with sample rate
    previousTargetGain = -1.0f; // Also reset here in case of sample rate change
    internalPpq = 0.0;
}

void RhythmicGateAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RhythmicGateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // We need 2 in, 4 out.
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& mainOut = layouts.getChannelSet(false, 0);
    const auto& auxOut = layouts.getChannelSet(false, 1);

    if (mainIn == juce::AudioChannelSet::disabled() || mainOut == juce::AudioChannelSet::disabled() || auxOut == juce::AudioChannelSet::disabled())
        return false;

    return (mainIn == juce::AudioChannelSet::stereo() &&
            mainOut == juce::AudioChannelSet::stereo() &&
            auxOut == juce::AudioChannelSet::stereo());
}
#endif

void RhythmicGateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels to prevent garbage audio
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get playhead from host
    auto* playHead = getPlayHead();
    if (playHead == nullptr)
    {
        // If no playhead, just pass audio through to main output
        auto* mainBus = getBus(false, 0);
        for(int ch = 0; ch < mainBus->getNumberOfChannels(); ++ch)
            buffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
        return;
    }

    auto optionalPositionInfo = playHead->getPosition();
    if (!optionalPositionInfo.hasValue())
    {
        // If no position info, mute all outputs
        activeStep = -1; // Reset active step when not playing
        buffer.clear();
        return;
    }

    const auto& positionInfo = *optionalPositionInfo;

    // Handle parameter linking before processing audio
    updateLinkedParameters();

    // Calculate BPM (default to 120 if unavailable) and PPQ increment per sample
    auto bpmOpt = positionInfo.getBpm();
    double bpm = bpmOpt.hasValue() ? *bpmOpt : 120.0;
    double ppqPerSample = bpm / (currentSampleRate * 60.0);

    // Determine the starting PPQ for this block
    double currentBlockPpq = 0.0;

    if (positionInfo.getIsPlaying() && positionInfo.getPpqPosition().hasValue())
    {
        currentBlockPpq = *positionInfo.getPpqPosition();
    }
    else
    {
        currentBlockPpq = internalPpq;
    }
    internalPpq = currentBlockPpq + buffer.getNumSamples() * ppqPerSample;

    // Get pointers to our separate output buses
    auto mainOutputBuffer = getBusBuffer(buffer, false, 0);
    auto auxOutputBuffer  = getBusBuffer(buffer, false, 1);

    int numSteps = static_cast<int>(stepsParam->load());

    float attackMs = attackParam->load();
    float releaseMs = releaseParam->load();

    // --- Rhythmic Gate Logic ---
    int metricIndex = static_cast<int>(metricParam->load());
    double stepDurationInPpq;

    switch (metricIndex)
    {
        case 0: stepDurationInPpq = 0.5;                break; // 8th
        case 1: stepDurationInPpq = 1.0 / 3.0;          break; // 8th Triplet
        case 2: stepDurationInPpq = 0.25;               break; // 16th
        case 3: stepDurationInPpq = 0.5 / 3.0;          break; // 16th Triplet
        case 4: stepDurationInPpq = 0.125;              break; // 32nd
        case 5: stepDurationInPpq = 0.25 / 3.0;         break; // 32nd Triplet
        default: stepDurationInPpq = 0.25;              break; // Fallback to 16th (default)
    }

    double sequenceDurationInPpq = numSteps * stepDurationInPpq;

    // Calculate active step for the GUI using the correct step duration
    double sequencePpqForGui = fmod(currentBlockPpq, sequenceDurationInPpq);
    activeStep = static_cast<int>(sequencePpqForGui / stepDurationInPpq);


    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Calculate precise position for this sample using the pre-calculated start PPQ
        double currentPpq = currentBlockPpq + (sample * ppqPerSample);
        
        // Find our position within the 32-step sequence
        double sequencePpq = fmod(currentPpq, sequenceDurationInPpq);
        int currentStep = static_cast<int>(sequencePpq / stepDurationInPpq);

        // Ensure step is within bounds
        currentStep = juce::jlimit(0, numSteps - 1, currentStep);

        // Calculate how far into the current step we are (0.0 to 1.0)
        double stepProgress = fmod(sequencePpq, stepDurationInPpq) / stepDurationInPpq;
        
        // Get current step's parameters (now shared between channels)
        bool isOn = onOffParams[currentStep]->load() > 0.5f;
        float duration = durationParams[currentStep]->load();
        float mainLevel = juce::Decibels::decibelsToGain(levelParams[currentStep]->load());
        float auxLevel = juce::Decibels::decibelsToGain(auxSendParams[currentStep]->load());
        float pan = panParams[currentStep]->load(); // -1 (L) to 1 (R)
        
        // Determine target gain for the smoother
        float targetGain = (isOn && stepProgress < duration) ? 1.0f : 0.0f;

        // Only update the smoother's target and ramp time when the target gain changes
        if (targetGain != previousTargetGain)
        {
            // Set ramp length based on whether we are opening (attack) or closing (release) the gate
            if (targetGain > previousTargetGain)
                gateSmoother.reset(currentSampleRate, attackMs / 1000.0); // Attack
            else
                gateSmoother.reset(currentSampleRate, releaseMs / 1000.0); // Release

            gateSmoother.setTargetValue(targetGain);
            previousTargetGain = targetGain;
        }

        float gain = gateSmoother.getNextValue();
        // Calculate pan gains using a constant-power pan law
        float panLeft = std::sqrt(0.5f * (1.0f - pan));
        float panRight = std::sqrt(0.5f * (1.0f + pan));

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float inputSample = buffer.getSample(channel, sample);
            float panGain = (channel == 0) ? panLeft : panRight;
            
            // Write to output buffers
            // Note: Aux send is also panned. To keep it mono, you could remove panGain from the auxOutputBuffer line.
            mainOutputBuffer.setSample(channel, sample, inputSample * gain * mainLevel * panGain);
            auxOutputBuffer.setSample(channel, sample, inputSample * gain * auxLevel * panGain);
        }
    }
}

void RhythmicGateAudioProcessor::updateLinkedParameters()
{
    // Helper lambda to handle linking for a specific parameter array
    auto handleLinking = [this](std::array<juce::AudioProcessorParameter*, NUM_STEPS>& params,
                                std::array<float, NUM_STEPS>& lastValues)
    {
        for (int i = 0; i < NUM_STEPS; ++i)
        {
            float currentValue = params[i]->getValue();
            if (std::abs(currentValue - lastValues[i]) > 0.0001f) // Check for change
            {
                // If this step is linked, propagate to other linked steps
                if (linkParams[i]->load() > 0.5f)
                {
                    for (int j = 0; j < NUM_STEPS; ++j)
                    {
                        if (i != j && linkParams[j]->load() > 0.5f)
                        {
                            params[j]->setValueNotifyingHost(currentValue);
                            lastValues[j] = currentValue; // Update history to prevent feedback
                        }
                    }
                }
                lastValues[i] = currentValue;
            }
        }
    };

    handleLinking(onOffParamObjects, lastOnOffValues);
    handleLinking(durationParamObjects, lastDurationValues);
    handleLinking(levelParamObjects, lastLevelValues);
    handleLinking(auxSendParamObjects, lastAuxSendValues);
    handleLinking(panParamObjects, lastPanValues);
}

//==============================================================================
juce::AudioProcessorEditor* RhythmicGateAudioProcessor::createEditor()
{
    return new RhythmicGateAudioProcessorEditor (*this);
}

//==============================================================================
void RhythmicGateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RhythmicGateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout RhythmicGateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global Metric Control
    params.push_back(std::make_unique<juce::AudioParameterChoice>("METRIC", "Metric", 
        juce::StringArray{"8th", "8th T", "16th", "16th T", "32nd", "32nd T"},
        2)); // Default to 16th (index 2)

    params.push_back(std::make_unique<juce::AudioParameterInt>("STEPS", "Steps", 2, 16, 16));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ATTACK",
        "Attack",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 0.3f), // 0-100ms, skewed
        0.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RELEASE",
        "Release",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 0.3f), // 0-100ms, skewed
        5.0f, "ms"));


    // Per-step controls
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            ParameterID::get(step, "ON"),
            "On " + juce::String(step + 1),
            true)); // Default to On

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParameterID::get(step, "DUR"),
            "Duration " + juce::String(step + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            1.0f)); // Default duration 1.0

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParameterID::get(step, "LVL"),
            "Level " + juce::String(step + 1),
            juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 4.0f),
            0.0f, "dB")); // Default level 0 dB

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParameterID::get(step, "AUX_LVL"),
            "Aux Send " + juce::String(step + 1),
            juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 4.0f),
            -60.0f, "dB")); // Default aux send -inf

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParameterID::get(step, "PAN"),
            "Pan " + juce::String(step + 1),
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
            0.0f)); // Default pan center
    }

    // Link buttons (non-automatable as they are UI state rather than audio parameters)
    for (int step = 0; step < NUM_STEPS; ++step)
    {
        auto attributes = juce::AudioParameterBoolAttributes()
                              .withCategory (juce::AudioProcessorParameter::Category::genericParameter)
                              .withAutomatable (false);

        params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterID::get (step, "LINK"), "Link " + juce::String (step), false, attributes));
    }
    return { params.begin(), params.end() };
}

//==============================================================================
// Boilerplate JUCE code...
bool RhythmicGateAudioProcessor::hasEditor() const { return true; }
bool RhythmicGateAudioProcessor::acceptsMidi() const { return false; }
bool RhythmicGateAudioProcessor::producesMidi() const { return false; }
bool RhythmicGateAudioProcessor::isMidiEffect() const { return false; }
double RhythmicGateAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int RhythmicGateAudioProcessor::getNumPrograms() { return 1; }
int RhythmicGateAudioProcessor::getCurrentProgram() { return 0; }
void RhythmicGateAudioProcessor::setCurrentProgram (int index) {}
const juce::String RhythmicGateAudioProcessor::getProgramName (int index) { return {}; }
void RhythmicGateAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RhythmicGateAudioProcessor();
}
