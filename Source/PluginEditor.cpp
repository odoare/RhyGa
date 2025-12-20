/*
  ==============================================================================

    PluginEditor.cpp
    Created: 01 Nov 2025 9:46:12pm
    Author:  doare

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Main Editor Implementation
//==============================================================================
RhythmicGateAudioProcessorEditor::RhythmicGateAudioProcessorEditor (RhythmicGateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      attackKnob(p.apvts, "ATTACK", "Attack", juce::Colours::orangered.darker()),
      releaseKnob(p.apvts, "RELEASE", "Release", juce::Colours::orangered.darker())
{
    // Global metric selector (reordered to match PluginProcessor.cpp)
    const auto& metrics = RhythmicGateAudioProcessor::getMetrics();
    for (int i = 0; i < metrics.size(); ++i)
    {
        metricSelector.addItem(metrics[i].name, i + 1);
    }
    addAndMakeVisible(metricSelector);
    metricAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "METRIC", metricSelector);
    metricSelector.onChange = [this] { updateStepAccents(); };

    // Steps selector
    for (int i = 2; i <= RhythmicGateAudioProcessor::NUM_STEPS; ++i)
        stepsSelector.addItem(juce::String(i), i);
    stepsSelector.setSelectedId(16);
    addAndMakeVisible(stepsSelector);
    stepsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "STEPS", stepsSelector);
    stepsSelector.onChange = [this] { updateStepComponentVisibility(); };

    // Attack and Release Knobs
    addAndMakeVisible(attackKnob);
    attackKnob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attackKnob.setLookAndFeel(&fxmeLookAndFeel);

    addAndMakeVisible(releaseKnob);
    releaseKnob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    releaseKnob.setLookAndFeel(&fxmeLookAndFeel);
    
    // --- Link Control Buttons ---
    addAndMakeVisible(linkAllButton);
    linkAllButton.setLookAndFeel(&fxmeLookAndFeel);
    linkAllButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    linkAllButton.onClick = [this] {
        for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i) {
            if (auto* param = audioProcessor.apvts.getParameter(ParameterID::get(i, "LINK")))
                param->setValueNotifyingHost(1.0f);
        }
    };

    addAndMakeVisible(linkNoneButton);
    linkNoneButton.setLookAndFeel(&fxmeLookAndFeel);
    linkNoneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    linkNoneButton.onClick = [this] {
        for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i) {
            if (auto* param = audioProcessor.apvts.getParameter(ParameterID::get(i, "LINK")))
                param->setValueNotifyingHost(0.0f);
        }
    };

    addAndMakeVisible(linkInvertButton);
    linkInvertButton.setLookAndFeel(&fxmeLookAndFeel);
    linkInvertButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    linkInvertButton.onClick = [this] {
        for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i)
        {
            if (auto* param = audioProcessor.apvts.getParameter(ParameterID::get(i, "LINK")))
                param->setValueNotifyingHost(param->getValue() < 0.5f ? 1.0f : 0.0f);
        }
    };

    // --- Create and setup Step Components ---
    for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i)
    {
        stepComponents[i] = std::make_unique<StepComponent>(audioProcessor.apvts, i, fxmeLookAndFeel);
        addAndMakeVisible(*stepComponents[i]);
    }

    // --- Setup Row Labels ---
    auto setupLabel = [this] (juce::Label& label)
    {
        label.setFont (juce::Font (juce::FontOptions (14.0f)));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (label);
    };

    setupLabel(onOffLabel);
    setupLabel(durationLabel);
    setupLabel(panLabel);
    setupLabel(levelLabel);
    setupLabel(auxLabel);
    setupLabel(linkLabel);

    addAndMakeVisible(logo);
    // Add the randomization callback when the logo is clicked
    logo.onClick = [this] { randomizeParameters(); };

    // Set initial visibility of step components
    updateStepComponentVisibility();
    updateStepAccents();

    setResizable(true, true);
    setResizeLimits(600, 250, 1800, 600);
    setSize (1024, 250);

    // Start the timer to update the GUI at 30 Hz
    startTimerHz(60);
}

RhythmicGateAudioProcessorEditor::~RhythmicGateAudioProcessorEditor()
{
}

//==============================================================================
void RhythmicGateAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto diagonale = (getLocalBounds().getTopLeft() - getLocalBounds().getBottomRight()).toFloat();
    auto length = diagonale.getDistanceFromOrigin();
    auto perpendicular = diagonale.rotatedAboutOrigin (juce::degreesToRadians (90.0f)) / length;
    auto height = float (getWidth() * getHeight()) / length;
    auto bluegreengrey = juce::Colour::fromFloatRGBA (0.15f, 0.15f, 0.25f, 1.0f);
    juce::ColourGradient grad (bluegreengrey.darker().darker().darker(), perpendicular * height,
                           bluegreengrey, perpendicular * -height, false);
    g.setGradientFill(grad);
    g.fillAll();
}

void RhythmicGateAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    juce::FlexBox mainLayout;
    mainLayout.flexDirection = juce::FlexBox::Direction::row;

    // Sequencer row
    juce::FlexBox sequencerRow;
    sequencerRow.alignItems = juce::FlexBox::AlignItems::stretch;
    for (auto& step : stepComponents)
        sequencerRow.items.add(juce::FlexItem(*step).withFlex(1.0f));

    // Horizontal box for the link control buttons, now on the right side
    juce::FlexBox linkButtonsBox;
    linkButtonsBox.flexDirection = juce::FlexBox::Direction::row;
    linkButtonsBox.items.add(juce::FlexItem(linkAllButton).withFlex(1.0f));
    linkButtonsBox.items.add(juce::FlexItem(linkNoneButton).withFlex(1.0f));
    linkButtonsBox.items.add(juce::FlexItem(linkInvertButton).withFlex(1.0f));

    juce::FlexBox arBox;
    arBox.flexDirection = juce::FlexBox::Direction::row;
    arBox.items.add(juce::FlexItem(attackKnob).withFlex(1.0f));
    arBox.items.add(juce::FlexItem(releaseKnob).withFlex(1.0f));

    // Vertical box for controls on the left
    juce::FlexBox leftPanel;
    leftPanel.flexDirection = juce::FlexBox::Direction::column;
    leftPanel.items.add(juce::FlexItem(logo).withFlex(1.f));
    leftPanel.items.add(juce::FlexItem(metricSelector).withFlex(.25f).withMargin(juce::FlexItem::Margin(5.f, 2.f, 5.f, 2.f)));
    leftPanel.items.add(juce::FlexItem(stepsSelector).withFlex(.25f).withMargin(juce::FlexItem::Margin(2.f, 2.f, 5.f, 2.f)));
    leftPanel.items.add(juce::FlexItem(arBox).withFlex(1.1f).withMargin(juce::FlexItem::Margin(5.0f, 2, 2, 2)));
    leftPanel.items.add(juce::FlexItem(linkButtonsBox).withFlex(0.3f));

    // Vertical box for the new labels
    juce::FlexBox labelPanel;
    labelPanel.flexDirection = juce::FlexBox::Direction::column;
    labelPanel.items.add(juce::FlexItem(onOffLabel).withHeight(20.0f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2)));
    labelPanel.items.add(juce::FlexItem(durationLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(panLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(levelLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(auxLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(linkLabel).withHeight(20.0f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2)));

    // Add panels and sequencer to the main layout
    mainLayout.items.add(juce::FlexItem(leftPanel).withFlex(2.f).withMargin(juce::FlexItem::Margin(0.f, 5.f, 0.f, 0.f)));
    mainLayout.items.add(juce::FlexItem(sequencerRow).withFlex(16.0f));
    mainLayout.items.add(juce::FlexItem(labelPanel).withFlex(1.2f));

    mainLayout.performLayout(bounds.reduced(10));
}

void RhythmicGateAudioProcessorEditor::timerCallback()
{
    int currentActiveStep = audioProcessor.activeStep.load();

    if (currentActiveStep != lastActiveStep)
    {
        // Deactivate the old step
        if (lastActiveStep >= 0 && lastActiveStep < stepComponents.size())
            stepComponents[lastActiveStep]->setActive(false);

        // Activate the new step
        if (currentActiveStep >= 0 && currentActiveStep < stepComponents.size())
            stepComponents[currentActiveStep]->setActive(true);

        lastActiveStep = currentActiveStep;
    }
}

void RhythmicGateAudioProcessorEditor::updateStepComponentVisibility()
{
    int numSteps = stepsSelector.getSelectedId();

    if (numSteps != lastNumSteps)
    {
        for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i)
        {
            const bool isVisible = (i < numSteps);
            stepComponents[i]->setVisible(isVisible);
        }
        lastNumSteps = numSteps;
    }
}

void RhythmicGateAudioProcessorEditor::updateStepAccents()
{
    const auto& metrics = RhythmicGateAudioProcessor::getMetrics();
    int selectedIndex = metricSelector.getSelectedId() - 1;
    
    bool isTernary = false;
    if (selectedIndex >= 0 && selectedIndex < metrics.size())
        isTernary = metrics[selectedIndex].isTriplet;

    for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i)
    {
        bool shouldBeAccented = false;
        if (isTernary)
        {
            // Accent every 3rd step for triplet feels
            if (i % 3 == 0)
                shouldBeAccented = true;
        }
        else
        {
            // Accent every 4th step for binary feels (downbeats)
            if (i % 4 == 0)
                shouldBeAccented = true;
        }
        stepComponents[i]->setAccented(shouldBeAccented);
    }
}

void RhythmicGateAudioProcessorEditor::randomizeParameters()
{
    juce::Random random;

    // We iterate through all parameters in the APVTS
    for (auto* param : audioProcessor.getParameters())
    {
        // We only want to randomize per-step controls, not global ones like STEPS, ATTACK, or RELEASE.
        // We also exclude the LINK parameters
        // To get the parameter ID, we must safely cast the base AudioProcessorParameter
        // to the RangedAudioParameter that it actually is.
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            const auto& paramID = rangedParam->getParameterID();

            if (!paramID.startsWith("METRIC") &&
                !paramID.startsWith("ATTACK") &&
                !paramID.startsWith("RELEASE") &&
                !paramID.contains("LINK"))
            {
                // Set the parameter to a random value within its normalized range (0.0 to 1.0).
                param->setValueNotifyingHost(random.nextFloat());
            }
        }
    }
}
