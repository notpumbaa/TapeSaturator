#include "VUMeter.h"
#include "TapeLookAndFeel.h"

namespace
{
    constexpr float minVU = -20.0f;
    constexpr float maxVU = 3.0f;
    constexpr float sweep = 0.85f; // halber Zeigerausschlag in Radiant (~49°)
    constexpr float referenceDbfs = -18.0f;
}

VUMeter::VUMeter (juce::String c) : caption (std::move (c))
{
    setOpaque (false);
}

float VUMeter::vuToPosition (float vu)
{
    // Echte VU-Skalen sind spannungs-linear: -20 VU liegt fast ganz links, 0 VU bei ~70 %
    const float lo = std::pow (10.0f, minVU / 20.0f);
    const float hi = std::pow (10.0f, maxVU / 20.0f);
    const float v  = std::pow (10.0f, juce::jlimit (minVU - 6.0f, maxVU + 2.0f, vu) / 20.0f);
    return (v - lo) / (hi - lo);
}

void VUMeter::update (float rmsLinear, float dt)
{
    const float db = (rmsLinear > 1.0e-5f ? 20.0f * std::log10 (rmsLinear) : -100.0f) - referenceDbfs;
    const float target = juce::jlimit (-0.02f, 1.06f, vuToPosition (db));

    // Feder-Masse: ω ≈ 2π·2.1 Hz, Dämpfung ζ ≈ 0.8 → VU-typisches Verhalten
    constexpr float omega = 13.2f, zeta = 0.8f;
    const int steps = juce::jmax (1, (int) std::ceil (dt / 0.005f));
    const float h = dt / (float) steps;
    for (int i = 0; i < steps; ++i)
    {
        const float acc = omega * omega * (target - needlePos) - 2.0f * zeta * omega * needleVel;
        needleVel += acc * h;
        needlePos += needleVel * h;
    }
    needlePos = juce::jlimit (-0.03f, 1.08f, needlePos);
    repaint();
}

void VUMeter::resized()
{
    renderStaticLayer();
}

void VUMeter::renderStaticLayer()
{
    const float scale = 2.0f; // für Retina-Schärfe
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    staticLayer = juce::Image (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (staticLayer);
    g.addTransform (juce::AffineTransform::scale (scale));

    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);

    // Chrom-Rahmen
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8eaec), r.getX(), r.getY(),
                                             juce::Colour (0xff6e7174), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    auto bezel = r.reduced (5.0f);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (bezel, 4.0f);

    // Beleuchtete Skala (warmes Glühlampenlicht)
    auto face = bezel.reduced (3.0f);
    juce::ColourGradient glow (juce::Colour (0xfffbe3a4), face.getCentreX(), face.getBottom() - face.getHeight() * 0.15f,
                               juce::Colour (0xffd9a24a), face.getX(), face.getY(), true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (face, 3.0f);

    const float cx = face.getCentreX();
    const float pivotY = face.getBottom() + face.getHeight() * 0.18f;
    const float radius = face.getHeight() * 0.92f;

    auto angleFor = [] (float pos) { return -sweep + 2.0f * sweep * pos; };
    auto pointAt = [&] (float angle, float rad)
    {
        return juce::Point<float> (cx + rad * std::sin (angle), pivotY - rad * std::cos (angle));
    };

    // Roter Bereich 0..+3 VU
    {
        juce::Path red;
        const float a0 = angleFor (vuToPosition (0.0f)), a1 = angleFor (1.0f);
        red.addCentredArc (cx, pivotY, radius * 0.86f, radius * 0.86f, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xffb8261c));
        g.strokePath (red, juce::PathStrokeType (radius * 0.06f));
    }

    // Skalenbogen
    {
        juce::Path arc;
        arc.addCentredArc (cx, pivotY, radius * 0.82f, radius * 0.82f, 0.0f, angleFor (0.0f), angleFor (1.0f), true);
        g.setColour (juce::Colour (0xff2a2017));
        g.strokePath (arc, juce::PathStrokeType (1.2f));
    }

    // Skalenstriche + Zahlen
    const float fontSize = juce::jmax (8.0f, face.getHeight() * 0.11f);
    g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
    for (int vuInt : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const float vu = (float) vuInt;
        const float a = angleFor (vuToPosition (vu));
        const bool major = vuInt == -20 || vuInt == -10 || vuInt == -7 || vuInt == -5 || vuInt == -3 || vuInt == 0 || vuInt == 3;
        g.setColour (vuInt > 0 ? juce::Colour (0xffa81f16) : juce::Colour (0xff2a2017));
        g.drawLine (juce::Line<float> (pointAt (a, radius * 0.82f), pointAt (a, radius * (major ? 0.93f : 0.89f))), major ? 1.6f : 1.1f);

        const auto txtPos = pointAt (a, radius * 1.02f);
        const juce::String txt = vuInt > 0 ? "+" + juce::String (vuInt) : juce::String (std::abs (vuInt));
        g.drawText (txt, juce::Rectangle<float> (fontSize * 2.4f, fontSize * 1.2f).withCentre (txtPos),
                    juce::Justification::centred);
    }

    // "VU" und Beschriftung
    g.setColour (juce::Colour (0xff2a2017));
    g.setFont (juce::FontOptions (fontSize * 1.7f, juce::Font::bold));
    g.drawText ("VU", juce::Rectangle<float> (cx - 30.0f, face.getY() + face.getHeight() * 0.48f, 60.0f, fontSize * 1.8f),
                juce::Justification::centred);
    g.setFont (juce::FontOptions (fontSize * 0.95f));
    g.drawText (caption, juce::Rectangle<float> (face.getX(), face.getBottom() - fontSize * 1.9f, face.getWidth(), fontSize * 1.4f),
                juce::Justification::centred);
}

void VUMeter::paint (juce::Graphics& g)
{
    if (staticLayer.isValid())
        g.drawImage (staticLayer, getLocalBounds().toFloat());

    auto face = getLocalBounds().toFloat().reduced (8.0f);
    const float cx = face.getCentreX();
    const float pivotY = face.getBottom() + face.getHeight() * 0.18f;
    const float radius = face.getHeight() * 0.92f;
    const float angle = -sweep + 2.0f * sweep * needlePos;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (face.toNearestInt());

    // Zeiger mit Schatten
    const juce::Point<float> tip (cx + radius * 0.95f * std::sin (angle), pivotY - radius * 0.95f * std::cos (angle));
    const juce::Point<float> base (cx, pivotY);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (juce::Line<float> (base.translated (2.5f, 2.0f), tip.translated (2.5f, 2.0f)), 1.6f);
    g.setColour (juce::Colour (0xff141414));
    g.drawLine (juce::Line<float> (base, tip), 1.4f);

    // Abdeckung des Drehpunkts
    const float capH = face.getHeight() * 0.16f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), cx, face.getBottom() - capH,
                                             juce::Colour (0xff111111), cx, face.getBottom(), false));
    g.fillRect (face.withTop (face.getBottom() - capH * 0.55f));
}
