#include "TapeLookAndFeel.h"

TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, TapeColours::engraving);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, TapeColours::engraving);
    setColour (juce::BubbleComponent::backgroundColourId, TapeColours::machine.withAlpha (0.92f));
    setColour (juce::BubbleComponent::outlineColourId, TapeColours::amber);
    setColour (juce::TooltipWindow::backgroundColourId, TapeColours::machine);
    setColour (juce::TooltipWindow::textColourId, TapeColours::cream);
}

void TapeLookAndFeel::drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                                        juce::Justification just, float fontSize, bool bold)
{
    g.setFont (juce::FontOptions (fontSize, bold ? juce::Font::bold : juce::Font::plain));
    g.setColour (TapeColours::engraveHi);
    g.drawText (text, r.translated (0.0f, 1.0f), just, false);
    g.setColour (TapeColours::engraving);
    g.drawText (text, r, just, false);
}

juce::Image TapeLookAndFeel::createBrushedMetal (int width, int height, float scale)
{
    const int w = juce::jmax (1, (int) (width * scale));
    const int h = juce::jmax (1, (int) (height * scale));
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Graphics g (img);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd2d5d8), 0.0f, 0.0f,
                                             juce::Colour (0xffa7abaf), 0.0f, (float) h, false));
    g.fillAll();

    // horizontale Bürstung
    juce::Random rng (1234);
    for (int y = 0; y < h; ++y)
    {
        const float v = rng.nextFloat();
        g.setColour ((v > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f + 0.05f * std::abs (v - 0.5f)));
        g.fillRect (0, y, w, 1);
    }
    for (int i = 0; i < w * h / 600; ++i)
    {
        const float y = rng.nextFloat() * (float) h;
        const float x = rng.nextFloat() * (float) w;
        const float len = 20.0f + rng.nextFloat() * 160.0f * scale;
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
        g.drawHorizontalLine ((int) y, x, x + len);
    }

    // weiches Glanzlicht
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), (float) w * 0.3f, 0.0f,
                                             juce::Colours::transparentWhite, (float) w * 0.6f, (float) h, false));
    g.fillAll();
    return img;
}

void TapeLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float rotation)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e8ea), c.x - r, c.y - r,
                                             juce::Colour (0xff5e6266), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colour (0xff3d4044));
    g.drawEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r, 0.8f);

    juce::Path slot;
    slot.addRectangle (-r * 0.7f, -r * 0.12f, r * 1.4f, r * 0.24f);
    slot.addRectangle (-r * 0.12f, -r * 0.7f, r * 0.24f, r * 1.4f);
    g.setColour (juce::Colour (0xff2b2d30));
    g.fillPath (slot, juce::AffineTransform::rotation (rotation).translated (c));
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Gravierte Skala: 11 Striche, Zahlen 0..10 an jedem zweiten
    const int ticks = 11;
    const float fontSize = juce::jmax (8.0f, R * 0.2f);
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const float s = std::sin (a), co = -std::cos (a);
        const bool major = (i % 2) == 0;
        const float r0 = R * 0.72f, r1 = R * (major ? 0.82f : 0.79f);

        g.setColour (TapeColours::engraveHi);
        g.drawLine (c.x + r0 * s, c.y + r0 * co + 1.0f, c.x + r1 * s, c.y + r1 * co + 1.0f, major ? 1.4f : 1.0f);
        g.setColour (TapeColours::engraving);
        g.drawLine (c.x + r0 * s, c.y + r0 * co, c.x + r1 * s, c.y + r1 * co, major ? 1.4f : 1.0f);

        if (i == 0 || i == 5 || i == 10)
        {
            const float rt = R * (i == 5 ? 0.95f : 0.97f);
            drawEngravedText (g, juce::String (i), juce::Rectangle<float> (fontSize * 2.0f, fontSize * 1.2f)
                                                       .withCentre ({ c.x + rt * s, c.y + rt * co }),
                              juce::Justification::centred, fontSize, false);
        }
    }

    // Bodenplatte (Chrom-Ring)
    const float baseR = R * 0.56f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2f3), c.x - baseR, c.y - baseR,
                                             juce::Colour (0xff6f7377), c.x + baseR, c.y + baseR, false));
    g.fillEllipse (c.x - baseR, c.y - baseR, 2.0f * baseR, 2.0f * baseR);

    // Chicken-Head-Körper (Schatten, dann Bakelit)
    const float bodyR = R * 0.40f;
    const float pointerLen = R * 0.70f;
    juce::Path head;
    head.startNewSubPath (0.0f, -pointerLen);
    head.cubicTo (bodyR * 0.30f, -pointerLen * 0.75f, bodyR * 0.85f, -bodyR * 0.55f, bodyR, 0.0f);
    head.cubicTo (bodyR * 1.05f, bodyR * 0.85f, bodyR * 0.45f, bodyR * 1.35f, 0.0f, bodyR * 1.40f);
    head.cubicTo (-bodyR * 0.45f, bodyR * 1.35f, -bodyR * 1.05f, bodyR * 0.85f, -bodyR, 0.0f);
    head.cubicTo (-bodyR * 0.85f, -bodyR * 0.55f, -bodyR * 0.30f, -pointerLen * 0.75f, 0.0f, -pointerLen);
    head.closeSubPath();

    const auto xf = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (head, xf.translated (2.0f, 3.0f));

    const bool hot = slider.isMouseOverOrDragging();
    g.setGradientFill (juce::ColourGradient (juce::Colour (hot ? 0xff3a3a3a : 0xff2e2e2e), c.x - bodyR, c.y - bodyR,
                                             TapeColours::bakelite, c.x + bodyR, c.y + bodyR, false));
    g.fillPath (head, xf);

    // Glanzlicht
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (c.x - bodyR * 0.75f, c.y - bodyR * 0.8f, bodyR * 1.0f, bodyR * 0.7f);

    // weiße Zeigerlinie
    juce::Path line;
    line.addRoundedRectangle (-1.2f, -pointerLen + 3.0f, 2.4f, pointerLen * 0.62f, 1.2f);
    g.setColour (TapeColours::cream);
    g.fillPath (line, xf);
}

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    const auto labels = juce::StringArray::fromTokens (b.getProperties()["switchLabels"].toString(), "|", "");
    const juce::String upLabel   = labels.size() > 0 ? labels[0] : "ON";
    const juce::String downLabel = labels.size() > 1 ? labels[1] : "OFF";

    const float labelH = juce::jmin (16.0f, r.getHeight() * 0.2f);
    drawEngravedText (g, upLabel, r.removeFromTop (labelH), juce::Justification::centred, labelH * 0.75f);
    drawEngravedText (g, downLabel, r.removeFromBottom (labelH), juce::Justification::centred, labelH * 0.75f);

    // Montagemutter
    const auto c = r.getCentre();
    const float nutR = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
    juce::Path hex;
    hex.addPolygon (c, 6, nutR, 0.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f1), c.x - nutR, c.y - nutR,
                                             juce::Colour (0xff6a6e72), c.x + nutR, c.y + nutR, false));
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff45494d));
    g.strokePath (hex, juce::PathStrokeType (0.8f));
    g.setColour (juce::Colour (0xff2a2c2e));
    g.fillEllipse (c.x - nutR * 0.45f, c.y - nutR * 0.45f, nutR * 0.9f, nutR * 0.9f);

    // Kipphebel ("Bat Handle"), oben = an
    const float leverLen = juce::jmin (r.getHeight() * 0.42f, nutR * 2.2f);
    const float dir = on ? -1.0f : 1.0f;
    const float tipY = c.y + dir * leverLen;
    const float tipR = nutR * 0.42f;

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawLine (c.x + 2.0f, c.y + 2.0f, c.x + 2.0f, tipY + 2.0f, nutR * 0.4f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8e92), c.x - nutR * 0.2f, c.y,
                                             juce::Colour (0xfff5f6f7), c.x + nutR * 0.05f, c.y, false));
    juce::Path lever;
    lever.startNewSubPath (c.x - nutR * 0.18f, c.y);
    lever.lineTo (c.x - tipR * 0.75f, tipY);
    lever.lineTo (c.x + tipR * 0.75f, tipY);
    lever.lineTo (c.x + nutR * 0.18f, c.y);
    lever.closeSubPath();
    g.fillPath (lever);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), c.x - tipR * 0.4f, tipY - tipR * 0.4f,
                                             juce::Colour (0xff7d8185), c.x + tipR, tipY + tipR, true));
    g.fillEllipse (c.x - tipR, tipY - tipR, 2.0f * tipR, 2.0f * tipR);
    g.setColour (highlighted ? TapeColours::amber : juce::Colour (0xff55595d));
    g.drawEllipse (c.x - tipR, tipY - tipR, 2.0f * tipR, 2.0f * tipR, 0.8f);
}
