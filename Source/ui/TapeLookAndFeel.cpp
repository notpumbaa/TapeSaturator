#include "TapeLookAndFeel.h"

TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, TapeColours::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, TapeColours::cream);
    setColour (juce::ComboBox::backgroundColourId, TapeColours::knobBody);
    setColour (juce::ComboBox::textColourId, TapeColours::cream);
    setColour (juce::ComboBox::outlineColourId, TapeColours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, TapeColours::accent);
    setColour (juce::PopupMenu::backgroundColourId, TapeColours::panel);
    setColour (juce::PopupMenu::textColourId, TapeColours::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, TapeColours::accent);
    setColour (juce::ToggleButton::textColourId, TapeColours::cream);
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcRadius = radius - 2.0f;

    // Skala
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (TapeColours::panelEdge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (TapeColours::accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knopf
    const float knobR = radius * 0.72f;
    g.setGradientFill (juce::ColourGradient (TapeColours::knobBody.brighter (0.25f), centre.x, centre.y - knobR,
                                             TapeColours::knobBody.darker (0.4f), centre.x, centre.y + knobR, false));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (TapeColours::panelEdge.brighter (0.2f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    // Zeiger
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (TapeColours::cream);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? TapeColours::accent : TapeColours::knobBody);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? TapeColours::cream : TapeColours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setColour (on ? TapeColours::background : TapeColours::cream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}
