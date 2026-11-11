#include "TapeMachine.h"
#include "Fonts.h"
#include "../Parameters.h"

namespace
{
    constexpr int woodW         = 26;
    constexpr int transportH    = 300;
    constexpr int colW          = 80;
    constexpr int sectionPad    = 12;
    constexpr int sectionGap    = 12;
    constexpr int innerMargin   = 18;
    constexpr int captionH      = 18;
    constexpr int knobSize      = 80;
    constexpr int reelSize      = 268;

    constexpr std::array<int, 4> sectionColumns { 4, 2, 3, 3 };

    constexpr float packMin = 0.36f;    // leere Spule (Nabe)
    constexpr float packMax = 0.93f;    // volle Spule
    constexpr float fullReelSeconds = 900.0f; // Spielzeit einer Spule bei 15 ips (optisch)
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    float packForAmount (float amount) // amount 0..1 = Anteil des Bandes auf der Spule
    {
        return std::sqrt (packMin * packMin + (packMax * packMax - packMin * packMin) * juce::jlimit (0.0f, 1.0f, amount));
    }

    /** Tangentenpunkt von Punkt p an Kreis (c, r); wählt den unteren der beiden Punkte. */
    juce::Point<float> lowerTangent (juce::Point<float> c, float r, juce::Point<float> p)
    {
        const auto d = p - c;
        const float dist = d.getDistanceFromOrigin();
        if (dist <= r)
            return c + d * (r / juce::jmax (1.0f, dist));

        const float base = std::atan2 (d.y, d.x);
        const float off = std::acos (r / dist);
        const juce::Point<float> t1 (c.x + r * std::cos (base + off), c.y + r * std::sin (base + off));
        const juce::Point<float> t2 (c.x + r * std::cos (base - off), c.y + r * std::sin (base - off));
        return t1.y > t2.y ? t1 : t2;
    }

    void chromeCircle (juce::Graphics& g, juce::Point<float> c, float r)
    {
        juce::Path p;
        p.addEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 5, { 2, 3 }).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff5f6f7), c.x - r, c.y - r,
                                                 juce::Colour (0xff595e63), c.x + r, c.y + r, false));
        g.fillPath (p);
        g.setColour (juce::Colour (0xff2c2f32));
        g.strokePath (p, juce::PathStrokeType (0.7f));
    }
}

juce::Point<float> TapeMachine::Arm::rollerCentre() const
{
    constexpr float length = 56.0f;
    return { pivot.x + length * std::sin (angle), pivot.y - length * std::cos (angle) };
}

//==============================================================================
TapeMachine::TapeMachine (juce::AudioProcessorValueTreeState& s) : state (s)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    addAndMakeVisible (leftReel);
    addAndMakeVisible (rightReel);
    addAndMakeVisible (inputVU);
    addAndMakeVisible (outputVU);

    addKnob (input,      ParamIDs::input,      "INPUT");
    addKnob (saturation, ParamIDs::saturation, "SATURATION");
    addKnob (bias,       ParamIDs::bias,       "BIAS");
    addSwitch (speedSwitch, ParamIDs::speed,   "SPEED", "15 IPS|7.5 IPS");
    addKnob (flutter,    ParamIDs::flutter,    "WOW / FLUTTER");
    addKnob (noise,      ParamIDs::noise,      "NOISE");
    addKnob (delayTime,  ParamIDs::delayTime,  "TIME");
    addKnob (delayFb,    ParamIDs::delayFb,    "FEEDBACK");
    addKnob (delayMix,   ParamIDs::delayMix,   "MIX");
    addKnob (output,     ParamIDs::output,     "OUTPUT");
    addKnob (mix,        ParamIDs::mix,        "DRY / WET");
    addSwitch (autoGainSwitch, ParamIDs::autoGain, "AUTO GAIN", "ON|OFF");

    sections = { Section { "TAPE", {} }, Section { "TRANSPORT", {} },
                 Section { "SLAP DELAY", {} }, Section { "OUTPUT", {} } };

    lampOn = autoGainSwitch.button.getToggleState();
    setSize (kBaseWidth, kBaseHeight);
    updateReels();
}

TapeMachine::~TapeMachine()
{
    setLookAndFeel (nullptr);
}

