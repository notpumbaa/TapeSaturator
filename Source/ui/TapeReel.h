#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Tonbandspule (NAB-Metallspule) mit Bandwickel.
    Winkel und Wickelgröße werden von außen gesetzt; das Glanzlicht bleibt beim
    Drehen stehen (Licht von links oben).
*/
class TapeReel : public juce::Component
{
public:
    explicit TapeReel (float packRatio) : pack (packRatio) { setOpaque (false); }

    void setState (float radians, float packRatio)
    {
        if (std::abs (radians - angle) < 1.0e-4f && std::abs (packRatio - pack) < 1.0e-4f)
            return;
        angle = radians;
        pack = packRatio;
        repaint();
    }

    float getPackRatio() const noexcept { return pack; }

    /** Radius des Spulenflanschs in Komponenten-Koordinaten. */
    float getFlangeRadius() const noexcept { return juce::jmin (getWidth(), getHeight()) * 0.5f - 8.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float R = getFlangeRadius();
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        // weicher Schatten auf dem Laufwerk
        juce::Path disc;
        disc.addEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 12, { 4, 7 }).drawForPath (g, disc);

        // Bandwickel (sichtbar durch die Aussparungen)
        const float packR = R * pack;
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillEllipse (c.x - R * 0.97f, c.y - R * 0.97f, 1.94f * R, 1.94f * R); // dunkler Hintergrund
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff53392a), c.x - packR * 0.4f, c.y - packR * 0.4f,
                                                 juce::Colour (0xff1e140d), c.x + packR, c.y + packR, true));
        g.fillEllipse (c.x - packR, c.y - packR, 2.0f * packR, 2.0f * packR);
        g.setColour (juce::Colour (0xff6a4e3a).withAlpha (0.35f));
        for (float rr = R * 0.3f; rr < packR; rr += 2.5f)
            g.drawEllipse (c.x - rr, c.y - rr, 2.0f * rr, 2.0f * rr, 0.35f);
        // Glanz auf der Bandkante
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), c.x - packR, c.y - packR,
                                                 juce::Colours::transparentWhite, c.x, c.y, false));
        g.fillEllipse (c.x - packR, c.y - packR, 2.0f * packR, 2.0f * packR);

        // Metallflansch mit drei Aussparungen
        juce::Path flange;
        flange.addEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
        flange.setUsingNonZeroWinding (false);
        for (int i = 0; i < 3; ++i)
        {
            const float a = angle + (float) i * twoPi / 3.0f;
            juce::Path window;
            window.addCentredArc (0.0f, 0.0f, R * 0.80f, R * 0.80f, 0.0f, -0.62f, 0.62f, true);
            window.addCentredArc (0.0f, 0.0f, R * 0.36f, R * 0.36f, 0.0f, 0.55f, -0.55f, false);
            window.closeSubPath();
            window.applyTransform (juce::AffineTransform::rotation (a).translated (c));
            flange.addPath (window);
        }

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dcdf), c.x - R, c.y - R,
                                                 juce::Colour (0xff70767b), c.x + R, c.y + R, false));
        g.fillPath (flange);

        // konzentrische Drehriefen (bleiben optisch stehen – wie bei echter Spule)
        for (float rr = R * 0.82f; rr < R * 0.99f; rr += 1.6f)
        {
            g.setColour ((((int) (rr * 7.0f)) % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
            g.drawEllipse (c.x - rr, c.y - rr, 2.0f * rr, 2.0f * rr, 0.7f);
        }

        // statisches Glanzlicht (Sweep von links oben)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (flange);
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), c.x - R, c.y + R * 0.2f,
                                        juce::Colours::white.withAlpha (0.0f), c.x + R * 0.2f, c.y - R, false);
            sheen.addColour (0.45, juce::Colours::white.withAlpha (0.45f));
            sheen.addColour (0.55, juce::Colours::white.withAlpha (0.10f));
            g.setGradientFill (sheen);
            g.fillEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);

            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x, c.y,
                                                     juce::Colours::black.withAlpha (0.25f), c.x + R, c.y + R, true));
            g.fillEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
        }

        // Kanten der Aussparungen: Fase (hell unten rechts, dunkel oben links)
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (flange, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (flange, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (0.6f, 0.8f));

        // Nabe
        const float hubR = R * 0.24f;
        juce::Path hub;
        hub.addEllipse (c.x - hubR, c.y - hubR, 2.0f * hubR, 2.0f * hubR);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 5, { 1, 2 }).drawForPath (g, hub);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6f74), c.x - hubR, c.y - hubR,
                                                 juce::Colour (0xff1a1c1e), c.x + hubR, c.y + hubR, false));
        g.fillPath (hub);

        g.setColour (juce::Colour (0xffd2d6d9));
        for (int i = 0; i < 3; ++i)
        {
            const float a = angle + 0.5f + (float) i * twoPi / 3.0f;
            juce::Path key;
            key.addRoundedRectangle (-hubR * 0.14f, -hubR * 0.95f, hubR * 0.28f, hubR * 0.5f, 1.5f);
            g.fillPath (key, juce::AffineTransform::rotation (a).translated (c));
        }
        g.setColour (juce::Colour (0xff0a0a0a));
        g.fillEllipse (c.x - hubR * 0.28f, c.y - hubR * 0.28f, hubR * 0.56f, hubR * 0.56f);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawEllipse (c.x - hubR * 0.28f, c.y - hubR * 0.28f, hubR * 0.56f, hubR * 0.56f, 0.6f);
    }

private:
    float pack = 0.7f;
    float angle = 0.0f;
};
