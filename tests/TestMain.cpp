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
