#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Analoges VU-Meter mit Zeiger, beleuchteter Skala und VU-artiger Ballistik
    (Feder-Masse-System, ~300 ms Anstiegszeit, leichtes Überschwingen).
    0 VU = -18 dBFS RMS.
*/
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String caption);

    /** Neuen RMS-Pegel (linear) setzen und Zeiger um dt Sekunden weiterbewegen. */
    void update (float rmsLinear, float dtSeconds);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static float vuToPosition (float vu);
    void renderStaticLayer();

    juce::String caption;
    float needlePos = 0.0f;   // 0..1
    float needleVel = 0.0f;
    juce::Image staticLayer;  // Gehäuse + Skala, wird nur bei Größenänderung neu gezeichnet
};
