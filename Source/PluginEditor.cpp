#include "PluginEditor.h"

namespace
{
    constexpr int colW          = 80;   // Breite einer Bedienelement-Spalte
    constexpr int sectionPad    = 12;
    constexpr int sectionGap    = 12;
    constexpr int outerMargin   = 18;
    constexpr int transportH    = 300;
    constexpr int plateH        = 220;
    constexpr int captionH      = 18;
    constexpr int knobSize      = 80;
    constexpr int timerHz       = 30;

    constexpr std::array<int, 4> sectionColumns { 4, 2, 3, 3 };

    int totalWidth()
    {
        int cols = 0;
        for (int c : sectionColumns)
            cols += c;
        return 2 * outerMargin + cols * colW + 4 * 2 * sectionPad + 3 * sectionGap;
    }
}

TapeSaturatorAudioProcessorEditor::TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (leftReel);
    addAndMakeVisible (rightReel);
    addAndMakeVisible (inputVU);
    addAndMakeVisible (outputVU);

    addKnob (input,      ParamIDs::input,      "INPUT");
    addKnob (saturation, ParamIDs::saturation, "SATURATION");
    addKnob (bias,       ParamIDs::bias,       "BIAS");
    addSwitch (speedSwitch, ParamIDs::speed,   "SPEED", "15 IPS|7.5 IPS");
    addKnob (flutter,    ParamIDs::flutter,    "WOW/FLUTTER");
    addKnob (noise,      ParamIDs::noise,      "NOISE");
    addKnob (delayTime,  ParamIDs::delayTime,  "TIME");
    addKnob (delayFb,    ParamIDs::delayFb,    "FEEDBACK");
    addKnob (delayMix,   ParamIDs::delayMix,   "MIX");
    addKnob (output,     ParamIDs::output,     "OUTPUT");
    addKnob (mix,        ParamIDs::mix,        "DRY/WET");
    addSwitch (autoGainSwitch, ParamIDs::autoGain, "AUTO GAIN", "ON|OFF");

    sections = { Section { "TAPE", {} }, Section { "TRANSPORT", {} },
                 Section { "SLAP DELAY", {} }, Section { "OUTPUT", {} } };

    lampOn = autoGainSwitch.button.getToggleState();

    setSize (totalWidth(), transportH + plateH);
    startTimerHz (timerHz);
}

TapeSaturatorAudioProcessorEditor::~TapeSaturatorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TapeSaturatorAudioProcessorEditor::addKnob (Knob& k, const char* paramID, const juce::String& caption)
{
    k.caption = caption;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (215.0f), juce::degreesToRadians (505.0f), true);
    k.slider.setPopupDisplayEnabled (true, true, this, 1500);
    k.slider.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, k.slider);

    if (auto* param = processor.apvts.getParameter (paramID))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    captions.emplace_back (&k.slider, caption);
}

void TapeSaturatorAudioProcessorEditor::addSwitch (Switch& s, const char* paramID, const juce::String& caption,
                                                   const juce::String& labels)
{
    s.caption = caption;
    s.button.getProperties().set ("switchLabels", labels);
    s.button.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (s.button);
    s.attachment = std::make_unique<ButtonAttachment> (processor.apvts, paramID, s.button);
    captions.emplace_back (&s.button, caption);
}

void TapeSaturatorAudioProcessorEditor::timerCallback()
{
    const float dt = 1.0f / (float) timerHz;

    inputVU.update (processor.getInputLevel(), dt);
    outputVU.update (processor.getOutputLevel(), dt);

    // Spulen drehen sich nur, wenn der Host tatsächlich Audio verarbeitet
    const auto count = processor.getProcessedBlockCount();
    idleTicks = (count != lastBlockCount) ? 0 : idleTicks + 1;
    lastBlockCount = count;

    if (idleTicks < timerHz / 3)
    {
        const bool fast = speedSwitch.button.getToggleState();
        const float revPerSec = fast ? 0.9f : 0.45f;
        reelAngle += juce::MathConstants<float>::twoPi * revPerSec * dt;
        if (reelAngle > juce::MathConstants<float>::twoPi * 1000.0f)
            reelAngle = 0.0f;

        // Kleiner Wickel dreht sich schneller (gleiche Bandgeschwindigkeit)
        leftReel.setAngle (reelAngle * 0.55f / leftReel.getPackRatio());
        rightReel.setAngle (reelAngle * 0.55f / rightReel.getPackRatio());
    }

    if (autoGainSwitch.button.getToggleState() != lampOn)
    {
        lampOn = autoGainSwitch.button.getToggleState();
        repaint (lampBounds.expanded (24.0f).toNearestInt());
    }
}

void TapeSaturatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintTransport (g, transportArea.toFloat());
    paintFaceplate (g, plateArea.toFloat());
}

