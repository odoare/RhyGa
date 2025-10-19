#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//==============================================================================
// StepComponent Implementation
//==============================================================================
StepComponent::StepComponent(RhythmicGateAudioProcessor& p, int step, juce::LookAndFeel_V4& lookAndFeel) :
    stepIndex(step),
    onOffButton(p.apvts, ParameterID::get(step, "ON"), "", juce::Colours::cyan),
    durationSlider(p.apvts, ParameterID::get(step, "DUR"), juce::Colours::magenta.darker(1.2f), juce::Slider::LinearHorizontal),
    panSlider(p.apvts, ParameterID::get(step, "PAN"), juce::Colours::orange.darker(), juce::Slider::LinearHorizontal),
    levelMeter(p.apvts, ParameterID::get(step, "LVL"), juce::Colours::green),
    auxSendMeter(p.apvts, ParameterID::get(step, "AUX_LVL"), juce::Colours::cornflowerblue),
    linkButton(p.apvts, ParameterID::get(step, "LINK"), "", juce::Colours::grey.darker())
{
    // On/Off Button
    addAndMakeVisible(onOffButton);
    onOffButton.setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(durationSlider);
    durationSlider.setLookAndFeel(&lookAndFeel);
    
    addAndMakeVisible(panSlider);
    panSlider.setLookAndFeel(&lookAndFeel);
    panSlider.slider.getProperties().set ("drawFromCentre", true);

    addAndMakeVisible(levelMeter);
    addAndMakeVisible(auxSendMeter);
    levelMeter.setLookAndFeel(&lookAndFeel);
    auxSendMeter.setLookAndFeel(&lookAndFeel);

    // Link Button
    linkButton.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(linkButton);
}

void StepComponent::resized()
{
    static const int margin = 4;
    juce::FlexBox mainBox;
    mainBox.flexDirection = juce::FlexBox::Direction::column;
    mainBox.items.add(juce::FlexItem(onOffButton).withFlex(0.5f).withMargin(2));
    mainBox.items.add(juce::FlexItem(durationSlider).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(panSlider).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(levelMeter).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(auxSendMeter).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(linkButton).withFlex(0.5f).withMargin(juce::FlexItem::Margin(2.f, 15.f, 2.f, 15.f)));
    mainBox.performLayout(getLocalBounds());
}

void StepComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (active)
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
    }
    else if (isAccented)
    {
        // Draw a slightly lighter background for accented steps
        //auto backgroundColour = juce::Colour::fromFloatRGBA (0.15f, 0.15f, 0.25f, 1.0f);
        g.setColour(juce::Colour::fromFloatRGBA (0.25f, 0.25f, 0.3f, 1.0f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    }
}

void StepComponent::setActive(bool isActive)
{
    active = isActive;
    repaint();
}

void StepComponent::setAccented(bool shouldBeAccented)
{
    if (isAccented != shouldBeAccented)
    {
        isAccented = shouldBeAccented;
        repaint();
    }
}

//==============================================================================
// Main Editor Implementation
//==============================================================================
RhythmicGateAudioProcessorEditor::RhythmicGateAudioProcessorEditor (RhythmicGateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      attackKnob(p.apvts, "ATTACK", "Attack", juce::Colours::orangered.darker()),
      releaseKnob(p.apvts, "RELEASE", "Release", juce::Colours::orangered.darker())
{
    // Global metric selector (reordered to match PluginProcessor.cpp)
    metricSelector.addItem("8th",    1);
    metricSelector.addItem("8th T",  2);
    metricSelector.addItem("16th",   3);
    metricSelector.addItem("16th T", 4);
    metricSelector.addItem("32nd",   5);
    metricSelector.addItem("32nd T", 6);
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
        stepComponents[i] = std::make_unique<StepComponent>(audioProcessor, i, fxmeLookAndFeel);
        addAndMakeVisible(*stepComponents[i]);
    }

    // This helper sets up the callback for a control (like a slider or button)
    // to update all other linked controls when its value changes.
    auto setupLinkedControlCallbacks = [this](auto& sourceStep, auto& sourceControl, auto stepControlMember)
    {
        sourceControl.slider.onValueChange = [this, &sourceStep, &sourceControl, stepControlMember]
        {
            // If the step that was just changed is linked...
            if (sourceStep.linkButton.button.getToggleState())
            {
                // ...then find its parameter and get the new value.
                if (auto* sourceParam = sourceControl.getParameter())
                {
                    float newValue = sourceParam->getValue();

                    // Iterate over all other steps...
                    for (auto& targetStep : stepComponents)
                    {
                        // ...and if they are also linked (and not the source itself)...
                        if (targetStep.get() != &sourceStep && targetStep->linkButton.button.getToggleState())
                        {
                            // ...update their corresponding parameter.
                            if (auto* targetParam = (targetStep.get()->*stepControlMember).getParameter())
                            {
                                targetParam->setValueNotifyingHost(newValue);
                            }
                        }
                    }
                }
            }
        };
    };

    auto setupLinkedButtonCallbacks = [this](auto& sourceStep, auto& sourceControl, auto stepControlMember)
    {
        sourceControl.button.onClick = [this, &sourceStep, &sourceControl, stepControlMember]
        {
            // If the step that was just changed is linked...
            if (sourceStep.linkButton.button.getToggleState())
            {
                // ...then find its parameter and get the new value.
                // For a toggle button, the state has just been flipped by the click.
                if (auto* sourceParam = sourceControl.getParameter())
                {
                    float newValue = sourceParam->getValue();

                    // Iterate over all other steps...
                    for (auto& targetStep : stepComponents)
                    {
                        // ...and if they are also linked (and not the source itself)...
                        if (targetStep.get() != &sourceStep && targetStep->linkButton.button.getToggleState())
                        {
                            // ...update their corresponding parameter.
                            if (auto* targetParam = (targetStep.get()->*stepControlMember).getParameter())
                                targetParam->setValueNotifyingHost(newValue);
                        }
                    }
                }
            }
        };
    };

    // Now, apply this logic to all sliders in all step components.
    for (auto& step : stepComponents)
    {
        setupLinkedButtonCallbacks(*step, step->onOffButton, &StepComponent::onOffButton);
        setupLinkedControlCallbacks(*step, step->durationSlider, &StepComponent::durationSlider);
        setupLinkedControlCallbacks(*step, step->panSlider,      &StepComponent::panSlider);
        setupLinkedControlCallbacks(*step, step->levelMeter,     &StepComponent::levelMeter);
        setupLinkedControlCallbacks(*step, step->auxSendMeter,   &StepComponent::auxSendMeter);
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
    // Ternary metrics have even IDs (2, 4, 6) in the new order.
    const bool isTernary = (metricSelector.getSelectedId() % 2 == 0);

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
