#pragma once

#include <array>
#include <cmath>
#include <vector>

/**
    Modulierbare Verzögerungsleitung mit gefensterter Sinc-Interpolation (16 Taps, Kaiser-Fenster).

    Anders als Lagrange-/Linear-Interpolation bleibt der Frequenzgang bei gebrochenen
    Verzögerungen bis ca. 85 % der Nyquist-Frequenz praktisch linear. Wow & Flutter
    verändert damit nur die Tonhöhe, nicht den Höhenanteil.

    Verzögerung in Samples, Minimum = kHalfTaps (8).
*/
class SincDelay
{
public:
    static constexpr int kHalfTaps = 8;
    static constexpr int kTaps = 2 * kHalfTaps;
    static constexpr int kPhases = 256;

    void prepare (int numChannels, int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + kTaps + 4)
            size <<= 1;
        mask = size - 1;
        buffers.assign ((size_t) numChannels, std::vector<float> ((size_t) size, 0.0f));
        newest.assign ((size_t) numChannels, 0);
        table(); // Tabelle einmalig außerhalb des Audio-Threads anlegen
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        std::fill (newest.begin(), newest.end(), 0);
    }

    void push (int channel, float x) noexcept
    {
        auto& idx = newest[(size_t) channel];
        idx = (idx + 1) & mask;
        buffers[(size_t) channel][(size_t) idx] = x;
    }

    /** Wert von vor `delay` Samples (0 = zuletzt geschriebenes Sample). */
    float read (int channel, float delay) const noexcept
    {
        if (delay < (float) kHalfTaps)
            delay = (float) kHalfTaps;

        const int di = (int) delay;
        const float frac = delay - (float) di;

        // Koeffizienten zwischen zwei Tabellenphasen linear interpolieren
        const float p = frac * (float) kPhases;
        const int p0 = (int) p;
        const float t = p - (float) p0;
        const auto& c0 = table()[(size_t) p0];
        const auto& c1 = table()[(size_t) p0 + 1];

        const auto& buf = buffers[(size_t) channel];
        const int base = newest[(size_t) channel] - di;

        float y = 0.0f;
        for (int k = 0; k < kTaps; ++k)
        {
            const int j = k - kHalfTaps;                     // j = -8 .. 7
            const float c = c0[(size_t) k] + t * (c1[(size_t) k] - c0[(size_t) k]);
            y += c * buf[(size_t) ((base + j) & mask)];
        }
        return y;
    }

private:
    using Row = std::array<float, kTaps>;

    static const std::vector<Row>& table()
    {
        static const std::vector<Row> rows = []
        {
            std::vector<Row> r ((size_t) kPhases + 1);
            constexpr double beta = 7.0;
            const double i0Beta = besselI0 (beta);
            constexpr double pi = 3.14159265358979323846;

            for (int p = 0; p <= kPhases; ++p)
            {
                const double frac = (double) p / (double) kPhases;
                double sum = 0.0;
                for (int k = 0; k < kTaps; ++k)
                {
                    // Sample bei Index (base + j) liegt um s = j + frac neben dem Zielzeitpunkt
                    const double s = (double) (k - kHalfTaps) + frac;
                    const double sinc = std::abs (s) < 1.0e-9 ? 1.0 : std::sin (pi * s) / (pi * s);
                    const double ratio = s / (double) kHalfTaps;
                    const double win = std::abs (ratio) >= 1.0 ? 0.0
                                     : besselI0 (beta * std::sqrt (1.0 - ratio * ratio)) / i0Beta;
                    r[(size_t) p][(size_t) k] = (float) (sinc * win);
                    sum += sinc * win;
                }
                for (auto& c : r[(size_t) p]) // Gleichanteil exakt 1
                    c = (float) ((double) c / sum);
            }
            return r;
        }();
        return rows;
    }

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 32; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    std::vector<std::vector<float>> buffers;
    std::vector<int> newest;
    int mask = 0;
};
