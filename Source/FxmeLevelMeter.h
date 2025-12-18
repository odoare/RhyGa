/*
  ==============================================================================

    FxmeLevelMeter
    Created: 18 Dec 2025 9:50:25pm
    Author:  doare

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>


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
                   juce::Slider::SliderStyle style = juce::Slider::LinearBarVertical) :
        apvtsRef(apvts), parameterID(paramName)
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

    void setDrawFromCentre(bool shouldDrawFromCentre)
    {
        slider.getProperties().set("drawFromCentre", shouldDrawFromCentre);
    }

    juce::RangedAudioParameter* getParameter() const
    {
        return apvtsRef.getParameter(parameterID);
    }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::String parameterID;
};
