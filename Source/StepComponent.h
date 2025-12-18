/*
  ==============================================================================

    StepComponent.h
    Created: 18 Dec 2025 9:46:12pm
    Author:  doare

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FxmeLevelMeter.h"

// A simple component to group the 4 controls for a single step
class StepComponent : public juce::Component
{
public:
    StepComponent(juce::AudioProcessorValueTreeState& apvts, int step, juce::LookAndFeel_V4& lookAndFeel);
    void resized() override;
    void paint(juce::Graphics& g) override;

    void setActive(bool isActive);
    void setAccented(bool shouldBeAccented);
    
    FxmeLevelMeter durationSlider;
    FxmeLevelMeter panSlider;
    FxmeLevelMeter levelMeter;
    FxmeLevelMeter auxSendMeter;
    fxme::FxmeButton linkButton;
    fxme::FxmeButton onOffButton;

    const int stepIndex; // To store the step number for parameter access

private:
    bool active = false;
    bool isAccented = false;
};