void TapeMachine::addKnob (Knob& k, const char* paramID, const juce::String& caption)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (215.0f), juce::degreesToRadians (505.0f), true);
    k.slider.setPopupDisplayEnabled (true, true, this, 1500);
    k.slider.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    k.slider.setBufferedToImage (false);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<SliderAttachment> (state, paramID, k.slider);

    if (auto* param = state.getParameter (paramID))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    captions.emplace_back (&k.slider, caption);
}

void TapeMachine::addSwitch (Switch& s, const char* paramID, const juce::String& caption, const juce::String& labels)
{
    s.button.getProperties().set ("switchLabels", labels);
    s.button.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (s.button);
    s.attachment = std::make_unique<ButtonAttachment> (state, paramID, s.button);
    captions.emplace_back (&s.button, caption);
}

//==============================================================================
void TapeMachine::tick (float dt, bool audioRunning, float inputRms, float outputRms)
{
    time += dt;
    const bool fast = speedSwitch.button.getToggleState();
    const float flutterAmt = (float) flutter.slider.getValue();

    // Anlaufen / Auslaufen der Spulen (Trägheit)
    const float target = audioRunning ? (fast ? 1.0f : 0.5f) : 0.0f;
    const float prevSpeed = tapeSpeed;
    tapeSpeed += (target - tapeSpeed) * (1.0f - std::exp (-dt / 0.45f));
    if (tapeSpeed < 1.0e-3f && ! audioRunning)
        tapeSpeed = 0.0f;
    const float accel = (tapeSpeed - prevSpeed) / juce::jmax (1.0e-3f, dt);

    // Band wandert von links nach rechts; danach automatisch "zurückgespult"
    tapePosition += tapeSpeed * dt / fullReelSeconds;
    if (tapePosition > 0.97f)
        tapePosition = 0.03f;

    // Winkelgeschwindigkeit ~ Bandgeschwindigkeit / Wickelradius
    const float k = twoPi * 0.5f;
    leftAngle  += tapeSpeed * k / packForAmount (1.0f - tapePosition) * dt;
    rightAngle += tapeSpeed * k / packForAmount (tapePosition) * dt;
    leftAngle  = std::fmod (leftAngle, twoPi * 100.0f);
    rightAngle = std::fmod (rightAngle, twoPi * 100.0f);
    updateReels();

    // Bandzugarme: federn beim Anlaufen/Bremsen, zittern leicht mit Wow/Flutter
    for (auto* arm : { &leftArm, &rightArm })
    {
        const float side = arm == &leftArm ? 1.0f : -1.0f;
        const float wobble = tapeSpeed * (0.012f + 0.05f * flutterAmt)
                             * (std::sin (twoPi * 6.3f * time) + 0.5f * std::sin (twoPi * 0.55f * time + 1.0f))
                             + tapeSpeed * 0.004f * (rng.nextFloat() - 0.5f);
        const float armTarget = arm->restAngle + side * juce::jlimit (-0.35f, 0.35f, accel * 0.18f) + side * wobble;

        constexpr float omega = 18.0f, zeta = 0.35f; // leicht unterdämpft = federnd
        const float acc = omega * omega * (armTarget - arm->angle) - 2.0f * zeta * omega * arm->velocity;
        arm->velocity += acc * dt;
        arm->angle += arm->velocity * dt;
    }

    inputVU.update (inputRms, dt, 0.3f + flutterAmt);
    outputVU.update (outputRms, dt, 0.3f + flutterAmt);

    if (autoGainSwitch.button.getToggleState() != lampOn)
    {
        lampOn = autoGainSwitch.button.getToggleState();
        repaint (lampBounds.expanded (24.0f).toNearestInt());
    }

    // Bandweg + Arme neu zeichnen, solange sich etwas bewegt
    if (tapeSpeed > 0.0f || std::abs (leftArm.velocity) > 1.0e-3f || std::abs (rightArm.velocity) > 1.0e-3f)
        repaint (transportArea.withTrimmedTop (120));
}

void TapeMachine::updateReels()
{
    leftReel.setState (leftAngle, packForAmount (1.0f - tapePosition));
    rightReel.setState (rightAngle, packForAmount (tapePosition));
}

