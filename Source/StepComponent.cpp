/*
  ==============================================================================

    StepComponent.cpp
    Created: 18 Dec 2025 9:46:12pm
    Author:  doare

  ==============================================================================
*/

#include "StepComponent.h"

StepComponent::StepComponent(juce::AudioProcessorValueTreeState& apvts, int step, juce::LookAndFeel_V4& lookAndFeel) :
    stepIndex(step),
    onOffButton(apvts, ParameterID::get(step, "ON"), "", juce::Colours::cyan),
    durationSlider(apvts, ParameterID::get(step, "DUR"), juce::Colours::magenta.darker(1.2f), juce::Slider::LinearHorizontal),
    panSlider(apvts, ParameterID::get(step, "PAN"), juce::Colours::orange.darker(), juce::Slider::LinearHorizontal),
    levelMeter(apvts, ParameterID::get(step, "LVL"), juce::Colours::green),
    auxSendMeter(apvts, ParameterID::get(step, "AUX_LVL"), juce::Colours::cornflowerblue),
    linkButton(apvts, ParameterID::get(step, "LINK"), "", juce::Colours::grey.darker())
{
    // On/Off Button
    addAndMakeVisible(onOffButton);
    onOffButton.setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(durationSlider);
    durationSlider.setLookAndFeel(&lookAndFeel);
    
    addAndMakeVisible(panSlider);
    panSlider.setLookAndFeel(&lookAndFeel);
    panSlider.setDrawFromCentre(true);

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
