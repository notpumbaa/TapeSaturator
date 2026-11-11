#include "PluginEditor.h"

namespace
{
    constexpr int headerHeight = 54;
    constexpr int margin       = 14;
    constexpr int knobW        = 92;
    constexpr int knobH        = 118;
}

TapeSaturatorAudioProcessorEditor::TapeSaturatorAudioProcessorEditor (TapeSaturatorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    addKnob (input,      ParamIDs::input,      "INPUT");
    addKnob (saturation, ParamIDs::saturation, "SATURATION");
    addKnob (bias,       ParamIDs::bias,       "BIAS");
    addKnob (flutter,    ParamIDs::flutter,    "WOW/FLUTTER");
    addKnob (noise,      ParamIDs::noise,      "NOISE");
    addKnob (delayTime,  ParamIDs::delayTime,  "TIME");
    addKnob (delayFb,    ParamIDs::delayFb,    "FEEDBACK");
    addKnob (delayMix,   ParamIDs::delayMix,   "DELAY MIX");
    addKnob (output,     ParamIDs::output,     "OUTPUT");
    addKnob (mix,        ParamIDs::mix,        "DRY/WET");

    speedBox.addItemList (juce::StringArray { "7.5 ips", "15 ips" }, 1);
    addAndMakeVisible (speedBox);
    speedAttachment = std::make_unique<ComboAttachment> (processor.apvts, ParamIDs::speed, speedBox);

    speedLabel.setText ("SPEED", juce::dontSendNotification);
    speedLabel.setJustificationType (juce::Justification::centred);
    speedLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    speedLabel.setColour (juce::Label::textColourId, TapeColours::creamDim);
    addAndMakeVisible (speedLabel);

    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamIDs::autoGain, autoGainButton);

    sections = { Section { "TAPE", {} }, Section { "TRANSPORT", {} },
                 Section { "SLAP DELAY", {} }, Section { "OUTPUT", {} } };

    // 11 Knopf-Spalten in 4 Sektionen (je 16 px Innenrand), Abstände dazwischen
    const int width  = 11 * knobW + 4 * 16 + 5 * margin;
    const int height = headerHeight + 30 + knobH + 52 + margin;
    setSize (width, height);
}

TapeSaturatorAudioProcessorEditor::~TapeSaturatorAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TapeSaturatorAudioProcessorEditor::addKnob (Knob& k, const char* paramID, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, TapeColours::cream);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 8, 18);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, TapeColours::creamDim);
    addAndMakeVisible (k.label);
}

void TapeSaturatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TapeColours::background);

    // Kopfzeile
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);
    g.setColour (TapeColours::cream);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("TAPESATURATOR", header, juce::Justification::centredLeft);
    g.setColour (TapeColours::accent);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("STUDIO TAPE MACHINE", header, juce::Justification::centredRight);

    // Sektionen
    for (const auto& s : sections)
    {
        g.setColour (TapeColours::panel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 8.0f);
        g.setColour (TapeColours::panelEdge);
        g.drawRoundedRectangle (s.bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (TapeColours::accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.withHeight (26), juce::Justification::centred);
    }
}

void TapeSaturatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (margin, 0);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (margin);

    auto placeKnob = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (18));
        k.slider.setBounds (r);
    };

    auto layoutSection = [&] (Section& s, int numColumns)
    {
        s.bounds = area.removeFromLeft (numColumns * knobW + 16);
        area.removeFromLeft (margin);
        auto inner = s.bounds.reduced (8, 0);
        inner.removeFromTop (30);
        return inner;
    };

    // TAPE: Input, Saturation, Bias, Speed
    {
        auto r = layoutSection (sections[0], 4);
        placeKnob (input,      r.removeFromLeft (knobW).withHeight (knobH));
        placeKnob (saturation, r.removeFromLeft (knobW).withHeight (knobH));
        placeKnob (bias,       r.removeFromLeft (knobW).withHeight (knobH));
        auto sp = r.removeFromLeft (knobW).withHeight (knobH);
        speedLabel.setBounds (sp.removeFromTop (18));
        speedBox.setBounds (sp.withSizeKeepingCentre (knobW - 12, 28));
    }

    // TRANSPORT
    {
        auto r = layoutSection (sections[1], 2);
        placeKnob (flutter, r.removeFromLeft (knobW).withHeight (knobH));
        placeKnob (noise,   r.removeFromLeft (knobW).withHeight (knobH));
    }

    // SLAP DELAY
    {
        auto r = layoutSection (sections[2], 3);
        placeKnob (delayTime, r.removeFromLeft (knobW).withHeight (knobH));
        placeKnob (delayFb,   r.removeFromLeft (knobW).withHeight (knobH));
        placeKnob (delayMix,  r.removeFromLeft (knobW).withHeight (knobH));
    }

    // OUTPUT
    {
        auto r = layoutSection (sections[3], 2);
        auto knobs = r.removeFromTop (knobH);
        placeKnob (output, knobs.removeFromLeft (knobW));
        placeKnob (mix,    knobs.removeFromLeft (knobW));
        r.removeFromTop (8);
        autoGainButton.setBounds (r.removeFromTop (28).reduced (24, 0));
    }
}
