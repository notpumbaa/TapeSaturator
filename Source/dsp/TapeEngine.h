#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Hysteresis.h"

struct TapeParams
{
    float inputDb       = 0.0f;   // -18..+18
    float saturation    = 0.5f;   // 0..1
    float bias          = 0.5f;   // 0..1 (0.5 = nominal)
    int   speed         = 1;      // 0 = 7.5 ips, 1 = 15 ips
    float flutter       = 0.2f;   // 0..1
    float noise         = 0.0f;   // 0..1
    float delayMs       = 120.0f; // 30..800
    float delayFeedback = 0.2f;   // 0..0.9
    float delayMix      = 0.0f;   // 0..1
    float outputDb      = 0.0f;   // -18..+18
    float mix           = 1.0f;   // 0..1
    bool  autoGain      = true;
};

/**
    Komplette Bandmaschinen-Signalkette:

    Input -> Record-EQ (Pre-Emphasis) -> [4x Oversampling: Hysterese] -> Playback-EQ (De-Emphasis)
          -> Speed-EQ (Head Bump, HF-Rolloff, Bias-Tilt) -> Wow & Flutter -> Slap-Delay
          -> Bandrauschen -> Output / Auto Gain -> Dry/Wet
*/
class TapeEngine
{
public:
    TapeEngine();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParameters (const TapeParams& p);
    void process (juce::AudioBuffer<float>& buffer);

    /** Gesamtlatenz in Samples (Oversampling-Filter + Wow/Flutter-Grundverzögerung). */
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    using Filter     = juce::dsp::IIR::Filter<float>;
    using Coeffs     = juce::dsp::IIR::Coefficients<float>;
    using StereoIIR  = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    void updateFilters();
    void processTapeMotion (juce::dsp::AudioBlock<float>& block);
    void processDelay (juce::dsp::AudioBlock<float>& block);
    void addNoise (juce::dsp::AudioBlock<float>& block);

    static constexpr int oversamplingOrder = 2; // 2^2 = 4x

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;
    int latencySamples = 0;
    bool prepared = false;

    TapeParams params;
    int lastSpeed = -1;
    float lastBias = -1.0f;

    // Hysterese (oversampled)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<Hysteresis> hysteresis;
    juce::SmoothedValue<float> makeupGain;

    // EQs
    StereoIIR preEmphasis, deEmphasis, lowCut, headBump, bumpDip, hfRolloff, biasTilt;

    // Wow & Flutter
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> motionDelay { 1 << 12 };
    double wowPhase = 0.0, flutterPhase = 0.0, flutter2Phase = 0.0;
    float driftState = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    float motionBaseDelay = 0.0f;
    juce::SmoothedValue<float> flutterAmount;

    // Slap-Delay
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echoDelay { 1 << 16 };
    juce::SmoothedValue<float> echoTimeSamples, echoFeedback, echoMix;
    std::vector<float> echoLp, echoHp;
    float echoLpCoef = 0.5f, echoHpCoef = 0.99f;

    // Rauschen
    juce::Random random;
    std::vector<float> noiseLp;
    juce::SmoothedValue<float> noiseGain;

    // Gain / Mix
    juce::SmoothedValue<float> inputGain, outputGain;
    juce::dsp::DryWetMixer<float> dryWet { 8192 };
};
