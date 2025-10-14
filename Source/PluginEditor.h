#pragma once
#include <FxmeJuceTools/Components/FxmeKnob.h>
#include <FxmeJuceTools/Components/FxmeButton.h>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FxmeLogo.h"

//==============================================================================
/** A vertical meter component that acts like a fader.
    Draws a filled rectangle from the bottom up to represent the current value.
    The value can be changed by clicking or dragging vertically.
*/
class FxmeLevelMeter : public juce::Component
{
public:
    FxmeLevelMeter(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramName,
                   juce::Colour meterColour = juce::Colours::white,
                   juce::Slider::SliderStyle style = juce::Slider::LinearBarVertical)
    {
        // Use an internal slider to manage the parameter and drawing
        slider.setSliderStyle(style);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour(juce::Slider::trackColourId, meterColour);
        addAndMakeVisible(slider);

        // Only create an attachment if the parameter exists
        if (apvts.getParameter(paramName) != nullptr)
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramName, slider);
    }
    
    // No longer needed, the LookAndFeel will handle drawing.
    // void paint(juce::Graphics& g) override {}

    void resized() override
    {
        slider.setBounds(getLocalBounds());
    }

    void setLookAndFeel(juce::LookAndFeel* newLookAndFeel)
    {
        slider.setLookAndFeel(newLookAndFeel);
    }

    juce::Slider slider; // Public to allow access from master controls.

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A simple component to group the 4 controls for a single step
class StepComponent : public juce::Component
{
public:
    StepComponent(RhythmicGateAudioProcessor& p, int step, juce::LookAndFeel_V4& lookAndFeel);
    void resized() override;
    void paint(juce::Graphics& g) override;

    void setActive(bool isActive);
    void setAccented(bool shouldBeAccented);
    
    FxmeButton onOffButton; // Assuming FxmeButton is in the global namespace as per original structure
    FxmeLevelMeter durationKnob;
    FxmeLevelMeter panKnob;
    FxmeLevelMeter levelMeter;
    FxmeLevelMeter auxSendMeter;
    juce::ToggleButton linkButton;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    bool active = false;
    bool isAccented = false;
};

//==============================================================================
class RhythmicGateAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           public juce::Timer
{
public:
    RhythmicGateAudioProcessorEditor (RhythmicGateAudioProcessor&);
    ~RhythmicGateAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    // Timer callback
    void timerCallback() override;

    // UI update function
    void updateStepComponentVisibility();
    void updateStepAccents();

private:
    RhythmicGateAudioProcessor& audioProcessor;
    
    juce::ComboBox metricSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> metricAttachment;

    juce::ComboBox stepsSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stepsAttachment;

    fxme::FxmeKnob attackKnob;
    fxme::FxmeKnob releaseKnob;

    // Master knobs for controlling rows
    FxmeLevelMeter masterDurationKnob;
    FxmeLevelMeter masterPanKnob;
    FxmeLevelMeter masterLevelMeter;
    FxmeLevelMeter masterAuxSendMeter;

    std::array<std::unique_ptr<StepComponent>, RhythmicGateAudioProcessor::NUM_STEPS> stepComponents;

    // Labels for the master control rows
    juce::Label durationLabel { {}, "Duration" };
    juce::Label panLabel      { {}, "Pan" };
    juce::Label levelLabel    { {}, "Level" };
    juce::Label auxLabel      { {}, "Aux" };
    juce::Label linkLabel     { {}, "Link" };

    // Link control buttons
    juce::TextButton linkAllButton    { "1" };
    juce::TextButton linkNoneButton   { "0" };
    juce::TextButton linkInvertButton { "/" };

    fxme::FxmeLookAndFeel fxmeLookAndFeel;

    int lastActiveStep = -1;
    int lastNumSteps = -1;

    FxmeLogo logo{"",false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmicGateAudioProcessorEditor)
};