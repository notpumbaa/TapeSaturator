#pragma once

#include <cmath>

/**
    Magnetische Hysterese nach dem Jiles-Atherton-Modell (ein Kanal).

    H = Feldstärke (Eingangssignal), M = Magnetisierung (Ausgang).
    Gelöst mit Runge-Kutta 4. Ordnung; sollte mit Oversampling laufen.

    Parameter (alle 0..1):
      drive : wie stark das Band in die Sättigung getrieben wird
      width : Breite der Hystereseschleife (wird aus "Bias" abgeleitet)

    Ausgang ist auf Kleinsignal-Verstärkung ~1 normalisiert (Kalibrierung
    in setParameters()), damit der Pegel beim Drehen an Drive/Bias stabil bleibt.
*/
class Hysteresis
{
public:
    void prepare (double sampleRate) noexcept
    {
        T = 1.0 / sampleRate;
        reset();
        updateCoefficients();
    }

    void reset() noexcept { M_n1 = H_n1 = Hd_n1 = 0.0; }

    void setParameters (double newDrive, double newWidth) noexcept
    {
        if (std::abs (newDrive - drive) < 1.0e-4 && std::abs (newWidth - width) < 1.0e-4)
            return;

        drive = newDrive;
        width = newWidth;
        updateCoefficients();
    }

    /** Kleinsignal-Ausgleich für die aktuellen Parameter. */
    double getMakeupGain() const noexcept { return makeup; }

    /** Verarbeitet ein Sample; Ausgang ohne Makeup (Makeup separat glätten). */
    double process (double x) noexcept
    {
        const double H  = x;
        const double Hd = derivative (H);
        double M = rk4 (H, Hd);

        if (! std::isfinite (M))
        {
            reset();
            return 0.0;
        }

        M_n1 = M;
        H_n1 = H;
        Hd_n1 = Hd;
        return M;
    }

private:
    // Modellparameter
    double Ms = 1.0, a = 1.0, alpha = 1.6e-3, k = 0.47875, c = 0.5;
    double T = 1.0 / 192000.0;
    double drive = -1.0, width = -1.0, makeup = 1.0;

    // Zustand
    double M_n1 = 0.0, H_n1 = 0.0, Hd_n1 = 0.0;

    static constexpr double dAlpha = 0.75; // Alpha-Transform für die Ableitung

    void updateCoefficients() noexcept
    {
        const double d = drive < 0.0 ? 0.5 : drive;
        const double w = width < 0.0 ? 0.5 : width;

        Ms = 1.0;
        a  = Ms / (0.01 + 6.0 * d);
        c  = std::sqrt (1.0 - w) - 0.01;
        k  = 0.47875;

        makeup = calibrate();
    }

    double derivative (double H) const noexcept
    {
        return ((1.0 + dAlpha) / T) * (H - H_n1) - dAlpha * Hd_n1;
    }

    static void langevin (double x, double& L, double& Lp) noexcept
    {
        if (std::abs (x) < 1.0e-2)
        {
            const double x2 = x * x;
            L  = x / 3.0 - x * x2 / 45.0;
            Lp = 1.0 / 3.0 - x2 / 15.0;
            return;
        }

        const double cothX = 1.0 / std::tanh (x);
        L  = cothX - 1.0 / x;
        Lp = 1.0 / (x * x) - cothX * cothX + 1.0;
    }

    double dMdt (double M, double H, double Hd) const noexcept
    {
        const double Q = (H + alpha * M) / a;
        double L, Lp;
        langevin (Q, L, Lp);

        const double Mdiff  = Ms * L - M;
        const double delta  = Hd >= 0.0 ? 1.0 : -1.0;
        const double deltaM = ((delta > 0.0) == (Mdiff > 0.0)) ? 1.0 : 0.0;

        const double f1Denom = (1.0 - c) * delta * k - alpha * Mdiff;
        const double f1 = std::abs (f1Denom) > 1.0e-9 ? (1.0 - c) * deltaM * Mdiff / f1Denom : 0.0;
        const double f2 = c * Ms / a * Lp;
        const double f3 = 1.0 - c * alpha * Ms / a * Lp;

        return Hd * (f1 + f2) / f3;
    }

    double rk4 (double H, double Hd) const noexcept
    {
        const double Hmid  = 0.5 * (H + H_n1);
        const double Hdmid = 0.5 * (Hd + Hd_n1);

        const double k1 = T * dMdt (M_n1,              H_n1, Hd_n1);
        const double k2 = T * dMdt (M_n1 + 0.5 * k1,   Hmid, Hdmid);
        const double k3 = T * dMdt (M_n1 + 0.5 * k2,   Hmid, Hdmid);
        const double k4 = T * dMdt (M_n1 + k3,         H,    Hd);

        return M_n1 + (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
    }

    /** Misst die Kleinsignal-Verstärkung mit einem leisen Sinus (kein Heap, echtzeitfest). */
    double calibrate() const noexcept
    {
        Hysteresis probe = *this;
        probe.reset();

        constexpr int period = 64;
        constexpr int cycles = 6;
        constexpr double amp = 0.01;
        constexpr double twoPi = 6.283185307179586;

        double inSq = 0.0, outSq = 0.0;
        for (int n = 0; n < period * cycles; ++n)
        {
            const double x = amp * std::sin (twoPi * n / period);
            const double y = probe.process (x);
            if (n >= period * (cycles - 2))
            {
                inSq  += x * x;
                outSq += y * y;
            }
        }

        if (outSq <= 1.0e-20 || ! std::isfinite (outSq))
            return 1.0;

        const double g = std::sqrt (inSq / outSq);
        return g < 0.1 ? 0.1 : (g > 50.0 ? 50.0 : g);
    }
};
