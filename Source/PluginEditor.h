#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/TapeMachine.h"

/**
    Editor: hält die TapeMachine-Oberfläche und skaliert sie stufenlos (60–200 %).
    Die gewählte Größe wird im Plugin-State gespeichert.
*/
class TapeSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor&);
    ~TapeSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Für Screenshots/Tests. */
    TapeMachine& getMachine() noexcept { return machine; }

private:
    void timerCallback() override;

    TapeSaturatorAudioProcessor& tapeProcessor;
    TapeMachine machine;

    juce::uint32 lastBlockCount = 0;
    int idleTicks = 0;
    bool sizeRestored = false;

    static constexpr int timerHz = 30;
    static constexpr float minScale = 0.6f, maxScale = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeSaturatorAudioProcessorEditor)
};
