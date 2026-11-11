#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    inline constexpr const char* input       = "input";
    inline constexpr const char* saturation  = "saturation";
    inline constexpr const char* bias        = "bias";
    inline constexpr const char* speed       = "speed";
    inline constexpr const char* flutter     = "flutter";
    inline constexpr const char* noise       = "noise";
    inline constexpr const char* delayTime   = "delayTime";
    inline constexpr const char* delayFb     = "delayFeedback";
    inline constexpr const char* delayMix    = "delayMix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* autoGain    = "autoGain";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
