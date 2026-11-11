#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Tonbandspule (NAB-Metallspule) mit Bandwickel; dreht sich per setAngle(). */
class TapeReel : public juce::Component
{
public:
    /** packRatio: Außenradius des Bandwickels relativ zum Spulenradius (0.35..0.95). */
    explicit TapeReel (float packRatio) : pack (packRatio) { setOpaque (false); }

    void setAngle (float radians)
    {
        angle = radians;
        repaint();
    }

    float getPackRatio() const noexcept { return pack; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (2.0f);
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();

        // Schatten
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - R + 4.0f, c.y - R + 6.0f, 2.0f * R, 2.0f * R);

        // Bandwickel (sichtbar durch die Aussparungen)
        const float packR = R * pack;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3426), c.x, c.y,
                                                 juce::Colour (0xff22170f), c.x + packR, c.y, true));
        g.fillEllipse (c.x - packR, c.y - packR, 2.0f * packR, 2.0f * packR);
        g.setColour (juce::Colour (0xff5b4231).withAlpha (0.5f));
        for (float rr = R * 0.3f; rr < packR; rr += 3.0f)
            g.drawEllipse (c.x - rr, c.y - rr, 2.0f * rr, 2.0f * rr, 0.4f);

        // Metallflansch mit drei Aussparungen
        juce::Path flange;
        flange.addEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R);
        flange.setUsingNonZeroWinding (false);
        for (int i = 0; i < 3; ++i)
        {
            const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path window;
            window.addCentredArc (0.0f, 0.0f, R * 0.80f, R * 0.80f, 0.0f, -0.62f, 0.62f, true);
            window.addCentredArc (0.0f, 0.0f, R * 0.36f, R * 0.36f, 0.0f, 0.55f, -0.55f, false);
            window.closeSubPath();
            window.applyTransform (juce::AffineTransform::rotation (a).translated (c));
            flange.addPath (window);
        }

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9ecee), c.x - R, c.y - R,
                                                 juce::Colour (0xff80868b), c.x + R, c.y + R, false));
        g.fillPath (flange);

        // radiale Bürstung / Glanzlicht
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = angle + (float) i * juce::MathConstants<float>::twoPi / 48.0f;
            g.drawLine (c.x + R * 0.83f * std::sin (a), c.y - R * 0.83f * std::cos (a),
                        c.x + R * 0.98f * std::sin (a), c.y - R * 0.98f * std::cos (a), 0.5f);
        }

        g.setColour (juce::Colour (0xff4a4f53));
        g.drawEllipse (c.x - R, c.y - R, 2.0f * R, 2.0f * R, 1.2f);
        g.strokePath (flange, juce::PathStrokeType (0.6f));

        // Nabe
        const float hubR = R * 0.24f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5d6266), c.x, c.y - hubR,
                                                 juce::Colour (0xff1d1f21), c.x, c.y + hubR, false));
        g.fillEllipse (c.x - hubR, c.y - hubR, 2.0f * hubR, 2.0f * hubR);

        // Drei Mitnehmer der Nabe
        g.setColour (juce::Colour (0xffc9cdd0));
        for (int i = 0; i < 3; ++i)
        {
            const float a = angle + 0.5f + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path key;
            key.addRoundedRectangle (-hubR * 0.14f, -hubR * 0.95f, hubR * 0.28f, hubR * 0.5f, 1.5f);
            g.fillPath (key, juce::AffineTransform::rotation (a).translated (c));
        }
        g.setColour (juce::Colour (0xff0c0c0c));
        g.fillEllipse (c.x - hubR * 0.28f, c.y - hubR * 0.28f, hubR * 0.56f, hubR * 0.56f);
    }

private:
    float pack = 0.7f;
    float angle = 0.0f;
};