void TapeSaturatorAudioProcessorEditor::paintTransport (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Dunkles, leicht strukturiertes Laufwerkspanel
    g.setGradientFill (juce::ColourGradient (TapeColours::machineHi, area.getX(), area.getY(),
                                             TapeColours::machine, area.getX(), area.getBottom(), false));
    g.fillRect (area);

    juce::Random rng (42);
    for (int i = 0; i < 2500; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.02f + 0.02f * rng.nextFloat()));
        g.fillRect (area.getX() + rng.nextFloat() * area.getWidth(), area.getY() + rng.nextFloat() * area.getHeight(), 1.0f, 1.0f);
    }

    // Typenschild
    const auto plate = juce::Rectangle<float> (300.0f, 40.0f).withCentre ({ area.getCentreX(), area.getY() + 32.0f });
    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (TapeColours::amber.withAlpha (0.6f));
    g.drawRoundedRectangle (plate.reduced (2.5f), 3.0f, 0.8f);
    g.setColour (TapeColours::cream);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("TAPESATURATOR", plate.withTrimmedBottom (13.0f).translated (0.0f, 3.0f), juce::Justification::centred);
    g.setColour (TapeColours::amber);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("TS-1  \xc2\xb7  STUDIO MASTERING RECORDER")),
                plate.withTrimmedTop (24.0f), juce::Justification::centred);

    // Bandweg: Wickel links -> Umlenkrolle -> Köpfe -> Umlenkrolle -> Wickel rechts
    const auto lc = leftReel.getBounds().toFloat().getCentre();
    const auto rc = rightReel.getBounds().toFloat().getCentre();
    const float lR = (float) leftReel.getWidth() * 0.5f - 2.0f, rR = (float) rightReel.getWidth() * 0.5f - 2.0f;
    const float tapeY = area.getBottom() - 30.0f;
    const float rollerR = 11.0f;
    const juce::Point<float> rollerL ((float) inputVU.getX() + 6.0f, tapeY - rollerR);
    const juce::Point<float> rollerR2 ((float) outputVU.getRight() - 6.0f, tapeY - rollerR);

    juce::Path tape;
    tape.startNewSubPath (lc.x - 4.0f, lc.y + lR * leftReel.getPackRatio());
    tape.lineTo (rollerL.x - 2.0f, tapeY);
    tape.lineTo (rollerR2.x + 2.0f, tapeY);
    tape.lineTo (rc.x + 4.0f, rc.y + rR * rightReel.getPackRatio());
    g.setColour (TapeColours::tape);
    g.strokePath (tape, juce::PathStrokeType (3.0f));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (tape, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.0f, -1.0f));

    // Umlenkrollen
    for (auto pt : { rollerL, rollerR2 })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2f3), pt.x - rollerR, pt.y - rollerR,
                                                 juce::Colour (0xff60656a), pt.x + rollerR, pt.y + rollerR, false));
        g.fillEllipse (pt.x - rollerR, pt.y - rollerR, 2.0f * rollerR, 2.0f * rollerR);
        g.setColour (juce::Colour (0xff222222));
        g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
    }

    // Kopfträger: Löschen / Aufnahme / Wiedergabe
    const juce::StringArray headNames { "ERASE", "RECORD", "PLAY" };
    const float headW = 30.0f, headH = 26.0f, spacing = 70.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float hx = area.getCentreX() + (float) (i - 1) * spacing;
        auto head = juce::Rectangle<float> (headW, headH).withCentre ({ hx, tapeY - headH * 0.5f + 1.5f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f4f5), head.getX(), head.getY(),
                                                 juce::Colour (0xff5d6267), head.getRight(), head.getBottom(), false));
        g.fillRoundedRectangle (head, 5.0f);
        g.setColour (juce::Colour (0xff2b2e31));
        g.drawRoundedRectangle (head, 5.0f, 0.8f);
        g.fillRect (head.getCentreX() - 0.5f, head.getBottom() - 6.0f, 1.0f, 6.0f); // Kopfspalt

        g.setColour (TapeColours::cream.withAlpha (0.65f));
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText (headNames[i], juce::Rectangle<float> (70.0f, 12.0f).withCentre ({ hx, tapeY + 13.0f }),
                    juce::Justification::centred);
    }
}

