#include "Fonts.h"
#include "BinaryData.h"

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    const juce::Typeface::Ptr& labelFace()
    {
        static const auto tf = load (TapeAssets::BarlowCondensedSemiBold_ttf, TapeAssets::BarlowCondensedSemiBold_ttfSize);
        return tf;
    }

    const juce::Typeface::Ptr& scaleFace()
    {
        static const auto tf = load (TapeAssets::BarlowCondensedMedium_ttf, TapeAssets::BarlowCondensedMedium_ttfSize);
        return tf;
    }

    const juce::Typeface::Ptr& emblemFace()
    {
        static const auto tf = load (TapeAssets::PlayfairDisplaySCBlack_ttf, TapeAssets::PlayfairDisplaySCBlack_ttfSize);
        return tf;
    }
}

namespace TapeFonts
{
    juce::FontOptions label (float height)  { return juce::FontOptions (labelFace()).withHeight (height); }
    juce::FontOptions scale (float height)  { return juce::FontOptions (scaleFace()).withHeight (height); }
    juce::FontOptions emblem (float height) { return juce::FontOptions (emblemFace()).withHeight (height); }
}
