#include "PluginProcessor.h"
#include "PluginEditor.h"

TapeSaturatorAudioProcessor::TapeSaturatorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TapeSaturatorState", createParameterLayout())
{
    pInput      = apvts.getRawParameterValue (ParamIDs::input);
    pSaturation = apvts.getRawParameterValue (ParamIDs::saturation);
    pBias       = apvts.getRawParameterValue (ParamIDs::bias);
    pSpeed      = apvts.getRawParameterValue (ParamIDs::speed);
    pFlutter    = apvts.getRawParameterValue (ParamIDs::flutter);
    pNoise      = apvts.getRawParameterValue (ParamIDs::noise);
    pDelayTime  = apvts.getRawParameterValue (ParamIDs::delayTime);
    pDelayFb    = apvts.getRawParameterValue (ParamIDs::delayFb);
    pDelayMix   = apvts.getRawParameterValue (ParamIDs::delayMix);
    pOutput     = apvts.getRawParameterValue (ParamIDs::output);
    pMix        = apvts.getRawParameterValue (ParamIDs::mix);
    pAutoGain   = apvts.getRawParameterValue (ParamIDs::autoGain);
    pAir        = apvts.getRawParameterValue (ParamIDs::air);
}

TapeParams TapeSaturatorAudioProcessor::readParams() const
{
    TapeParams p;
    p.inputDb       = pInput->load();
    p.saturation    = pSaturation->load();
    p.bias          = pBias->load();
    p.speed         = juce::roundToInt (pSpeed->load());
    p.flutter       = pFlutter->load();
    p.noise         = pNoise->load();
    p.delayMs       = pDelayTime->load();
    p.delayFeedback = pDelayFb->load();
    p.delayMix      = pDelayMix->load();
    p.outputDb      = pOutput->load();
    p.mix           = pMix->load();
    p.autoGain      = pAutoGain->load() > 0.5f;
    p.air           = pAir->load();
    return p;
}

void TapeSaturatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    inputMeanSq = 0.0f;
    outputMeanSq = 0.0f;
    engine.setParameters (readParams());
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    setLatencySamples (engine.getLatencySamples());
}

bool TapeSaturatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void TapeSaturatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setParameters (readParams());
    engine.process (buffer);

    // Input-VU: Pegel nach dem INPUT-Regler (was aufs Band geht), Output-VU: Plugin-Ausgang
    smoothMeter (inputMeanSq, engine.getTapeInputMeanSquare(), buffer.getNumSamples());
    smoothMeter (outputMeanSq, meanSquare (buffer), buffer.getNumSamples());
    blockCounter.fetch_add (1, std::memory_order_relaxed);
}

float TapeSaturatorAudioProcessor::meanSquare (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    if (n == 0 || chans == 0)
        return 0.0f;

    double sum = 0.0;
    for (int ch = 0; ch < chans; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            sum += (double) d[i] * d[i];
    }
    return (float) (sum / (double) (n * chans));
}

void TapeSaturatorAudioProcessor::smoothMeter (std::atomic<float>& meanSq, float blockMeanSq, int n)
{
    if (n <= 0)
        return;

    // Glättung mit ~50 ms Zeitkonstante, unabhängig von der Blockgröße
    const float coef = 1.0f - std::exp (-(float) n / (float) (0.05 * currentSampleRate));
    const float prev = meanSq.load (std::memory_order_relaxed);
    const float next = prev + coef * (blockMeanSq - prev);
    meanSq.store (std::isfinite (next) ? next : 0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TapeSaturatorAudioProcessor::createEditor()
{
    return new TapeSaturatorAudioProcessorEditor (*this);
}

void TapeSaturatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeSaturatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeSaturatorAudioProcessor();
}
