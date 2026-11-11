#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/TapeLookAndFeel.h"

class TapeSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor&);
    ~TapeSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void addKnob (Knob&, const char* paramID, const juce::String& name);

    TapeSaturatorAudioProcessor& processor;
    TapeLookAndFeel lnf;

    Knob input, saturation, bias, flutter, noise, delayTime, delayFb, delayMix, output, mix;

    juce::ComboBox speedBox;
    juce::Label speedLabel;
    std::unique_ptr<ComboAttachment> speedAttachment;

    juce::ToggleButton autoGainButton { "AUTO GAIN" };
    std::unique_ptr<ButtonAttachment> autoGainAttachment;

    std::array<Section, 4> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeSaturatorAudioProcessorEditor)
};
