/*
    DSP-Tests + Offline-Renderer für TapeSaturator.

    Aufruf:
      TapeSaturatorTests                 -> führt alle Tests aus (Exit-Code 0 = ok)
      TapeSaturatorTests --render <dir>  -> zusätzlich Hörbeispiele als WAV nach <dir>
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <string>

namespace
{
    constexpr double fs = 48000.0;
    constexpr int blockSize = 512;

    int failures = 0;

    void check (bool ok, const std::string& what)
    {
        std::cout << (ok ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;
        if (! ok)
            ++failures;
    }

    juce::AudioBuffer<float> makeSine (double freq, float dbfs, double seconds, int channels = 2)
    {
        const int n = (int) (seconds * fs);
        juce::AudioBuffer<float> b (channels, n);
        const float amp = juce::Decibels::decibelsToGain (dbfs);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < n; ++i)
                b.setSample (ch, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / fs));
        return b;
    }

    /** Verarbeitet einen Buffer blockweise mit frischem Plugin und gegebenen Parametern. */
    juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& in,
                                     const std::function<void (juce::AudioProcessorValueTreeState&)>& setup)
    {
        TapeSaturatorAudioProcessor proc;
        if (setup)
            setup (proc.apvts);

        proc.setPlayConfigDetails (in.getNumChannels(), in.getNumChannels(), fs, blockSize);
        proc.prepareToPlay (fs, blockSize);

        juce::AudioBuffer<float> out (in);
        juce::MidiBuffer midi;
        for (int start = 0; start < out.getNumSamples(); start += blockSize)
        {
            const int len = juce::jmin (blockSize, out.getNumSamples() - start);
            juce::AudioBuffer<float> sub (out.getArrayOfWritePointers(), out.getNumChannels(), start, len);
            proc.processBlock (sub, midi);
        }
        return out;
    }

    void set (juce::AudioProcessorValueTreeState& s, const char* id, float plainValue)
    {
        auto* p = s.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    float rmsDb (const juce::AudioBuffer<float>& b, int startSample)
    {
        double sum = 0.0;
        int n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = startSample; i < b.getNumSamples(); ++i, ++n)
                sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return juce::Decibels::gainToDecibels ((float) std::sqrt (sum / juce::jmax (1, n)), -200.0f);
    }

    /** Amplitude einer Frequenz (Goertzel) auf Kanal 0 ab startSample. */
    double toneAmp (const juce::AudioBuffer<float>& b, double freq, int startSample)
    {
        const int n = b.getNumSamples() - startSample;
        const double w = juce::MathConstants<double>::twoPi * freq / fs;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            // Hann-Fenster
            const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (n - 1));
            const double x = b.getSample (0, startSample + i) * win;
            re += x * std::cos (w * i);
            im -= x * std::sin (w * i);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (0.5 * n);
    }

    /** THD (2.-5. Harmonische) in dB relativ zur Grundwelle. */
    double thdDb (const juce::AudioBuffer<float>& b, double f0, int startSample)
    {
        const double fund = toneAmp (b, f0, startSample);
        double h = 0.0;
        for (int k = 2; k <= 5; ++k)
        {
            const double a = toneAmp (b, f0 * k, startSample);
            h += a * a;
        }
        return 20.0 * std::log10 (std::sqrt (h) / juce::jmax (1.0e-12, fund) + 1.0e-12);
    }

    /** Pegel (RMS, inkl. durch Wow/Flutter verschmierter Energie) eines Sinustons nach dem Plugin, relativ zum Eingang. */
    double toneGainDb (double freq, float level, const std::function<void (juce::AudioProcessorValueTreeState&)>& setup)
    {
        auto in = makeSine (freq, level, 1.0);
        auto out = render (in, setup);
        return rmsDb (out, (int) (0.5 * fs)) - rmsDb (in, (int) (0.5 * fs));
    }

    auto cleanSetup (float saturation)
    {
        return [saturation] (juce::AudioProcessorValueTreeState& s)
        {
            set (s, ParamIDs::saturation, saturation);
            set (s, ParamIDs::flutter, 0.0f);
            set (s, ParamIDs::noise, 0.0f);
            set (s, ParamIDs::delayMix, 0.0f);
        };
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& b)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os = file.createOutputStream();
        if (os == nullptr)
            return;
        const auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (fs)
                                                          .withNumChannels (b.getNumChannels())
                                                          .withBitsPerSample (24);
        if (auto w = wav.createWriterFor (os, opts))
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const int settle = (int) (0.5 * fs);

    std::cout << "TapeSaturator DSP-Tests" << std::endl;

    // 1) Latenz plausibel
    {
        TapeSaturatorAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, fs, blockSize);
        proc.prepareToPlay (fs, blockSize);
        const int lat = proc.getLatencySamples();
        std::cout << "  Latenz: " << lat << " Samples" << std::endl;
        check (lat > 0 && lat < 512, "Latenz wird gemeldet und ist klein");
    }

    // 2) Stille rein -> Stille raus (ohne Rauschen)
    {
        juce::AudioBuffer<float> silence (2, (int) fs);
        silence.clear();
        auto out = render (silence, cleanSetup (0.5f));
        check (allFinite (out), "Stille: keine NaN/Inf");
        check (out.getMagnitude (0, out.getNumSamples()) < 1.0e-5f, "Stille bleibt Stille");
    }

    // 3) Pegelstabilität bei -18 dBFS (Default-Einstellungen ohne Modulation)
    {
        auto in = makeSine (1000.0, -18.0f, 2.0);
        auto out = render (in, cleanSetup (0.5f));
        const float diff = rmsDb (out, settle) - rmsDb (in, settle);
        std::cout << "  Pegeldifferenz bei -18 dBFS: " << diff << " dB" << std::endl;
        check (allFinite (out), "Sinus: keine NaN/Inf");
        check (std::abs (diff) < 3.0f, "Pegel bei -18 dBFS innerhalb +/-3 dB");
    }

    // 4) Mehr Saturation -> mehr Klirr
    {
        auto in = makeSine (200.0, -6.0f, 2.0);
        const double thdLow  = thdDb (render (in, cleanSetup (0.1f)), 200.0, settle);
        const double thdHigh = thdDb (render (in, cleanSetup (1.0f)), 200.0, settle);
        std::cout << "  THD Saturation 10 %: " << thdLow << " dB, 100 %: " << thdHigh << " dB" << std::endl;
        check (thdHigh > thdLow + 3.0, "Saturation erhöht den Klirrfaktor");
    }

    // 5) Bias: Unterbias verzerrt stärker als Überbias
    {
        auto in = makeSine (200.0, -6.0f, 2.0);
        auto withBias = [] (float bias)
        {
            return [bias] (juce::AudioProcessorValueTreeState& s)
            {
                cleanSetup (0.7f) (s);
                set (s, ParamIDs::bias, bias);
            };
        };
        const double under = thdDb (render (in, withBias (0.0f)), 200.0, settle);
        const double over  = thdDb (render (in, withBias (1.0f)), 200.0, settle);
        std::cout << "  THD Bias 0 %: " << under << " dB, 100 %: " << over << " dB" << std::endl;
        check (under > over, "Unterbias verzerrt stärker als Überbias");
    }

    // 6) Worst Case: alles am Anschlag, +18 dB Input, 0 dBFS Sinus
    {
        auto in = makeSine (60.0, 0.0f, 3.0);
        auto out = render (in, [] (juce::AudioProcessorValueTreeState& s)
        {
            set (s, ParamIDs::input, 18.0f);
            set (s, ParamIDs::saturation, 1.0f);
            set (s, ParamIDs::bias, 0.0f);
            set (s, ParamIDs::flutter, 1.0f);
            set (s, ParamIDs::noise, 1.0f);
            set (s, ParamIDs::delayFb, 0.9f);
            set (s, ParamIDs::delayMix, 1.0f);
            set (s, ParamIDs::output, 18.0f);
            set (s, ParamIDs::autoGain, 0.0f);
        });
        const int third = out.getNumSamples() / 3;
        const float peakMid = out.getMagnitude (third, third);
        const float peakEnd = out.getMagnitude (2 * third, third);
        std::cout << "  Spitzenpegel Worst Case (+36 dB Gain ohne Auto Gain): Mitte "
                  << juce::Decibels::gainToDecibels (peakMid) << " dBFS, Ende "
                  << juce::Decibels::gainToDecibels (peakEnd) << " dBFS" << std::endl;
        check (allFinite (out), "Worst Case: keine NaN/Inf");
        check (peakEnd < peakMid * 1.25f && peakEnd < 100.0f, "Worst Case: kein Aufschaukeln (Pegel stabil)");
    }

    // 7) Dry/Wet 0 % = latenzkompensiertes Originalsignal
    {
        auto in = makeSine (440.0, -12.0f, 1.0);
        TapeSaturatorAudioProcessor tmp;
        tmp.setPlayConfigDetails (2, 2, fs, blockSize);
        tmp.prepareToPlay (fs, blockSize);
        const int lat = tmp.getLatencySamples();

        auto out = render (in, [] (juce::AudioProcessorValueTreeState& s) { set (s, ParamIDs::mix, 0.0f); });
        float maxErr = 0.0f;
        for (int i = lat + 1000; i < in.getNumSamples(); ++i)
            maxErr = juce::jmax (maxErr, std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        check (maxErr < 1.0e-4f, "Dry/Wet 0 % liefert das um die Latenz verschobene Original");
    }

    // 8) State speichern / laden
    {
        TapeSaturatorAudioProcessor a, b;
        set (a.apvts, ParamIDs::saturation, 0.83f);
        set (a.apvts, ParamIDs::speed, 0.0f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto pa = a.readParams(), pb = b.readParams();
        check (std::abs (pa.saturation - pb.saturation) < 1.0e-3f && pa.speed == pb.speed,
               "State-Speichern/Laden erhält Parameter");
    }

    // 9) Input-VU-Meter folgt dem INPUT-Regler (Pegel nach dem Gain = was aufs Band geht)
    {
        auto meterAfter = [] (float inputDb)
        {
            TapeSaturatorAudioProcessor proc;
            set (proc.apvts, ParamIDs::input, inputDb);
            proc.setPlayConfigDetails (2, 2, fs, blockSize);
            proc.prepareToPlay (fs, blockSize);
            auto buf = makeSine (1000.0, -18.0f, 1.0);
            juce::MidiBuffer midi;
            for (int start = 0; start < buf.getNumSamples(); start += blockSize)
            {
                juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, start,
                                              juce::jmin (blockSize, buf.getNumSamples() - start));
                proc.processBlock (sub, midi);
            }
            return juce::Decibels::gainToDecibels (proc.getInputLevel());
        };
        const float at0  = meterAfter (0.0f);
        const float at12 = meterAfter (12.0f);
        std::cout << "  Input-VU (RMS) bei INPUT 0 dB: " << at0 << " dBFS, bei +12 dB: " << at12 << " dBFS" << std::endl;
        check (std::abs (at0 - (-21.0f)) < 1.0f, "Input-VU zeigt bei INPUT 0 dB den Quellpegel");
        check (std::abs ((at12 - at0) - 12.0f) < 1.0f, "Input-VU steigt um den INPUT-Regler (+12 dB)");
    }

    // 10) Wow/Flutter kostet keine Höhen (Sinc-Interpolation)
    {
        auto withFlutter = [] (float amount)
        {
            return [amount] (juce::AudioProcessorValueTreeState& s)
            {
                set (s, ParamIDs::flutter, amount);
                set (s, ParamIDs::noise, 0.0f);
                set (s, ParamIDs::delayMix, 0.0f);
            };
        };
        const double loss = toneGainDb (16000.0, -30.0f, withFlutter (0.0f)) - toneGainDb (16000.0, -30.0f, withFlutter (1.0f));
        std::cout << "  Höhenverlust durch Wow/Flutter 100 % bei 16 kHz: " << loss << " dB" << std::endl;
        check (std::abs (loss) < 0.5, "Wow/Flutter ändert die Höhen um weniger als 0,5 dB");
    }

    // 11) AIR: erzeugt Obertöne in den Höhen, lässt die Mitten in Ruhe
    {
        auto withAir = [] (float amount)
        {
            return [amount] (juce::AudioProcessorValueTreeState& s)
            {
                cleanSetup (0.3f) (s);
                set (s, ParamIDs::air, amount);
            };
        };

        auto in = makeSine (4000.0, -24.0f, 2.0);
        const auto off = render (in, withAir (0.0f));
        const auto on  = render (in, withAir (1.0f));
        const double h2Gain = 20.0 * std::log10 (toneAmp (on, 8000.0, settle) / juce::jmax (1.0e-12, toneAmp (off, 8000.0, settle)));
        const double h3Gain = 20.0 * std::log10 (toneAmp (on, 12000.0, settle) / juce::jmax (1.0e-12, toneAmp (off, 12000.0, settle)));
        const double h2Rel = 20.0 * std::log10 (toneAmp (on, 8000.0, settle) / toneAmp (on, 4000.0, settle));
        std::cout << "  AIR 100 % bei 4 kHz: 2. Oberton +" << h2Gain << " dB, 3. Oberton +" << h3Gain
                  << " dB, 2. Oberton relativ zur Grundwelle " << h2Rel << " dB" << std::endl;
        check (h2Gain > 10.0 && h3Gain > 10.0, "AIR erzeugt deutlich hörbare 2. und 3. Obertöne");
        check (h2Rel < -12.0, "AIR-Obertöne bleiben dezent (unter -12 dB zur Grundwelle)");

        const double mids = toneGainDb (1000.0, -18.0f, withAir (1.0f)) - toneGainDb (1000.0, -18.0f, withAir (0.0f));
        std::cout << "  AIR 100 %: Pegeländerung bei 1 kHz " << mids << " dB" << std::endl;
        check (std::abs (mids) < 0.5, "AIR verändert die Mitten (1 kHz) um weniger als 0,5 dB");

        const auto hot = render (makeSine (6000.0, 0.0f, 1.0), [] (juce::AudioProcessorValueTreeState& s)
        {
            set (s, ParamIDs::input, 18.0f);
            set (s, ParamIDs::air, 1.0f);
            set (s, ParamIDs::autoGain, 0.0f);
        });
        check (allFinite (hot), "AIR bei Vollaussteuerung: keine NaN/Inf");
    }

    // 12) Auto Gain: Ausgang auf Eingangspegel, auch bei starker Sättigung; Stille wird nicht hochgezogen
    {
        juce::AudioBuffer<float> sig (2, (int) (4.0 * fs));
        juce::Random rng (7);
        for (int k = 0; k < sig.getNumSamples(); ++k)
        {
            const double t = k / fs;
            const float v = (float) (0.12 * std::exp (-3.0 * std::fmod (t, 0.5))
                                     * (std::sin (juce::MathConstants<double>::twoPi * 110.0 * t)
                                        + std::sin (juce::MathConstants<double>::twoPi * 440.0 * t)))
                            + (rng.nextFloat() - 0.5f) * 0.05f;
            sig.setSample (0, k, v);
            sig.setSample (1, k, v);
        }

        const int from = (int) (2.0 * fs);
        for (auto [inDb, sat] : { std::pair<float, float> { 0.0f, 1.0f }, { 18.0f, 1.0f }, { 9.0f, 0.5f } })
        {
            auto out = render (sig, [inDb, sat] (juce::AudioProcessorValueTreeState& s)
            {
                set (s, ParamIDs::input, inDb);
                set (s, ParamIDs::saturation, sat);
                set (s, ParamIDs::air, 1.0f);
                set (s, ParamIDs::autoGain, 1.0f);
            });
            const float diff = rmsDb (out, from) - rmsDb (sig, from);
            std::cout << "  Auto Gain (INPUT +" << inDb << " dB, Sat " << (int) (sat * 100) << " %, AIR 100 %): Abweichung "
                      << diff << " dB" << std::endl;
            check (std::abs (diff) < 1.0f, "Auto Gain gleicht den Pegel auf +/-1 dB an (INPUT +" + std::to_string ((int) inDb) + " dB)");
        }

        // Signal, dann Stille mit Bandrauschen: Rauschen darf nicht hochgeregelt werden
        juce::AudioBuffer<float> withGap (sig);
        withGap.clear ((int) (2.0 * fs), withGap.getNumSamples() - (int) (2.0 * fs)); // beide Kanäle
        auto hissLevel = [&] (float ag)
        {
            auto out = render (withGap, [ag] (juce::AudioProcessorValueTreeState& s)
            {
                set (s, ParamIDs::input, 12.0f);
                set (s, ParamIDs::noise, 1.0f);
                set (s, ParamIDs::autoGain, ag);
            });
            return rmsDb (out, (int) (3.0 * fs));
        };
        const float hissOff = hissLevel (0.0f), hissOn = hissLevel (1.0f);
        std::cout << "  Rauschen in Stille: Auto Gain AUS " << hissOff << " dBFS, AN " << hissOn << " dBFS" << std::endl;
        check (hissOn < hissOff + 1.0f && hissOn < -40.0f, "Auto Gain zieht Bandrauschen in Pausen nicht hoch");
    }

    // Optional: Auto-Gain-Verhalten messen (--autogain)
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) != "--autogain")
            continue;

        // Testsignale: Sinus, "Musik" (Akkord mit Hüllkurve + Bass), Rauschen – alle ca. -18 dBFS RMS
        auto music = [] (double seconds)
        {
            const int n = (int) (seconds * fs);
            juce::AudioBuffer<float> b (2, n);
            for (int s = 0; s < n; ++s)
            {
                const double t = s / fs;
                const double env = std::exp (-3.0 * std::fmod (t, 0.5));
                double v = 0.0;
                for (double f : { 55.0, 110.0, 220.0, 277.2, 329.6, 440.0, 1760.0, 3520.0 })
                    v += std::sin (juce::MathConstants<double>::twoPi * f * t);
                b.setSample (0, s, (float) (0.09 * env * v));
                b.setSample (1, s, (float) (0.09 * env * v));
            }
            return b;
        };
        auto noise = [] (double seconds)
        {
            juce::Random r (3);
            const int n = (int) (seconds * fs);
            juce::AudioBuffer<float> b (2, n);
            for (int s = 0; s < n; ++s) { const float v = (r.nextFloat() * 2.0f - 1.0f) * 0.218f; b.setSample (0, s, v); b.setSample (1, s, v); }
            return b;
        };
        struct Sig { const char* name; juce::AudioBuffer<float> buf; };
        std::vector<Sig> sigs;
        sigs.push_back ({ "Sinus 1k", makeSine (1000.0, -15.0f, 3.0) });
        sigs.push_back ({ "Musik   ", music (3.0) });
        sigs.push_back ({ "Rauschen", noise (3.0) });

        for (auto& sg : sigs)
        {
            std::cout << "  " << sg.name << " (Eingang " << juce::String (rmsDb (sg.buf, 0), 1) << " dBFS RMS):" << std::endl;
            for (float sat : { 0.5f, 1.0f })
                for (float inDb : { 0.0f, 6.0f, 12.0f, 18.0f })
                {
                    double res[2];
                    for (int ag = 0; ag < 2; ++ag)
                    {
                        auto out = render (sg.buf, [&] (juce::AudioProcessorValueTreeState& s)
                        {
                            set (s, ParamIDs::input, inDb);
                            set (s, ParamIDs::saturation, sat);
                            set (s, ParamIDs::autoGain, (float) ag);
                            set (s, ParamIDs::flutter, 0.0f);
                        });
                        const int from = (int) (1.5 * fs); // Auto Gain eingeschwungen
                        res[ag] = rmsDb (out, from) - rmsDb (sg.buf, from);
                    }
                    if (sat > 0.9f && inDb > 17.0f)
                    {
                        // Pumpen: Schwankung des Pegelverhältnisses Aus/Ein über 50-ms-Fenster
                        auto wobble = [&] (int ag)
                        {
                            auto out = render (sg.buf, [&] (juce::AudioProcessorValueTreeState& s)
                            {
                                set (s, ParamIDs::input, inDb);
                                set (s, ParamIDs::saturation, sat);
                                set (s, ParamIDs::autoGain, (float) ag);
                                set (s, ParamIDs::flutter, 0.0f);
                            });
                            const int w = (int) (0.05 * fs);
                            std::vector<double> r;
                            for (int st = (int) (1.5 * fs); st + w <= out.getNumSamples(); st += w)
                            {
                                double a = 0, b = 0;
                                for (int k = st; k < st + w; ++k) { a += out.getSample (0, k) * out.getSample (0, k); b += sg.buf.getSample (0, k) * sg.buf.getSample (0, k); }
                                if (b > 1.0e-9) r.push_back (10.0 * std::log10 (a / b));
                            }
                            double m = 0; for (double v : r) m += v; m /= (double) r.size();
                            double var = 0; for (double v : r) var += (v - m) * (v - m);
                            return std::sqrt (var / (double) r.size());
                        };
                        std::cout << "    Pumpen (Streuung Aus/Ein-Verhältnis, 50 ms): Auto Gain AUS "
                                  << juce::String (wobble (0), 2) << " dB | AN " << juce::String (wobble (1), 2) << " dB" << std::endl;
                    }
                    std::cout << "    Sat " << (int) (sat * 100) << " %, INPUT +" << (int) inDb << " dB:  Auto Gain AUS "
                              << juce::String (res[0], 1) << " dB | AN " << juce::String (res[1], 1) << " dB" << std::endl;
                }
        }
    }

    // Optional: Frequenzgang messen (--response)
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) != "--response")
            continue;

        struct Setting { const char* name; float speed, flutter, bias, sat, air; };
        const Setting settings[] = {
            { "15 ips, Default         ", 1.0f, 0.2f, 0.5f, 0.5f, 0.0f },
            { "15 ips, Wow/Flutter 0 % ", 1.0f, 0.0f, 0.5f, 0.5f, 0.0f },
            { "15 ips, Air 50 %        ", 1.0f, 0.2f, 0.5f, 0.5f, 0.5f },
            { "15 ips, Air 100 %       ", 1.0f, 0.2f, 0.5f, 0.5f, 1.0f },
            { "7.5 ips, Default        ", 0.0f, 0.2f, 0.5f, 0.5f, 0.0f },
            { "7.5 ips, Air 100 %      ", 0.0f, 0.2f, 0.5f, 0.5f, 1.0f },
        };
        const double freqs[] = { 50, 100, 1000, 5000, 8000, 10000, 12000, 14000, 16000, 18000, 20000 };
        for (float level : { -30.0f, -12.0f })
        {
            std::cout << "  Frequenzgang bei " << level << " dBFS (relativ zu 1 kHz):" << std::endl;
            for (const auto& st : settings)
            {
                std::vector<double> amps;
                for (double f : freqs)
                {
                    amps.push_back (std::pow (10.0, toneGainDb (f, level, [&] (juce::AudioProcessorValueTreeState& s)
                    {
                        set (s, ParamIDs::speed, st.speed);
                        set (s, ParamIDs::flutter, st.flutter);
                        set (s, ParamIDs::bias, st.bias);
                        set (s, ParamIDs::saturation, st.sat);
                        set (s, ParamIDs::air, st.air);
                        set (s, ParamIDs::noise, 0.0f);
                        set (s, ParamIDs::delayMix, 0.0f);
                    }) / 20.0));
                }
                const double ref = amps[2];
                std::cout << "    " << st.name << ":";
                for (size_t k = 0; k < amps.size(); ++k)
                    std::cout << "  " << (int) freqs[k] << "Hz " << juce::String (20.0 * std::log10 (amps[k] / ref), 1);
                std::cout << std::endl;
            }
        }
    }

    // Optional: Screenshot der Oberfläche (braucht ein Display, z. B. xvfb-run)
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) != "--snapshot")
            continue;

        TapeSaturatorAudioProcessor proc;
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto* tapeEditor = dynamic_cast<TapeSaturatorAudioProcessorEditor*> (editor.get());
        if (tapeEditor != nullptr)
            tapeEditor->getMachine().setDemoState (0.35f, 0.66f, 0.72f);

        auto save = [] (const juce::Image& img, const juce::File& file)
        {
            file.deleteFile();
            if (auto os = file.createOutputStream())
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (img, *os);
            }
            std::cout << "  Screenshot: " << file.getFullPathName() << std::endl;
        };

        juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        save (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), file);

        // Skalierung prüfen: 75 % Fenstergröße
        editor->setSize (editor->getWidth() * 3 / 4, editor->getHeight() * 3 / 4);
        save (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f),
              file.getSiblingFile (file.getFileNameWithoutExtension() + "_75" + file.getFileExtension()));
    }

    // Optional: Hörbeispiele rendern
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) != "--render")
            continue;

        juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        dir.createDirectory();

        // Testsignal: Akkord mit Hüllkurve + Bass
        const int n = (int) (4.0 * fs);
        juce::AudioBuffer<float> music (2, n);
        for (int s = 0; s < n; ++s)
        {
            const double t = s / fs;
            const double env = std::exp (-3.0 * std::fmod (t, 0.5));
            double v = 0.0;
            for (double f : { 110.0, 220.0, 277.2, 329.6, 440.0 })
                v += std::sin (juce::MathConstants<double>::twoPi * f * t);
            const float x = (float) (0.12 * env * v);
            music.setSample (0, s, x);
            music.setSample (1, s, x);
        }

        writeWav (dir.getChildFile ("00_original.wav"), music);
        writeWav (dir.getChildFile ("01_default.wav"), render (music, nullptr));
        writeWav (dir.getChildFile ("02_hot_7_5ips.wav"), render (music, [] (auto& s)
        {
            set (s, ParamIDs::input, 9.0f);
            set (s, ParamIDs::saturation, 0.9f);
            set (s, ParamIDs::speed, 0.0f);
            set (s, ParamIDs::flutter, 0.6f);
            set (s, ParamIDs::noise, 0.5f);
        }));
        writeWav (dir.getChildFile ("03_slap.wav"), render (music, [] (auto& s)
        {
            set (s, ParamIDs::delayTime, 110.0f);
            set (s, ParamIDs::delayFb, 0.35f);
            set (s, ParamIDs::delayMix, 0.5f);
        }));
        std::cout << "  Hörbeispiele geschrieben nach: " << dir.getFullPathName() << std::endl;
    }

    std::cout << (failures == 0 ? std::string ("Alle Tests bestanden.") : std::to_string (failures) + " Test(s) fehlgeschlagen.") << std::endl;
    return failures == 0 ? 0 : 1;
}
