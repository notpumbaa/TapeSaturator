#include "PluginEditor.h"

namespace
{
    const juce::Identifier uiScaleId { "uiScale" };
}

TapeSaturatorAudioProcessorEditor::TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor& p)
    : AudioProcessorEditor (&p), tapeProcessor (p), machine (p.apvts)
{
    addAndMakeVisible (machine);

    const int bw = TapeMachine::kBaseWidth, bh = TapeMachine::kBaseHeight;

    // Gespeicherte Größe lesen, bevor setResizeLimits() ein resized() auslöst
    const float savedScale = juce::jlimit (minScale, maxScale,
                                           (float) (double) tapeProcessor.apvts.state.getProperty (uiScaleId, 1.0));
    setResizable (true, true);
    setResizeLimits ((int) ((float) bw * minScale), (int) ((float) bh * minScale), (int) ((float) bw * maxScale), (int) ((float) bh * maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) bw / (double) bh);

    setSize (juce::roundToInt ((float) bw * savedScale), juce::roundToInt ((float) bh * savedScale));
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
        tapeProcessor.apvts.state.setProperty (uiScaleId, scale, nullptr);
}

void TapeSaturatorAudioProcessorEditor::timerCallback()
{
    // Spulen laufen nur, wenn der Host tatsächlich Audio verarbeitet
    const auto count = tapeProcessor.getProcessedBlockCount();
    idleTicks = (count != lastBlockCount) ? 0 : idleTicks + 1;
    lastBlockCount = count;

    machine.tick (1.0f / (float) timerHz, idleTicks < timerHz / 3,
                  tapeProcessor.getInputLevel(), tapeProcessor.getOutputLevel());
}
