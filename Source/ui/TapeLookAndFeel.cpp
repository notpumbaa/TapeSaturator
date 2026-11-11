#include "TapeLookAndFeel.h"
#include "Fonts.h"

TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, TapeColours::engraving);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, TapeColours::engraving);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee141414));
    setColour (juce::BubbleComponent::outlineColourId, TapeColours::amber.withAlpha (0.7f));
    setColour (juce::TooltipWindow::backgroundColourId, TapeColours::machine);
    setColour (juce::TooltipWindow::textColourId, TapeColours::cream);
}

juce::Font TapeLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return juce::Font (TapeFonts::label (16.0f));
}

int TapeLookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}

void TapeLookAndFeel::drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                                        juce::Justification just, float fontSize, bool label)
{
    g.setFont (label ? TapeFonts::label (fontSize) : TapeFonts::scale (fontSize));
    g.setColour (TapeColours::engraveHi);
    g.drawText (text, r.translated (0.0f, 1.0f), just, false);
    g.setColour (TapeColours::engraving);
    g.drawText (text, r, just, false);
}

juce::Image TapeLookAndFeel::createBrushedMetal (int width, int height, float scale)
{
    const int w = juce::jmax (1, (int) std::ceil (width * scale));
    const int h = juce::jmax (1, (int) std::ceil (height * scale));
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Graphics g (img);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6d9dc), 0.0f, 0.0f,
                                             juce::Colour (0xffa3a7ab), 0.0f, (float) h, false));
    g.fillAll();

    // horizontale Bürstung: feine Zeilen + längere Kratzer
    juce::Random rng (1234);
    for (int y = 0; y < h; ++y)
    {
        const float v = rng.nextFloat();
        g.setColour ((v > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + 0.06f * std::abs (v - 0.5f)));
        g.fillRect (0, y, w, 1);
    }
    for (int i = 0; i < w * h / 450; ++i)
    {
        const float y = rng.nextFloat() * (float) h;
        const float x = rng.nextFloat() * (float) w;
        const float len = (30.0f + rng.nextFloat() * 220.0f) * scale;
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.045f));
        g.drawHorizontalLine ((int) y, x, x + len);
    }

    // Licht von links oben + Abdunklung zu den Rändern
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), (float) w * 0.25f, -(float) h * 0.2f,
                                             juce::Colours::transparentWhite, (float) w * 0.55f, (float) h * 1.1f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.4f,
                                             juce::Colours::black.withAlpha (0.18f), 0.0f, (float) h * 1.2f, true));
    g.fillAll();
    return img;
}

juce::Image TapeLookAndFeel::createWoodPanel (int width, int height, float scale, int seed)
{
    const int w = juce::jmax (1, (int) std::ceil (width * scale));
    const int h = juce::jmax (1, (int) std::ceil (height * scale));
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Graphics g (img);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a341c), 0.0f, 0.0f,
                                             juce::Colour (0xff3a2010), (float) w, 0.0f, false));
    g.fillAll();

    // Maserung: senkrechte, leicht wellige Linien unterschiedlicher Stärke
    juce::Random rng (seed);
    const int lines = (int) (w * 0.9f);
    for (int i = 0; i < lines; ++i)
    {
        const float x0 = rng.nextFloat() * (float) w;
        const float amp = (0.5f + rng.nextFloat() * 2.5f) * scale;
        const float freq = 0.004f + rng.nextFloat() * 0.01f;
        const float phase = rng.nextFloat() * 6.28f;
        const bool dark = rng.nextFloat() < 0.65f;

        juce::Path p;
        p.startNewSubPath (x0, 0.0f);
        for (float y = 0.0f; y <= (float) h; y += 6.0f)
            p.lineTo (x0 + amp * std::sin (y * freq / scale + phase) + 0.6f * amp * std::sin (y * freq * 3.1f / scale), y);

        g.setColour ((dark ? juce::Colour (0xff1e0f06) : juce::Colour (0xff8a5a32)).withAlpha (0.10f + 0.18f * rng.nextFloat()));
        g.strokePath (p, juce::PathStrokeType ((0.4f + rng.nextFloat() * 1.4f) * scale));
    }

    // Lackglanz + Kanten
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), 0.0f, 0.0f,
                                             juce::Colours::transparentWhite, 0.0f, (float) h * 0.5f, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), 0.0f, 0.0f,
                                             juce::Colours::transparentWhite, (float) w * 0.35f, 0.0f, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, (float) w * 0.6f, 0.0f,
                                             juce::Colours::black.withAlpha (0.45f), (float) w, 0.0f, false));
    g.fillAll();
    return img;
}

void TapeLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float rotation)
{
    juce::Path head;
    head.addEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), (int) (r * 0.9f), { 1, 2 }).drawForPath (g, head);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2f3), c.x - r, c.y - r,
                                             juce::Colour (0xff5a5e62), c.x + r, c.y + r, false));
    g.fillPath (head);
    g.setColour (juce::Colour (0xff3d4044));
    g.strokePath (head, juce::PathStrokeType (0.7f));

    juce::Path slot;
    slot.addRectangle (-r * 0.68f, -r * 0.11f, r * 1.36f, r * 0.22f);
    slot.addRectangle (-r * 0.11f, -r * 0.68f, r * 0.22f, r * 1.36f);
    const auto xf = juce::AffineTransform::rotation (rotation).translated (c);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillPath (slot, xf.translated (0.4f, 0.6f));
    g.setColour (juce::Colour (0xff26282b));
    g.fillPath (slot, xf);
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // --- Gravierte Skala ---
    const int ticks = 11;
    const float fontSize = juce::jmax (9.0f, R * 0.26f);
    for (int i = 0; i < ticks; ++i)
    {
        const float t = (float) i / (float) (ticks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const float s = std::sin (a), co = -std::cos (a);
        const bool major = (i % 5) == 0;
        const float r0 = R * 0.70f, r1 = R * (major ? 0.83f : 0.78f);
        const float thick = major ? 1.6f : 1.0f;

        g.setColour (TapeColours::engraveHi);
        g.drawLine (c.x + r0 * s, c.y + r0 * co + 1.0f, c.x + r1 * s, c.y + r1 * co + 1.0f, thick);
        g.setColour (TapeColours::engraving);
        g.drawLine (c.x + r0 * s, c.y + r0 * co, c.x + r1 * s, c.y + r1 * co, thick);

        if (major)
        {
            const float rt = R * (i == 5 ? 0.95f : 0.97f);
            drawEngravedText (g, juce::String (i), juce::Rectangle<float> (fontSize * 2.0f, fontSize * 1.2f)
                                                       .withCentre ({ c.x + rt * s, c.y + rt * co }),
                              juce::Justification::centred, fontSize, false);
        }
    }

    // --- Chromsockel mit radialer Bürstung ---
    const float baseR = R * 0.58f;
    juce::Path base;
    base.addEllipse (c.x - baseR, c.y - baseR, 2.0f * baseR, 2.0f * baseR);
    juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 6, { 1, 3 }).drawForPath (g, base);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7f8f9), c.x - baseR, c.y - baseR,
                                             juce::Colour (0xff62666a), c.x + baseR, c.y + baseR, false));
    g.fillPath (base);
    for (int i = 0; i < 72; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 72.0f;
        g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.07f));
        g.drawLine (c.x + baseR * 0.3f * std::sin (a), c.y - baseR * 0.3f * std::cos (a),
                    c.x + baseR * 0.98f * std::sin (a), c.y - baseR * 0.98f * std::cos (a), 0.8f);
    }
    g.setColour (juce::Colour (0xff3f4347));
    g.strokePath (base, juce::PathStrokeType (0.8f));

    // --- Chicken-Head aus Bakelit ---
    const float bodyR = R * 0.40f;
    const float pointerLen = R * 0.70f;
    juce::Path head;
    head.startNewSubPath (0.0f, -pointerLen);
    head.cubicTo (bodyR * 0.30f, -pointerLen * 0.75f, bodyR * 0.85f, -bodyR * 0.55f, bodyR, 0.0f);
    head.cubicTo (bodyR * 1.05f, bodyR * 0.85f, bodyR * 0.45f, bodyR * 1.35f, 0.0f, bodyR * 1.40f);
    head.cubicTo (-bodyR * 0.45f, bodyR * 1.35f, -bodyR * 1.05f, bodyR * 0.85f, -bodyR, 0.0f);
    head.cubicTo (-bodyR * 0.85f, -bodyR * 0.55f, -bodyR * 0.30f, -pointerLen * 0.75f, 0.0f, -pointerLen);
    head.closeSubPath();
    head.applyTransform (juce::AffineTransform::rotation (angle).translated (c));

    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 7, { 2, 4 }).drawForPath (g, head);

    const bool hot = slider.isMouseOverOrDragging();
    g.setGradientFill (juce::ColourGradient (juce::Colour (hot ? 0xff4a4a4a : 0xff3a3a3a), c.x - bodyR, c.y - bodyR * 1.2f,
                                             TapeColours::bakelite, c.x + bodyR * 0.6f, c.y + bodyR, false));
    g.fillPath (head);

    // Glanzlicht (Licht von links oben, bleibt beim Drehen stehen)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (head);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), c.x - bodyR * 0.6f, c.y - bodyR * 0.7f,
                                                 juce::Colours::transparentWhite, c.x + bodyR * 0.1f, c.y + bodyR * 0.2f, true));
        g.fillEllipse (c.x - bodyR * 1.1f, c.y - bodyR * 1.2f, bodyR * 1.6f, bodyR * 1.3f);
    }
    // Lichtkante
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - bodyR, c.y - bodyR,
                                             juce::Colours::transparentWhite, c.x + bodyR * 0.5f, c.y + bodyR * 0.5f, false));
    g.strokePath (head, juce::PathStrokeType (1.0f));

    // eingelegte Zeigerlinie
    juce::Path line;
    line.addRoundedRectangle (-1.3f, -pointerLen + 3.0f, 2.6f, pointerLen * 0.62f, 1.3f);
    const auto lineXf = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (line, lineXf.translated (0.0f, 0.8f));
    g.setColour (TapeColours::cream);
    g.fillPath (line, lineXf);
}

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    const auto labels = juce::StringArray::fromTokens (b.getProperties()["switchLabels"].toString(), "|", "");
    const juce::String upLabel   = labels.size() > 0 ? labels[0] : "ON";
    const juce::String downLabel = labels.size() > 1 ? labels[1] : "OFF";

    const float labelH = juce::jmin (16.0f, r.getHeight() * 0.18f);
    drawEngravedText (g, upLabel, r.removeFromTop (labelH), juce::Justification::centred, labelH * 0.95f);
    drawEngravedText (g, downLabel, r.removeFromBottom (labelH), juce::Justification::centred, labelH * 0.95f);

    // Montagemutter
    const auto c = r.getCentre();
    const float nutR = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
    juce::Path hex;
    hex.addPolygon (c, 6, nutR, 0.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 5, { 1, 2 }).drawForPath (g, hex);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3f4f5), c.x - nutR, c.y - nutR,
                                             juce::Colour (0xff62666a), c.x + nutR, c.y + nutR, false));
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff45494d));
    g.strokePath (hex, juce::PathStrokeType (0.8f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0f10), c.x, c.y - nutR * 0.45f,
                                             juce::Colour (0xff4a4d50), c.x, c.y + nutR * 0.45f, false));
    g.fillEllipse (c.x - nutR * 0.45f, c.y - nutR * 0.45f, nutR * 0.9f, nutR * 0.9f);

    // Kipphebel ("Bat Handle"), oben = an
    const float leverLen = juce::jmin (r.getHeight() * 0.42f, nutR * 2.2f);
    const float dir = on ? -1.0f : 1.0f;
    const float tipY = c.y + dir * leverLen;
    const float tipR = nutR * 0.42f;

    juce::Path lever;
    lever.startNewSubPath (c.x - nutR * 0.18f, c.y);
    lever.lineTo (c.x - tipR * 0.75f, tipY);
    lever.lineTo (c.x + tipR * 0.75f, tipY);
    lever.lineTo (c.x + nutR * 0.18f, c.y);
    lever.closeSubPath();
    lever.addEllipse (c.x - tipR, tipY - tipR, 2.0f * tipR, 2.0f * tipR);

    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 5, { 3, on ? 4 : 2 }).drawForPath (g, lever);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7c8085), c.x - nutR * 0.25f, c.y,
                                             juce::Colour (0xfff7f8f9), c.x + nutR * 0.05f, c.y, false));
    g.fillPath (lever);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), c.x - tipR * 0.4f, tipY - tipR * 0.4f,
                                             juce::Colour (0xff767a7e), c.x + tipR, tipY + tipR, true));
    g.fillEllipse (c.x - tipR, tipY - tipR, 2.0f * tipR, 2.0f * tipR);
    g.setColour (highlighted ? TapeColours::amber : juce::Colour (0xff55595d));
    g.drawEllipse (c.x - tipR, tipY - tipR, 2.0f * tipR, 2.0f * tipR, 0.8f);
}
