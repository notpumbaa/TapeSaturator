#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace TapeColours
{
    inline const juce::Colour background { 0xff1e1a17 };
    inline const juce::Colour panel      { 0xff2b2521 };
    inline const juce::Colour panelEdge  { 0xff4a3f37 };
    inline const juce::Colour cream      { 0xffeadfc8 };
    inline const juce::Colour creamDim   { 0xffa89a84 };
    inline const juce::Colour accent     { 0xffd9822b }; // Bernstein
    inline const juce::Colour knobBody   { 0xff3a322c };
}

class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapeLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};