void TapeSaturatorAudioProcessorEditor::paintFaceplate (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (brushedMetal.isValid())
        g.drawImage (brushedMetal, area);

    // Trennkante zum Laufwerk
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area.withHeight (2.0f));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.fillRect (area.withHeight (1.0f).translated (0.0f, 2.0f));

    // Sektionen: gravierte Rahmen mit Titel
    for (const auto& s : sections)
    {
        const auto b = s.bounds.toFloat();
        const float titleW = (float) s.title.length() * 8.5f + 18.0f;
        const auto titleR = juce::Rectangle<float> (titleW, 16.0f).withCentre ({ b.getCentreX(), b.getY() });

        juce::Path frame;
        frame.addRoundedRectangle (b, 6.0f);

        juce::Graphics::ScopedSaveState state (g);
        g.excludeClipRegion (titleR.toNearestInt());
        g.setColour (TapeColours::engraveHi);
        g.strokePath (frame, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (TapeColours::engraving.withAlpha (0.8f));
        g.strokePath (frame, juce::PathStrokeType (1.0f));
    }
    for (const auto& s : sections)
    {
        const auto b = s.bounds.toFloat();
        TapeLookAndFeel::drawEngravedText (g, s.title, juce::Rectangle<float> (200.0f, 16.0f).withCentre ({ b.getCentreX(), b.getY() }),
                                           juce::Justification::centred, 12.5f);
    }

    // Beschriftungen über den Bedienelementen
    for (const auto& [comp, text] : captions)
    {
        const auto cb = comp->getBounds().toFloat();
        TapeLookAndFeel::drawEngravedText (g, text, juce::Rectangle<float> ((float) colW + 10.0f, (float) captionH)
                                                       .withCentre ({ cb.getCentreX(), cb.getY() - captionH * 0.5f }),
                                           juce::Justification::centred, 10.5f);
    }

    // Kontrolllampe Auto Gain
    {
        const auto c = lampBounds.getCentre();
        const float r = lampBounds.getWidth() * 0.5f;
        g.setColour (juce::Colour (0xff3c3f42));
        g.fillEllipse (lampBounds.expanded (3.0f));
        if (lampOn)
        {
            g.setGradientFill (juce::ColourGradient (TapeColours::lampOn.withAlpha (0.55f), c.x, c.y,
                                                     TapeColours::lampOn.withAlpha (0.0f), c.x + r * 2.6f, c.y, true));
            g.fillEllipse (lampBounds.expanded (r * 1.6f));
        }
        g.setGradientFill (juce::ColourGradient (lampOn ? juce::Colour (0xfffff1c2) : TapeColours::lampOff.brighter (0.3f),
                                                 c.x - r * 0.3f, c.y - r * 0.3f,
                                                 lampOn ? TapeColours::lampOn : TapeColours::lampOff, c.x + r, c.y + r, true));
        g.fillEllipse (lampBounds);
    }

    // Schrauben in den Ecken
    for (auto pt : { area.getTopLeft().translated (10.0f, 14.0f), area.getTopRight().translated (-10.0f, 14.0f),
                     area.getBottomLeft().translated (10.0f, -11.0f), area.getBottomRight().translated (-10.0f, -11.0f) })
        TapeLookAndFeel::drawScrew (g, pt, 5.0f, 0.8f);
}

void TapeSaturatorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    transportArea = bounds.removeFromTop (transportH);
    plateArea = bounds;

    brushedMetal = TapeLookAndFeel::createBrushedMetal (plateArea.getWidth(), plateArea.getHeight(), 2.0f);

    // --- Laufwerk: Spulen außen, VU-Meter in der Mitte ---
    const int reelSize = transportH - 40;
    leftReel.setBounds (outerMargin, 12, reelSize, reelSize);
    rightReel.setBounds (getWidth() - outerMargin - reelSize, 12, reelSize, reelSize);

    const int vuW = 196, vuH = 132, vuGap = 16;
    const int vuY = 70;
    const int cx = getWidth() / 2;
    inputVU.setBounds (cx - vuGap / 2 - vuW, vuY, vuW, vuH);
    outputVU.setBounds (cx + vuGap / 2, vuY, vuW, vuH);

    // --- Frontplatte ---
    auto area = plateArea.reduced (outerMargin + 8, 0);
    area.removeFromTop (28);
    area.removeFromBottom (24);

    std::array<juce::Component*, 12> controls {
        &input.slider, &saturation.slider, &bias.slider, &speedSwitch.button,
        &flutter.slider, &noise.slider,
        &delayTime.slider, &delayFb.slider, &delayMix.slider,
        &output.slider, &mix.slider, &autoGainSwitch.button
    };

    const int sectionsW = totalWidth() - 2 * outerMargin;
    area = area.withSizeKeepingCentre (sectionsW, area.getHeight());

    size_t idx = 0;
    for (size_t s = 0; s < sections.size(); ++s)
    {
        const int cols = sectionColumns[s];
        sections[s].bounds = area.removeFromLeft (cols * colW + 2 * sectionPad);
        area.removeFromLeft (sectionGap);

        auto inner = sections[s].bounds.reduced (sectionPad, 0);
        inner.removeFromTop (14 + captionH);

        for (int c = 0; c < cols; ++c, ++idx)
        {
            auto cell = inner.removeFromLeft (colW);
            auto* comp = controls[idx];
            if (dynamic_cast<juce::ToggleButton*> (comp) != nullptr)
                comp->setBounds (cell.withSizeKeepingCentre (52, 100).withY (cell.getY() + 8));
            else
                comp->setBounds (cell.withSizeKeepingCentre (knobSize, knobSize).withY (cell.getY() + 8));
        }
    }

    // Lampe rechts neben dem Auto-Gain-Schalter
    const auto sw = autoGainSwitch.button.getBounds().toFloat();
    lampBounds = juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ sw.getRight() + 6.0f, sw.getCentreY() });
}