void TapeMachine::setDemoState (float pos, float inNeedle, float outNeedle)
{
    tapePosition = pos;
    leftAngle = 0.4f;
    rightAngle = 1.3f;
    updateReels();
    inputVU.setNeedlePosition (inNeedle);
    outputVU.setNeedlePosition (outNeedle);
    repaint();
}

//==============================================================================
void TapeMachine::resized()
{
    const int w = kBaseWidth, h = kBaseHeight;
    woodLeft  = { 0, 0, woodW, h };
    woodRight = { w - woodW, 0, woodW, h };
    transportArea = { woodW, 0, w - 2 * woodW, transportH };
    plateArea = { woodW, transportH, w - 2 * woodW, h - transportH };

    const int cx = w / 2;
    tapeY = (float) transportH - 30.0f;

    // Spulen
    leftReel.setBounds (woodW + 4, 6, reelSize, reelSize);
    rightReel.setBounds (w - woodW - 4 - reelSize, 6, reelSize, reelSize);

    // VU-Meter
    const int vuW = 200, vuH = 136, vuGap = 14, vuY = 70;
    inputVU.setBounds (cx - vuGap / 2 - vuW, vuY, vuW, vuH);
    outputVU.setBounds (cx + vuGap / 2, vuY, vuW, vuH);

    // Bandzugarme zwischen Spule und VU-Meter
    const float armX = ((float) leftReel.getRight() + (float) inputVU.getX()) * 0.5f;
    leftArm.pivot  = { armX + 14.0f, tapeY + 14.0f };
    rightArm.pivot = { (float) w - armX - 14.0f, tapeY + 14.0f };
    leftArm.restAngle  = -0.42f;
    rightArm.restAngle =  0.42f;
    leftArm.angle  = leftArm.restAngle;
    rightArm.angle = rightArm.restAngle;

    // Frontplatte
    auto area = plateArea.reduced (innerMargin, 0);
    area.removeFromTop (30);
    area.removeFromBottom (24);

    std::array<juce::Component*, 12> controls {
        &input.slider, &saturation.slider, &bias.slider, &speedSwitch.button,
        &flutter.slider, &noise.slider,
        &delayTime.slider, &delayFb.slider, &delayMix.slider,
        &output.slider, &mix.slider, &autoGainSwitch.button
    };

    int sectionsW = 3 * sectionGap;
    for (int c : sectionColumns)
        sectionsW += c * colW + 2 * sectionPad;
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

    const auto sw = autoGainSwitch.button.getBounds().toFloat();
    lampBounds = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ sw.getRight() + 6.0f, sw.getCentreY() });

    cachedScale = 0.0f; // Caches neu erzeugen
}

//==============================================================================
void TapeMachine::rebuildCaches (float scale)
{
    cachedScale = scale;

    auto render = [scale] (juce::Rectangle<int> area, auto&& drawFn)
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, (int) std::ceil (area.getWidth() * scale)),
                         juce::jmax (1, (int) std::ceil (area.getHeight() * scale)), true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY()).scaled (scale));
        drawFn (g);
        return img;
    };

    transportCache = render (transportArea, [this] (juce::Graphics& g) { paintTransportStatic (g); });
    plateCache     = render (plateArea,     [this] (juce::Graphics& g) { paintPlateStatic (g); });
    woodLeftCache  = TapeLookAndFeel::createWoodPanel (woodLeft.getWidth(), woodLeft.getHeight(), scale, 11);
    woodRightCache = TapeLookAndFeel::createWoodPanel (woodRight.getWidth(), woodRight.getHeight(), scale, 29);
}

void TapeMachine::paint (juce::Graphics& g)
{
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (scale - cachedScale) > 0.01f)
        rebuildCaches (scale);

    g.drawImage (transportCache, transportArea.toFloat());
    g.drawImage (plateCache, plateArea.toFloat());
    g.drawImage (woodLeftCache, woodLeft.toFloat());
    g.drawImage (woodRightCache, woodRight.toFloat());

    paintTapePath (g);
    paintLamp (g);
}

