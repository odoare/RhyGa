#pragma once

#include <JuceHeader.h>

// A helper function to generate consistent parameter IDs
namespace ParameterID
{
    static juce::String get(int step, const juce::String& type)
    {
        return type + "_" + juce::String(step);
    }
}

//==============================================================================
class RhythmicGateAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    RhythmicGateAudioProcessor();
    ~RhythmicGateAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Use APVTS for robust parameter management
    juce::AudioProcessorValueTreeState apvts;

    // Public member to communicate the active step to the editor
    std::atomic<int> activeStep { -1 };

    static constexpr int NUM_STEPS = 16;
    static constexpr int NUM_CHANNELS = 2; // L/R for inputs

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Cached parameter pointers for real-time safety
    std::atomic<float>* metricParam = nullptr;
    std::atomic<float>* stepsParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;

    std::array<std::atomic<float>*, NUM_STEPS> onOffParams;
    std::array<std::atomic<float>*, NUM_STEPS> durationParams;
    std::array<std::atomic<float>*, NUM_STEPS> levelParams;
    std::array<std::atomic<float>*, NUM_STEPS> auxSendParams;
    std::array<std::atomic<float>*, NUM_STEPS> panParams;

    double currentSampleRate = 44100.0;
    
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateSmoother;

    float previousTargetGain = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmicGateAudioProcessor)
};