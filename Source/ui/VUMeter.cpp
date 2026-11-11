#include "VUMeter.h"
#include "TapeLookAndFeel.h"
#include "Fonts.h"

namespace
{
    constexpr float minVU = -20.0f;
    constexpr float maxVU = 3.0f;
    constexpr float sweep = 0.85f; // halber Zeigerausschlag in Radiant (~49°)
    constexpr float referenceDbfs = -18.0f;

    struct Geometry
    {
        juce::Rectangle<float> outer, bezel, face;
        float cx, pivotY, radius;
    };

    Geometry geometry (juce::Rectangle<float> bounds)
    {
        Geometry g;
        g.outer  = bounds;
        g.bezel  = bounds.reduced (6.0f);
        g.face   = g.bezel.reduced (4.0f);
        g.cx     = g.face.getCentreX();
        g.pivotY = g.face.getBottom() + g.face.getHeight() * 0.18f;
        g.radius = g.face.getHeight() * 0.92f;
        return g;
    }

    float angleFor (float pos) { return -sweep + 2.0f * sweep * pos; }
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

void VUMeter::update (float rmsLinear, float dt, float jitter)
{
    const float db = (rmsLinear > 1.0e-5f ? 20.0f * std::log10 (rmsLinear) : -100.0f) - referenceDbfs;
    float target = juce::jlimit (-0.02f, 1.06f, vuToPosition (db));

    // leichtes mechanisches Zittern, nur wenn die Nadel überhaupt ausschlägt
    if (target > 0.02f && jitter > 0.0f)
        target += (rng.nextFloat() - 0.5f) * 0.012f * jitter;

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

void VUMeter::renderFace (float scale)
{
    const int w = getWidth(), h = getHeight();
    faceLayer = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) std::ceil ((float) w * scale)), juce::jmax (1, (int) std::ceil ((float) h * scale)), true);
    juce::Graphics g (faceLayer);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto geo = geometry (getLocalBounds().toFloat());

    // Chrom-Rahmen mit Fase
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f5f6), geo.outer.getX(), geo.outer.getY(),
                                             juce::Colour (0xff55595d), geo.outer.getRight(), geo.outer.getBottom(), false));
    g.fillRoundedRectangle (geo.outer, 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b6f73), geo.outer.getX(), geo.outer.getY(),
                                             juce::Colour (0xffe6e8ea), geo.outer.getRight(), geo.outer.getBottom(), false));
    g.fillRoundedRectangle (geo.outer.reduced (3.0f), 6.0f);
    g.setColour (juce::Colour (0xff0c0c0c));
    g.fillRoundedRectangle (geo.bezel, 4.0f);

    // Beleuchtete Skala (warmes Glühlampenlicht von unten)
    const auto face = geo.face;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c4), face.getCentreX(), face.getBottom() - face.getHeight() * 0.1f,
                                             juce::Colour (0xffc98a35), face.getX() - face.getWidth() * 0.1f, face.getY(), true));
    g.fillRoundedRectangle (face, 3.0f);

    // Papierstruktur
    juce::Random paper (7);
    for (int i = 0; i < (int) (face.getWidth() * face.getHeight() / 6.0f); ++i)
    {
        g.setColour ((paper.nextBool() ? juce::Colours::white : juce::Colour (0xff6b4a1a)).withAlpha (0.05f));
        g.fillRect (face.getX() + paper.nextFloat() * face.getWidth(), face.getY() + paper.nextFloat() * face.getHeight(), 0.8f, 0.8f);
    }

    const float cx = geo.cx, pivotY = geo.pivotY, radius = geo.radius;
    auto pointAt = [&] (float angle, float rad)
    {
        return juce::Point<float> (cx + rad * std::sin (angle), pivotY - rad * std::cos (angle));
    };

    // Roter Bereich 0..+3 VU
    {
        juce::Path red;
        red.addCentredArc (cx, pivotY, radius * 0.865f, radius * 0.865f, 0.0f, angleFor (vuToPosition (0.0f)), angleFor (1.0f), true);
        g.setColour (juce::Colour (0xffb3241a));
        g.strokePath (red, juce::PathStrokeType (radius * 0.065f));
    }

    // Skalenbogen
    {
        juce::Path arc;
        arc.addCentredArc (cx, pivotY, radius * 0.82f, radius * 0.82f, 0.0f, angleFor (0.0f), angleFor (1.0f), true);
        g.setColour (juce::Colour (0xff2a2017));
        g.strokePath (arc, juce::PathStrokeType (1.2f));
    }

    // Skalenstriche + Zahlen
    const float fontSize = juce::jmax (10.0f, face.getHeight() * 0.14f);
    for (int vuInt : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const float a = angleFor (vuToPosition ((float) vuInt));
        const bool major = vuInt == -20 || vuInt == -10 || vuInt == -7 || vuInt == -5 || vuInt == -3 || vuInt == 0 || vuInt == 3;
        g.setColour (vuInt > 0 ? juce::Colour (0xffa81f16) : juce::Colour (0xff2a2017));
        g.drawLine (juce::Line<float> (pointAt (a, radius * 0.82f), pointAt (a, radius * (major ? 0.94f : 0.89f))), major ? 1.6f : 1.1f);

        g.setFont (TapeFonts::label (fontSize));
        const juce::String txt = vuInt > 0 ? "+" + juce::String (vuInt) : juce::String (std::abs (vuInt));
        g.drawText (txt, juce::Rectangle<float> (fontSize * 2.4f, fontSize * 1.2f).withCentre (pointAt (a, radius * 1.03f)),
                    juce::Justification::centred);
    }

    // Prozentskala unten (klassisch)
    {
        juce::Path arc2;
        arc2.addCentredArc (cx, pivotY, radius * 0.70f, radius * 0.70f, 0.0f, angleFor (0.0f), angleFor (vuToPosition (0.0f)), true);
        g.setColour (juce::Colour (0xff2a2017).withAlpha (0.55f));
        g.strokePath (arc2, juce::PathStrokeType (0.8f));
    }

    // "VU" und Beschriftung
    g.setColour (juce::Colour (0xff2a2017));
    g.setFont (TapeFonts::emblem (fontSize * 1.7f));
    g.drawText ("VU", juce::Rectangle<float> (cx - 40.0f, face.getY() + face.getHeight() * 0.44f, 80.0f, fontSize * 1.9f),
                juce::Justification::centred);
    g.setFont (TapeFonts::label (fontSize * 0.85f));
    g.drawText (caption, juce::Rectangle<float> (face.getX(), face.getBottom() - fontSize * 2.3f, face.getWidth(), fontSize * 1.2f),
                juce::Justification::centred);

    // Innenschatten am Skalenrand (Tiefe)
    juce::Path facePath;
    facePath.addRoundedRectangle (face, 3.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (facePath);
        juce::Path outside;
        outside.addRectangle (face.expanded (20.0f));
        outside.addRoundedRectangle (face, 3.0f);
        outside.setUsingNonZeroWinding (false);
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 10, { 2, 3 }).drawForPath (g, outside);
    }
}

