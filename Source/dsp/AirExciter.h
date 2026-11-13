#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

/**
    AIR: bringt Glanz und Präsenz in die Höhen zurück, die das Band abrundet.

    Zwei Anteile, beide mit dem AIR-Regler (0..1) skaliert:
      1. Harmonischer Exciter: Das Band 2.5–7.5 kHz wird abgegriffen, per Hüllkurve normiert
         und mit Tschebyschow-Polynomen (T2, T3) in saubere 2. und 3. Obertöne umgewandelt.
         Die liegen dann bei ~5–22 kHz. Ein Hochpass entfernt tieffrequente Mischprodukte.
         Polynome erzeugen nur Obertöne bis zur 3. Ordnung → kaum Aliasing, kein Oversampling
         und damit keine zusätzliche Latenz nötig.
      2. Sanftes "Air"-Shelf ab ca. 10 kHz (bis +4 dB bei AIR = 100 %).

    Dynamisch: Ist ohnehin schon viel Energie in den Höhen (S-Laute, Becken), wird weniger
    angeregt – damit wird es nicht spitz.

    Bei AIR = 0 wird das Signal nicht angefasst (bit-identisch).
*/
class AirExciter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        const auto n = (size_t) juce::jmax (1, numChannels);
        bandHp.assign (n, {});
        bandLp.assign (n, {});
        harmHp.assign (n, {});
        shelf.assign (n, {});
        envelope.assign (n, 0.0f);

        const double nyq = 0.45 * fs;
        for (auto& f : bandHp)  f.setHighPass (fs, 2500.0, 0.707);
        for (auto& f : bandLp)  f.setLowPass  (fs, juce::jmin (7500.0, nyq), 0.707);
        for (auto& f : harmHp)  f.setHighPass (fs, juce::jmin (5000.0, nyq), 0.707);
        for (auto& f : shelf) f.setHighShelf (fs, juce::jmin (10000.0, nyq), 0.7, 4.0);

        attackCoef  = (float) std::exp (-1.0 / (0.0005 * fs)); // 0.5 ms
        releaseCoef = (float) std::exp (-1.0 / (0.060 * fs));  // 60 ms

        amount.reset (fs, 0.05);
        amount.setCurrentAndTargetValue (pendingAmount);
        active = false;
        reset();
    }

    void reset()
    {
        for (auto* v : { &bandHp, &bandLp, &harmHp, &shelf })
            for (auto& f : *v)
                f.reset();
        std::fill (envelope.begin(), envelope.end(), 0.0f);
    }

    void setAmount (float newAmount)
    {
        pendingAmount = juce::jlimit (0.0f, 1.0f, newAmount);
        amount.setTargetValue (pendingAmount);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (amount.getTargetValue() <= 0.0f && ! amount.isSmoothing())
        {
            active = false;   // aus: Signal bleibt unangetastet, keine Rechenzeit
            return;
        }
        if (! active)
        {
            reset();          // frisch einschalten: keine alten Filterzustände
            amount.setCurrentAndTargetValue (0.0f);
            setAmount (pendingAmount);
            active = true;
        }

        const int numSamples = (int) block.getNumSamples();
        const int chans = juce::jmin ((int) block.getNumChannels(), (int) envelope.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const float a = amount.getNextValue();
            const float harmGain  = 0.30f * a;   // Obertöne max. ca. -10 dB unter dem Quellband
            // a = Überblendung zwischen trockenem und geshelftem Signal (Shelf max. +4 dB ab ~10 kHz)

            for (int ch = 0; ch < chans; ++ch)
            {
                float* d = block.getChannelPointer ((size_t) ch);
                const float x = d[i];

                // 1. Quellband 2.5–7.5 kHz + Hüllkurve
                const float band = bandLp[(size_t) ch].process (bandHp[(size_t) ch].process (x));
                const float mag = std::abs (band);
                float& env = envelope[(size_t) ch];
                env = mag > env ? mag + attackCoef * (env - mag) : mag + releaseCoef * (env - mag);

                // normierter Verlauf → Tschebyschow-Polynome liefern reine 2./3. Harmonische
                const float xn = juce::jlimit (-1.0f, 1.0f, band / (env + 1.0e-6f));
                const float t2 = 2.0f * xn * xn - 1.0f;
                const float t3 = (4.0f * xn * xn - 3.0f) * xn;

                // dynamisch: bei lauten Höhen weniger anregen
                const float dyn = 1.0f / (1.0f + env * 6.0f);
                const float harmonics = harmHp[(size_t) ch].process (env * dyn * (0.65f * t2 + 0.35f * t3));

                // 2. Air-Shelf
                const float shelved = shelf[(size_t) ch].process (x);

                d[i] = x + a * (shelved - x) + harmGain * harmonics;
            }
        }
    }

private:
    /** Einfacher Biquad (Transposed Direct Form II), RBJ-Koeffizienten, allokationsfrei. */
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;

        void setHighPass (double fs, double f, double q) { set (fs, f, q, true); }

        void setHighShelf (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const double sqA = 2.0 * std::sqrt (A) * alpha;
            const double a0 = (A + 1.0) - (A - 1.0) * cw + sqA;
            b0 = (float) (A * ((A + 1.0) + (A - 1.0) * cw + sqA) / a0);
            b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0);
            b2 = (float) (A * ((A + 1.0) + (A - 1.0) * cw - sqA) / a0);
            a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0);
            a2 = (float) (((A + 1.0) - (A - 1.0) * cw - sqA) / a0);
        }
        void setLowPass  (double fs, double f, double q) { set (fs, f, q, false); }

        void set (double fs, double f, double q, bool highPass)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            const double n0 = highPass ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5;
            const double n1 = highPass ? -(1.0 + cw) : (1.0 - cw);
            b0 = (float) (n0 / a0);
            b1 = (float) (n1 / a0);
            b2 = (float) (n0 / a0);
            a1 = (float) (-2.0 * cw / a0);
            a2 = (float) ((1.0 - alpha) / a0);
        }

        void reset() { z1 = z2 = 0.0f; }

        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    double fs = 44100.0;
    std::vector<Biquad> bandHp, bandLp, harmHp, shelf;
    std::vector<float> envelope;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    juce::SmoothedValue<float> amount;
    float pendingAmount = 0.0f;
    bool active = false;
};
