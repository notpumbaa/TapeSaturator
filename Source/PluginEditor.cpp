#include "PluginEditor.h"

namespace
{
    const juce::Identifier uiScaleId { "uiScale" };
}

TapeSaturatorAudioProcessorEditor::TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), machine (p.apvts)
{
    addAndMakeVisible (machine);

    const int bw = TapeMachine::kBaseWidth, bh = TapeMachine::kBaseHeight;

    // Gespeicherte Größe lesen, bevor setResizeLimits() ein resized() auslöst
    const float savedScale = juce::jlimit (minScale, maxScale,
                                           (float) (double) processor.apvts.state.getProperty (uiScaleId, 1.0));
    setResizable (true, true);
    setResizeLimits ((int) (bw * minScale), (int) (bh * minScale), (int) (bw * maxScale), (int) (bh * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) bw / (double) bh);

    setSize (juce::roundToInt (bw * savedScale), juce::roundToInt (bh * savedScale));
    sizeRestored = true;

    startTimerHz (timerHz);
}

TapeSaturatorAudioProcessorEditor::~TapeSaturatorAudioProcessorEditor()
{
    stopTimer();
}

void TapeSaturatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TapeSaturatorAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) TapeMachine::kBaseWidth;
    machine.setBounds (0, 0, TapeMachine::kBaseWidth, TapeMachine::kBaseHeight);
    machine.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored)
        processor.apvts.state.setProperty (uiScaleId, scale, nullptr);
}

void TapeSaturatorAudioProcessorEditor::timerCallback()
{
    // Spulen laufen nur, wenn der Host tatsächlich Audio verarbeitet
    const auto count = processor.getProcessedBlockCount();
    idleTicks = (count != lastBlockCount) ? 0 : idleTicks + 1;
    lastBlockCount = count;

    machine.tick (1.0f / (float) timerHz, idleTicks < timerHz / 3,
                  processor.getInputLevel(), processor.getOutputLevel());
}