void VUMeter::renderGlass (float scale)
{
    const int w = getWidth(), h = getHeight();
    glassLayer = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) std::ceil ((float) w * scale)), juce::jmax (1, (int) std::ceil ((float) h * scale)), true);
    juce::Graphics g (glassLayer);
    g.addTransform (juce::AffineTransform::scale (scale));

    const auto geo = geometry (getLocalBounds().toFloat());
    const auto face = geo.face;

    // Abdeckung des Drehpunkts (schwarze Kappe)
    const float capH = face.getHeight() * 0.13f;
    auto cap = face.withTop (face.getBottom() - capH);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), cap.getX(), cap.getY(),
                                             juce::Colour (0xff0d0d0d), cap.getX(), cap.getBottom(), false));
    g.fillRect (cap);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (cap.withHeight (1.0f));

    // Glasreflex: diagonale Lichtfläche von links oben
    juce::Path glare;
    glare.startNewSubPath (face.getX(), face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.62f, face.getY());
    glare.quadraticTo (face.getX() + face.getWidth() * 0.35f, face.getY() + face.getHeight() * 0.35f,
                       face.getX(), face.getY() + face.getHeight() * 0.58f);
    glare.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.32f), face.getX(), face.getY(),
                                             juce::Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.4f,
                                             face.getY() + face.getHeight() * 0.4f, false));
    g.fillPath (glare);

    // dünne Lichtkante oben am Glas
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine ((int) face.getY() + 1, face.getX() + 4.0f, face.getRight() - 4.0f);
}

void VUMeter::paint (juce::Graphics& g)
{
    const float pixelScale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (pixelScale - cachedScale) > 0.01f || cachedBounds != getLocalBounds())
    {
        cachedScale = pixelScale;
        cachedBounds = getLocalBounds();
        renderFace (pixelScale);
        renderGlass (pixelScale);
    }

    const auto area = getLocalBounds().toFloat();
    g.drawImage (faceLayer, area);

    const auto geo = geometry (area);
    const float angle = angleFor (needlePos);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (geo.face.toNearestInt());

        const auto tip = juce::Point<float> (geo.cx + geo.radius * 0.96f * std::sin (angle), geo.pivotY - geo.radius * 0.96f * std::cos (angle));
        const auto base = juce::Point<float> (geo.cx, geo.pivotY);

        // Schatten der Nadel auf der Skala (Licht von oben links)
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.drawLine (juce::Line<float> (base.translated (3.0f, 3.0f), tip.translated (3.0f, 3.0f)), 2.0f);
        g.setColour (juce::Colour (0xff121212));
        g.drawLine (juce::Line<float> (base, tip), 1.3f);
        // rote Spitze
        const auto mid = base + (tip - base) * 0.82f;
        g.setColour (juce::Colour (0xffb3241a));
        g.drawLine (juce::Line<float> (mid, tip), 1.4f);
    }

    g.drawImage (glassLayer, area);
}
