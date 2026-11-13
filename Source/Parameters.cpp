#include "Parameters.h"

using APF  = juce::AudioParameterFloat;
using APC  = juce::AudioParameterChoice;
using APB  = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;

namespace
{
    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    juce::String decibel (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String millis  (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }

    std::unique_ptr<APF> percentParam (const char* id, const juce::String& name, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name,
                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
                                      Attr().withStringFromValueFunction (percent));
    }

    std::unique_ptr<APF> gainParam (const char* id, const juce::String& name)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name,
                                      juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                                      Attr().withStringFromValueFunction (decibel).withLabel ("dB"));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Tape
    layout.add (gainParam (ParamIDs::input, "Input"));
    layout.add (percentParam (ParamIDs::saturation, "Saturation", 0.5f));
    layout.add (percentParam (ParamIDs::bias, "Bias", 0.5f));
    layout.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::speed, 1 }, "Speed",
                                       juce::StringArray { "7.5 ips", "15 ips" }, 1));

    // Transport
    layout.add (percentParam (ParamIDs::flutter, "Wow & Flutter", 0.2f));
    layout.add (percentParam (ParamIDs::noise, "Noise", 0.0f));

    // Slap delay
    {
        juce::NormalisableRange<float> range (30.0f, 800.0f, 1.0f);
        range.setSkewForCentre (150.0f);
        layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::delayTime, 1 }, "Delay Time",
                                           range, 120.0f,
                                           Attr().withStringFromValueFunction (millis).withLabel ("ms")));
    }
    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::delayFb, 1 }, "Delay Feedback",
                                       juce::NormalisableRange<float> (0.0f, 0.9f, 0.001f), 0.2f,
                                       Attr().withStringFromValueFunction (percent)));
    layout.add (percentParam (ParamIDs::delayMix, "Delay Mix", 0.0f));

    // Output
    layout.add (percentParam (ParamIDs::air, "Air", 0.0f));
    layout.add (gainParam (ParamIDs::output, "Output"));
    layout.add (percentParam (ParamIDs::mix, "Dry/Wet", 1.0f));
    layout.add (std::make_unique<APB> (juce::ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", true));

    return layout;
}
