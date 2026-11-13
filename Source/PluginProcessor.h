#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/TapeEngine.h"

class TapeSaturatorAudioProcessor : public juce::AudioProcessor
{
public:
    TapeSaturatorAudioProcessor();
    ~TapeSaturatorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TapeSaturator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Aktuelle Parameter als DSP-Struktur (auch für Tests nützlich). */
    TapeParams readParams() const;

    /** Geglätteter RMS-Pegel (linear) für die VU-Meter.
        Input = Pegel nach dem INPUT-Regler (was aufs Band geht), Output = Plugin-Ausgang. */
    float getInputLevel() const noexcept  { return std::sqrt (inputMeanSq.load()); }
    float getOutputLevel() const noexcept { return std::sqrt (outputMeanSq.load()); }

    /** Zählt verarbeitete Blöcke – die UI lässt die Spulen nur bei laufendem Audio drehen. */
    juce::uint32 getProcessedBlockCount() const noexcept { return blockCounter.load(); }

private:
    TapeEngine engine;

    static float meanSquare (const juce::AudioBuffer<float>& buffer);
    void smoothMeter (std::atomic<float>& meanSq, float blockMeanSq, int numSamples);

    std::atomic<float> inputMeanSq { 0.0f }, outputMeanSq { 0.0f };
    std::atomic<juce::uint32> blockCounter { 0 };
    double currentSampleRate = 44100.0;

    std::atomic<float>* pInput      = nullptr;
    std::atomic<float>* pSaturation = nullptr;
    std::atomic<float>* pBias       = nullptr;
    std::atomic<float>* pSpeed      = nullptr;
    std::atomic<float>* pFlutter    = nullptr;
    std::atomic<float>* pNoise      = nullptr;
    std::atomic<float>* pDelayTime  = nullptr;
    std::atomic<float>* pDelayFb    = nullptr;
    std::atomic<float>* pDelayMix   = nullptr;
    std::atomic<float>* pOutput     = nullptr;
    std::atomic<float>* pMix        = nullptr;
    std::atomic<float>* pAutoGain   = nullptr;
    std::atomic<float>* pAir        = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeSaturatorAudioProcessor)
};
