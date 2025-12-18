/*
  ==============================================================================

    PluginEditor.h
    Created: 01 Nov 2025 9:46:12pm
    Author:  doare

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FxmeLogo.h"
#include "StepComponent.h"

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
    void randomizeParameters();

private:
    RhythmicGateAudioProcessor& audioProcessor;
    
    juce::ComboBox metricSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> metricAttachment;

    juce::ComboBox stepsSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stepsAttachment;

    fxme::FxmeKnob attackKnob;
    fxme::FxmeKnob releaseKnob;

    std::array<std::unique_ptr<StepComponent>, RhythmicGateAudioProcessor::NUM_STEPS> stepComponents;

    // Link control buttons
    juce::TextButton linkAllButton    { "1" };
    juce::TextButton linkNoneButton   { "0" };
    juce::TextButton linkInvertButton { "/" };

    // Labels for the master control rows
    juce::Label onOffLabel { {}, "On/Off" };
    juce::Label durationLabel { {}, "Duration" };
    juce::Label panLabel      { {}, "Pan" };
    juce::Label levelLabel    { {}, "Level" };
    juce::Label auxLabel      { {}, "Aux" };
    juce::Label linkLabel     { {}, "Link" };

    fxme::FxmeLookAndFeel fxmeLookAndFeel;

    int lastActiveStep = -1;
    int lastNumSteps = -1;

    FxmeLogo logo{"",false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmicGateAudioProcessorEditor)
};