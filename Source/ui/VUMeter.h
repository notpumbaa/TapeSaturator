#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Analoges VU-Meter mit Zeiger, beleuchteter Skala, Glasreflex und VU-artiger Ballistik
    (Feder-Masse-System, ~300 ms Anstiegszeit, leichtes Überschwingen).
    0 VU = -18 dBFS RMS.
*/
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String caption);

    /** Neuen RMS-Pegel (linear) setzen und Zeiger um dt Sekunden weiterbewegen.
        jitter: 0..1, leichtes mechanisches Zittern der Nadel (z. B. aus Wow/Flutter). */
    void update (float rmsLinear, float dtSeconds, float jitter = 0.0f);

    void paint (juce::Graphics&) override;

    /** Für Screenshots/Tests: Zeiger direkt setzen (0..1). */
    void setNeedlePosition (float pos) { needlePos = pos; needleVel = 0.0f; repaint(); }

private:
    static float vuToPosition (float vu);
    void renderFace (float pixelScale);
    void renderGlass (float pixelScale);

    juce::String caption;
    float needlePos = 0.0f;   // 0..1
    float needleVel = 0.0f;
    juce::Random rng;

    juce::Image faceLayer, glassLayer;   // gecacht in Pixelauflösung
    float cachedScale = 0.0f;
    juce::Rectangle<int> cachedBounds;
};