void TapeMachine::paintTransportStatic (juce::Graphics& g)
{
    const auto area = transportArea.toFloat();

    // Pulverbeschichtetes Laufwerkspanel
    g.setGradientFill (juce::ColourGradient (TapeColours::machineHi, area.getX(), area.getY(),
                                             TapeColours::machine, area.getX(), area.getBottom(), false));
    g.fillRect (area);

    juce::Random grain (42);
    for (int i = 0; i < (int) (area.getWidth() * area.getHeight() / 30.0f); ++i)
    {
        g.setColour ((grain.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
        g.fillRect (area.getX() + grain.nextFloat() * area.getWidth(), area.getY() + grain.nextFloat() * area.getHeight(), 1.0f, 1.0f);
    }

    // Licht von oben links, Vignette
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), area.getX() + area.getWidth() * 0.3f, area.getY(),
                                             juce::Colours::transparentWhite, area.getCentreX(), area.getBottom(), true));
    g.fillRect (area);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, area.getCentreX(), area.getCentreY(),
                                             juce::Colours::black.withAlpha (0.45f), area.getX(), area.getY(), true));
    g.fillRect (area);

    // Lichtschein der VU-Lampen auf dem Panel
    for (auto* vu : { &inputVU, &outputVU })
    {
        const auto b = vu->getBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (TapeColours::amber.withAlpha (0.16f), b.getCentreX(), b.getCentreY(),
                                                 TapeColours::amber.withAlpha (0.0f), b.getCentreX() + b.getWidth() * 0.75f, b.getCentreY(), true));
        g.fillEllipse (b.expanded (b.getWidth() * 0.3f, b.getHeight() * 0.35f));
    }

    // Typenschild: gegossenes Emblem mit Emaille
    {
        const auto badge = juce::Rectangle<float> (330.0f, 46.0f).withCentre ({ area.getCentreX(), area.getY() + 34.0f });
        juce::Path badgePath;
        badgePath.addRoundedRectangle (badge, 10.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 8, { 2, 4 }).drawForPath (g, badgePath);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f7f8), badge.getX(), badge.getY(),
                                                 juce::Colour (0xff4e5357), badge.getRight(), badge.getBottom(), false));
        g.fillPath (badgePath);

        const auto enamel = badge.reduced (4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1a17), enamel.getX(), enamel.getY(),
                                                 juce::Colour (0xff070707), enamel.getX(), enamel.getBottom(), false));
        g.fillRoundedRectangle (enamel, 7.0f);
        juce::Random pebble (5);
        for (int i = 0; i < 700; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (enamel.getX() + pebble.nextFloat() * enamel.getWidth(), enamel.getY() + pebble.nextFloat() * enamel.getHeight(), 1.0f, 1.0f);
        }

        // erhabene Chrom-Schrift
        const auto textArea = enamel.withTrimmedBottom (12.0f).translated (0.0f, 2.0f);
        g.setFont (TapeFonts::emblem (24.0f));
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.drawText ("TapeSaturator", textArea.translated (1.0f, 1.5f), juce::Justification::centred);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffffaf0), textArea.getX(), textArea.getY() + 4.0f,
                                                 juce::Colour (0xffb9a37c), textArea.getX(), textArea.getBottom() - 2.0f, false));
        g.drawText ("TapeSaturator", textArea, juce::Justification::centred);

        g.setColour (TapeColours::amber.withAlpha (0.9f));
        g.setFont (TapeFonts::label (11.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("MODEL TS-1  \xc2\xb7  STUDIO MASTERING RECORDER")),
                    enamel.withTrimmedTop (enamel.getHeight() - 14.0f).translated (0.0f, -2.0f), juce::Justification::centred);
    }

    // Kopfträger-Grundplatte
    const float cx = area.getCentreX();
    {
        auto plate = juce::Rectangle<float> (cx - 118.0f, tapeY - 40.0f, 270.0f, 63.0f);
        juce::Path p;
        p.addRoundedRectangle (plate, 6.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 1, 3 }).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3e42), plate.getX(), plate.getY(),
                                                 juce::Colour (0xff1c1e20), plate.getX(), plate.getBottom(), false));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) plate.getY() + 1, plate.getX() + 6.0f, plate.getRight() - 6.0f);

        for (auto [name, hx] : { std::pair<const char*, float> { "ERASE", cx - 70.0f }, { "RECORD", cx }, { "PLAY", cx + 70.0f } })
        {
            g.setColour (TapeColours::cream.withAlpha (0.55f));
            g.setFont (TapeFonts::label (10.0f));
            g.drawText (name, juce::Rectangle<float> (70.0f, 12.0f).withCentre ({ hx, tapeY + 10.0f }), juce::Justification::centred);
        }
    }

    // Chromleiste zur Frontplatte
    auto trim = area.withTop (area.getBottom() - 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f1), trim.getX(), trim.getY(),
                                             juce::Colour (0xff5f6367), trim.getX(), trim.getBottom(), false));
    g.fillRect (trim);

    for (auto pt : { area.getTopLeft().translated (14.0f, 14.0f), area.getTopRight().translated (-14.0f, 14.0f) })
        TapeLookAndFeel::drawScrew (g, pt, 5.0f, 0.4f);
}

