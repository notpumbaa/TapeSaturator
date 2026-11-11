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
    return p;
}

void TapeSaturatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
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
