#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// StepComponent Implementation
//==============================================================================
StepComponent::StepComponent(RhythmicGateAudioProcessor& p, int step, juce::LookAndFeel_V4& lookAndFeel) :
    onOffButton(p.apvts, ParameterID::get(step, "ON"), "", juce::Colours::cyan),
    durationKnob(p.apvts, ParameterID::get(step, "DUR"), juce::Colours::purple.darker(), juce::Slider::LinearHorizontal),
    panKnob(p.apvts, ParameterID::get(step, "PAN"), juce::Colours::yellow.darker(1.5f), juce::Slider::LinearHorizontal),
    levelMeter(p.apvts, ParameterID::get(step, "LVL"), juce::Colours::green),
    auxSendMeter(p.apvts, ParameterID::get(step, "AUX_LVL"), juce::Colours::cornflowerblue)
{
    // On/Off Button
    addAndMakeVisible(onOffButton);
    onOffButton.setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(durationKnob);
    durationKnob.setLookAndFeel(&lookAndFeel);
    
    addAndMakeVisible(panKnob);
    panKnob.setLookAndFeel(&lookAndFeel);
    panKnob.slider.getProperties().set ("drawFromCentre", true);

    addAndMakeVisible(levelMeter);
    addAndMakeVisible(auxSendMeter);
    levelMeter.setLookAndFeel(&lookAndFeel);
    auxSendMeter.setLookAndFeel(&lookAndFeel);

    // Link Button (GUI only, no APVTS attachment)
    linkButton.setButtonText(""); // Small indicator
    linkButton.setToggleState(true, juce::dontSendNotification); // Default to linked
    linkButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::grey);
    linkButton.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(linkButton);
}

void StepComponent::resized()
{
    static const int margin = 4;
    juce::FlexBox mainBox;
    mainBox.flexDirection = juce::FlexBox::Direction::column;
    mainBox.items.add(juce::FlexItem(onOffButton).withFlex(0.5f).withMargin(2));
    mainBox.items.add(juce::FlexItem(durationKnob).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(panKnob).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(levelMeter).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(auxSendMeter).withFlex(1.0f).withMargin(margin));
    mainBox.items.add(juce::FlexItem(linkButton).withFlex(0.5f).withMargin(2));
    mainBox.performLayout(getLocalBounds());
}

void StepComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (active)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.6f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    }
    else if (isAccented)
    {
        // Draw a slightly lighter background for accented steps
        auto backgroundColour = juce::Colour::fromFloatRGBA (0.15f, 0.15f, 0.25f, 1.0f);
        g.setColour(backgroundColour.brighter(0.2f));
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
      releaseKnob(p.apvts, "RELEASE", "Release", juce::Colours::orangered.darker()),
      masterDurationKnob(p.apvts, "masterDur", juce::Colours::purple.darker(), juce::Slider::LinearHorizontal),
      masterPanKnob(p.apvts, "masterPan", juce::Colours::yellow.darker(), juce::Slider::LinearHorizontal),
      masterLevelMeter(p.apvts, "masterLvl", juce::Colours::green),
      masterAuxSendMeter(p.apvts, "masterAux", juce::Colours::cornflowerblue)

{
    // Global metric selector
    metricSelector.addItem("32nd",   1);
    metricSelector.addItem("32nd T", 2);
    metricSelector.addItem("16th",   3);
    metricSelector.addItem("16th T", 4);
    metricSelector.addItem("8th",    5);
    metricSelector.addItem("8th T",  6);
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
    auto setupLinkButton = [this](juce::TextButton& button)
    {
        addAndMakeVisible(button);
        button.setLookAndFeel(&fxmeLookAndFeel);
    };

    setupLinkButton(linkAllButton);
    linkAllButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    linkAllButton.onClick = [this] {
        for (auto& step : stepComponents) step->linkButton.setToggleState(true, juce::dontSendNotification);
    };

    setupLinkButton(linkNoneButton);
    linkNoneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    linkNoneButton.onClick = [this] {
        for (auto& step : stepComponents) step->linkButton.setToggleState(false, juce::dontSendNotification);
    };

    setupLinkButton(linkInvertButton);
    linkInvertButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    linkInvertButton.onClick = [this] {
        for (auto& step : stepComponents) step->linkButton.setToggleState(!step->linkButton.getToggleState(), juce::dontSendNotification);
    };

    // Create step components for each channel FIRST
    for (int i = 0; i < RhythmicGateAudioProcessor::NUM_STEPS; ++i)
    {
        stepComponents[i] = std::make_unique<StepComponent>(audioProcessor, i, fxmeLookAndFeel);
        addAndMakeVisible(*stepComponents[i]);
    }

    // --- Configure Master Knobs ---
    // Manually set their ranges and default values to match the step parameters
    masterDurationKnob.slider.setRange(0.0, 1.0, 0.01);
    masterDurationKnob.slider.setValue(1.0, juce::dontSendNotification);

    masterLevelMeter.slider.setRange(-60.0, 6.0, 0.1);
    masterLevelMeter.slider.setValue(0.0, juce::dontSendNotification);

    masterPanKnob.slider.setRange(-1.0, 1.0, 0.01);
    masterPanKnob.slider.setValue(0.0, juce::dontSendNotification);
    masterPanKnob.slider.getProperties().set ("drawFromCentre", true);

    masterAuxSendMeter.slider.setRange(-60.0, 6.0, 0.1);
    masterAuxSendMeter.slider.setValue(-60.0, juce::dontSendNotification);

    // --- Setup Row Labels ---
    auto setupLabel = [this] (juce::Label& label)
    {
        label.setFont (juce::Font (12.0f));
        label.setJustificationType (juce::Justification::centredRight);
        label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (label);
    };
    setupLabel(durationLabel);
    setupLabel(panLabel);
    setupLabel(levelLabel);
    setupLabel(auxLabel);
    setupLabel(linkLabel);

    // --- Master Knobs Setup ---
    auto setupMasterMeter = [this] (FxmeLevelMeter& masterMeter, auto stepMeterMember)
    {
        addAndMakeVisible(masterMeter);
        masterMeter.setLookAndFeel(&fxmeLookAndFeel);
        masterMeter.slider.onValueChange = [this, &masterMeter, stepMeterMember]
        {
            double masterValue = masterMeter.slider.getValue();
            for (auto& stepComp : stepComponents)
            {
                if (stepComp->linkButton.getToggleState())
                {
                    auto& stepSlider = (stepComp.get()->*stepMeterMember).slider;
                    stepSlider.setValue(masterValue);
                }
            }
        };
    };

    setupMasterMeter(masterDurationKnob, &StepComponent::durationKnob);
    setupMasterMeter(masterPanKnob,      &StepComponent::panKnob);
    setupMasterMeter(masterLevelMeter, &StepComponent::levelMeter);
    setupMasterMeter(masterAuxSendMeter, &StepComponent::auxSendMeter);

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

    // Vertical box for controls on the left
    juce::FlexBox leftPanel;
    leftPanel.flexDirection = juce::FlexBox::Direction::column;
    leftPanel.items.add(juce::FlexItem(logo).withFlex(1.f));
    leftPanel.items.add(juce::FlexItem(metricSelector).withFlex(.3f).withMargin(juce::FlexItem::Margin(5.f, 2.f, 5.f, 2.f)));
    leftPanel.items.add(juce::FlexItem(stepsSelector).withFlex(.3f).withMargin(juce::FlexItem::Margin(2.f, 2.f, 5.f, 2.f)));
    leftPanel.items.add(juce::FlexItem(attackKnob).withFlex(1.0f).withMargin(juce::FlexItem::Margin(5.0f, 2, 2, 2)));
    leftPanel.items.add(juce::FlexItem(releaseKnob).withFlex(1.0f).withMargin(juce::FlexItem::Margin(5.0f, 2, 2.0f, 2)));
    //leftPanel.items.add(juce::FlexItem().withFlex(0.4f)); // Spacer

    // Vertical box for the new labels
    juce::FlexBox labelPanel;
    labelPanel.flexDirection = juce::FlexBox::Direction::column;
    labelPanel.items.add(juce::FlexItem().withHeight(20.0f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2))); // Spacer for On/Off
    labelPanel.items.add(juce::FlexItem(durationLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(panLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(levelLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(auxLabel).withFlex(1.0f).withMargin(2));
    labelPanel.items.add(juce::FlexItem(linkLabel).withHeight(20.0f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2)));

    // Horizontal box for the link control buttons, now on the right side
    juce::FlexBox linkButtonsBox;
    linkButtonsBox.flexDirection = juce::FlexBox::Direction::row;
    linkButtonsBox.items.add(juce::FlexItem(linkAllButton).withFlex(1.0f));
    linkButtonsBox.items.add(juce::FlexItem(linkNoneButton).withFlex(1.0f));
    linkButtonsBox.items.add(juce::FlexItem(linkInvertButton).withFlex(1.0f));

    // Vertical box for labels on the right
    juce::FlexBox rightPanel;
    rightPanel.flexDirection = juce::FlexBox::Direction::column;
    rightPanel.items.add(juce::FlexItem().withFlex(0.5f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2))); // Spacer for On/Off row
    rightPanel.items.add(juce::FlexItem(masterDurationKnob).withFlex(1.0f).withMargin(2));
    rightPanel.items.add(juce::FlexItem(masterPanKnob).withFlex(1.0f).withMargin(2));
    rightPanel.items.add(juce::FlexItem(masterLevelMeter).withFlex(1.0f).withMargin(2));
    rightPanel.items.add(juce::FlexItem(masterAuxSendMeter).withFlex(1.0f).withMargin(2));
    rightPanel.items.add(juce::FlexItem(linkButtonsBox).withFlex(0.5f).withMargin(juce::FlexItem::Margin(2, 2, 2, 2)));


    // Add panels and sequencer to the main layout
    mainLayout.items.add(juce::FlexItem(leftPanel).withFlex(1.5f).withMargin(juce::FlexItem::Margin(0.f, 5.f, 0.f, 0.f)));
    mainLayout.items.add(juce::FlexItem(sequencerRow).withFlex(16.0f));
    mainLayout.items.add(juce::FlexItem(labelPanel).withFlex(1.f));
    mainLayout.items.add(juce::FlexItem(rightPanel).withFlex(1.f));

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
    // ComboBox IDs are 1-based. Ternary metrics have even IDs (2, 4, 6).
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
