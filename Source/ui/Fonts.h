#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Eingebettete Schriften (SIL Open Font License). */
namespace TapeFonts
{
    /** Barlow Condensed SemiBold – Beschriftungen, Sektionstitel. */
    juce::FontOptions label (float height);

    /** Barlow Condensed Medium – Skalenzahlen, kleine Texte. */
    juce::FontOptions scale (float height);

    /** Playfair Display SC Black – Typenschild. */
    juce::FontOptions emblem (float height);
}
