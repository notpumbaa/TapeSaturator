#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TapeLookAndFeel.h"
#include "TapeReel.h"
#include "VUMeter.h"

/**
    Die komplette Bandmaschinen-Oberfläche in fester Basisgröße (kBaseWidth × kBaseHeight).
    Der Editor skaliert sie per AffineTransform – dadurch bleibt alles vektorbasiert scharf,
    die gecachten Hintergründe werden in der jeweiligen Pixelauflösung neu gerendert.
*/
class TapeMachine : public juce::Component
{
public:
    static constexpr int kBaseWidth  = 1180;
    static constexpr int kBaseHeight = 520;

    explicit TapeMachine (juce::AudioProcessorValueTreeState&);
    ~TapeMachine() override;

    /** Mechanik-Animation weiterschalten (vom Editor-Timer aufgerufen). */
    void tick (float dt, bool audioRunning, float inputRms, float outputRms);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Für Screenshots: Spulen/Meter in einen "laufenden" Zustand versetzen. */
    void setDemoState (float tapePosition, float inputNeedle, float outputNeedle);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    struct Arm
    {
        juce::Point<float> pivot;
        float restAngle = 0.0f; // Radiant, 0 = senkrecht nach oben
        float angle = 0.0f, velocity = 0.0f;
        juce::Point<float> rollerCentre() const;
    };

    void addKnob (Knob&, const char* paramID, const juce::String& caption);
    void addSwitch (Switch&, const char* paramID, const juce::String& caption, const juce::String& labels);

    void rebuildCaches (float pixelScale);
    void paintTransportStatic (juce::Graphics&);
    void paintPlateStatic (juce::Graphics&);
    void paintTapePath (juce::Graphics&);
    void paintLamp (juce::Graphics&);
    void updateReels();

    juce::AudioProcessorValueTreeState& state;
    TapeLookAndFeel lnf;

    Knob input, saturation, bias, flutter, noise, delayTime, delayFb, delayMix, output, mix;
    Switch speedSwitch, autoGainSwitch;

    TapeReel leftReel { 0.8f }, rightReel { 0.5f };
    VUMeter inputVU { "INPUT" }, outputVU { "OUTPUT" };
    Arm leftArm, rightArm;

    std::array<Section, 4> sections;
    std::vector<std::pair<juce::Component*, juce::String>> captions;

    juce::Rectangle<int> transportArea, plateArea, woodLeft, woodRight;
    juce::Rectangle<float> lampBounds;
    float tapeY = 0.0f;

    // Caches in Pixelauflösung
    juce::Image transportCache, plateCache, woodLeftCache, woodRightCache;
    float cachedScale = 0.0f;

    // Mechanik
    float tapeSpeed = 0.0f;      // 0 = steht, 0.5 = 7.5 ips, 1 = 15 ips
    float tapePosition = 0.3f;   // 0..1: wie viel Band schon auf der rechten Spule ist
    float leftAngle = 0.0f, rightAngle = 0.0f;
    float time = 0.0f;
    bool lampOn = false;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeMachine)
};
