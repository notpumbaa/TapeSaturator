#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace TapeColours
{
    inline const juce::Colour machine    { 0xff1c1d1f }; // Laufwerk (oben)
    inline const juce::Colour machineHi  { 0xff2c2e31 };
    inline const juce::Colour engraving  { 0xff23262a }; // Gravur auf Metall
    inline const juce::Colour engraveHi  { 0x66ffffff };
    inline const juce::Colour cream      { 0xfff1e6cc };
    inline const juce::Colour amber      { 0xffe8a33d };
    inline const juce::Colour lampOn     { 0xffffb238 };
    inline const juce::Colour lampOff    { 0xff5a3a1a };
    inline const juce::Colour bakelite   { 0xff141414 };
    inline const juce::Colour tape       { 0xff3b2a1d };
}

/**
    Vintage-Studio-Optik:
      - Drehregler als "Chicken Head"-Bakelitknöpfe mit gravierter Skala
      - ToggleButtons als verchromte Kippschalter (Property "switchLabels" = "OBEN|UNTEN")
*/
class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapeLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    /** Gravierte Schrift: heller Versatz unten + dunkle Schrift. */
    static void drawEngravedText (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
                                  juce::Justification, float fontSize, bool bold = true);

    /** Gebürstete Aluminiumfläche (einmal erzeugen und cachen). */
    static juce::Image createBrushedMetal (int width, int height, float scale);

    /** Kreuzschlitz-Schraube. */
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float rotation);
};
