#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/TapeLookAndFeel.h"
#include "ui/TapeReel.h"
#include "ui/VUMeter.h"

class TapeSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor&);
    ~TapeSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String caption;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Switch
    {
        juce::ToggleButton button;
        juce::String caption;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void addKnob (Knob&, const char* paramID, const juce::String& caption);
    void addSwitch (Switch&, const char* paramID, const juce::String& caption, const juce::String& labels);
    void paintTransport (juce::Graphics&, juce::Rectangle<float> area);
    void paintFaceplate (juce::Graphics&, juce::Rectangle<float> area);

    TapeSaturatorAudioProcessor& processor;
    TapeLookAndFeel lnf;

    Knob input, saturation, bias, flutter, noise, delayTime, delayFb, delayMix, output, mix;
    Switch speedSwitch, autoGainSwitch;

    TapeReel leftReel { 0.86f }, rightReel { 0.55f };
    VUMeter inputVU { "INPUT" }, outputVU { "OUTPUT" };

    std::array<Section, 4> sections;
    std::vector<std::pair<juce::Component*, juce::String>> captions;

    juce::Rectangle<int> transportArea, plateArea;
    juce::Rectangle<float> lampBounds;
    juce::Image brushedMetal;

    juce::uint32 lastBlockCount = 0;
    int idleTicks = 0;
    float reelAngle = 0.0f;
    bool lampOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeSaturatorAudioProcessorEditor)
};