void TapeMachine::paintPlateStatic (juce::Graphics& g)
{
    const auto area = plateArea.toFloat();
    const auto metal = TapeLookAndFeel::createBrushedMetal (plateArea.getWidth(), plateArea.getHeight(), cachedScale);
    g.drawImage (metal, area);

    // Fase oben
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (area.withHeight (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillRect (area.withHeight (1.0f).translated (0.0f, 1.0f));

    // Sektionen: vertiefte Gravur-Rahmen
    for (const auto& s : sections)
    {
        const auto b = s.bounds.toFloat();
        const float titleW = (float) s.title.length() * 8.0f + 22.0f;
        const auto titleR = juce::Rectangle<float> (titleW, 16.0f).withCentre ({ b.getCentreX(), b.getY() });

        juce::Path frame;
        frame.addRoundedRectangle (b, 7.0f);

        juce::Graphics::ScopedSaveState st (g);
        g.excludeClipRegion (titleR.toNearestInt());
        g.setColour (TapeColours::engraveHi);
        g.strokePath (frame, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.6f, 1.0f));
        g.setColour (TapeColours::engraving.withAlpha (0.75f));
        g.strokePath (frame, juce::PathStrokeType (1.0f));
    }
    for (const auto& s : sections)
    {
        const auto b = s.bounds.toFloat();
        TapeLookAndFeel::drawEngravedText (g, s.title, juce::Rectangle<float> (200.0f, 18.0f).withCentre ({ b.getCentreX(), b.getY() }),
                                           juce::Justification::centred, 15.0f);
    }

    for (const auto& [comp, text] : captions)
    {
        const auto cb = comp->getBounds().toFloat();
        TapeLookAndFeel::drawEngravedText (g, text, juce::Rectangle<float> ((float) colW + 16.0f, (float) captionH)
                                                       .withCentre ({ cb.getCentreX(), cb.getY() - captionH * 0.5f }),
                                           juce::Justification::centred, 13.5f);
    }

    // Lampenfassung
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f2f3), lampBounds.getX() - 4.0f, lampBounds.getY() - 4.0f,
                                             juce::Colour (0xff4d5155), lampBounds.getRight() + 4.0f, lampBounds.getBottom() + 4.0f, false));
    g.fillEllipse (lampBounds.expanded (4.0f));
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (lampBounds.expanded (1.0f));

    for (auto pt : { area.getTopLeft().translated (12.0f, 14.0f), area.getTopRight().translated (-12.0f, 14.0f),
                     area.getBottomLeft().translated (12.0f, -12.0f), area.getBottomRight().translated (-12.0f, -12.0f) })
        TapeLookAndFeel::drawScrew (g, pt, 5.5f, 0.8f);
}

void TapeMachine::paintTapePath (juce::Graphics& g)
{
    const float cx = (float) kBaseWidth * 0.5f;
    const auto lc = leftReel.getBounds().toFloat().getCentre();
    const auto rc = rightReel.getBounds().toFloat().getCentre();
    const float lPack = leftReel.getFlangeRadius() * leftReel.getPackRatio();
    const float rPack = rightReel.getFlangeRadius() * rightReel.getPackRatio();

    const auto lRoller = leftArm.rollerCentre();
    const auto rRoller = rightArm.rollerCentre();
    constexpr float tensionR = 9.0f, guideR = 11.0f;
    const juce::Point<float> lGuide ((float) inputVU.getX() - 4.0f, tapeY - guideR);
    const juce::Point<float> rGuide ((float) outputVU.getRight() + 4.0f, tapeY - guideR);

    // Bandweg
    juce::Path tape;
    tape.startNewSubPath (lowerTangent (lc, lPack, lRoller.translated (-tensionR, 0.0f)));
    tape.lineTo (lRoller.translated (-tensionR * 0.7f, tensionR * 0.7f));
    tape.lineTo (lGuide.translated (0.0f, guideR));
    tape.lineTo (rGuide.translated (0.0f, guideR));
    tape.lineTo (rRoller.translated (tensionR * 0.7f, tensionR * 0.7f));
    tape.lineTo (lowerTangent (rc, rPack, rRoller.translated (tensionR, 0.0f)));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (tape, juce::PathStrokeType (3.0f), juce::AffineTransform::translation (1.5f, 2.5f));
    g.setColour (TapeColours::tape);
    g.strokePath (tape, juce::PathStrokeType (3.0f));
    g.setColour (juce::Colour (0xff8a6a4f).withAlpha (0.45f));
    g.strokePath (tape, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (0.0f, -1.0f));

    // Bandzugarme
    for (auto* arm : { &leftArm, &rightArm })
    {
        const auto roller = arm->rollerCentre();
        juce::Path rod;
        rod.startNewSubPath (arm->pivot);
        rod.lineTo (roller);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.strokePath (rod, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (2.0f, 3.0f));
        g.setColour (juce::Colour (0xffb9bdc1));
        g.strokePath (rod, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.strokePath (rod, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (-1.0f, -1.0f));

        chromeCircle (g, arm->pivot, 7.0f);
        chromeCircle (g, roller, tensionR);
        g.setColour (juce::Colour (0xff222222));
        g.fillEllipse (roller.x - 2.5f, roller.y - 2.5f, 5.0f, 5.0f);
    }

    // Umlenkrollen
    for (auto pt : { lGuide, rGuide })
    {
        chromeCircle (g, pt, guideR);
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
    }

    // Köpfe (liegen auf dem Band)
    for (float hx : { cx - 70.0f, cx, cx + 70.0f })
    {
        auto head = juce::Rectangle<float> (30.0f, 26.0f).withCentre ({ hx, tapeY - 11.0f });
        juce::Path p;
        p.addRoundedRectangle (head, 5.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 5, { 1, 2 }).drawForPath (g, p);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff5f6f7), head.getX(), head.getY(),
                                                 juce::Colour (0xff595e63), head.getRight(), head.getBottom(), false));
        g.fillPath (p);
        g.setColour (juce::Colour (0xff2b2e31));
        g.strokePath (p, juce::PathStrokeType (0.8f));
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRect (head.getCentreX() - 0.5f, head.getBottom() - 7.0f, 1.0f, 7.0f);
    }

    // Capstan + Andruckrolle rechts neben dem Kopfträger
    {
        const juce::Point<float> pinch (cx + 122.0f, tapeY + 11.5f);
        juce::Path rubber;
        rubber.addEllipse (pinch.x - 10.0f, pinch.y - 10.0f, 20.0f, 20.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 5, { 1, 3 }).drawForPath (g, rubber);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c3c), pinch.x - 10.0f, pinch.y - 10.0f,
                                                 juce::Colour (0xff080808), pinch.x + 10.0f, pinch.y + 10.0f, false));
        g.fillPath (rubber);
        chromeCircle (g, pinch, 4.5f);
        chromeCircle (g, { pinch.x, tapeY - 4.0f }, 4.0f);
    }
}

void TapeMachine::paintLamp (juce::Graphics& g)
{
    const auto c = lampBounds.getCentre();
    const float r = lampBounds.getWidth() * 0.5f;
    if (lampOn)
    {
        g.setGradientFill (juce::ColourGradient (TapeColours::lampOn.withAlpha (0.55f), c.x, c.y,
                                                 TapeColours::lampOn.withAlpha (0.0f), c.x + r * 3.0f, c.y, true));
        g.fillEllipse (lampBounds.expanded (r * 2.0f));
    }
    g.setGradientFill (juce::ColourGradient (lampOn ? juce::Colour (0xfffff3c8) : TapeColours::lampOff.brighter (0.35f),
                                             c.x - r * 0.3f, c.y - r * 0.3f,
                                             lampOn ? TapeColours::lampOn : TapeColours::lampOff.darker (0.3f), c.x + r, c.y + r, true));
    g.fillEllipse (lampBounds);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.5f, r * 0.4f);
}
