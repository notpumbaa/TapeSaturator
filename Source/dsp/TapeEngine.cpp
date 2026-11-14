#include "TapeEngine.h"

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Wow & Flutter bei 100 %: Tonhöhenabweichung (Spitze) als Anteil
    constexpr double wowFreq        = 0.55;   // Hz
    constexpr double flutterFreq    = 6.3;    // Hz
    constexpr double flutter2Freq   = 11.7;   // Hz
    constexpr double maxWowDev      = 0.0025; // 0.25 %
    constexpr double maxFlutterDev  = 0.0008; // 0.08 %
    constexpr double maxDriftSec    = 0.0003; // 0.3 ms langsame Drift
    constexpr double motionBaseSec  = 0.0015; // 1.5 ms Grundverzögerung

    // Pre-/De-Emphasis (Record-/Playback-EQ)
    constexpr float emphasisFreq = 3000.0f;
    constexpr float emphasisDb   = 5.0f;

    float onePoleCoef (double cutoff, double fs)
    {
        return (float) std::exp (-twoPi * cutoff / fs);
    }
}

TapeEngine::TapeEngine()
{
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
}

void TapeEngine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate  = newSampleRate;
    maxBlock    = juce::jmax (1, maxBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) numChannels };

    // Oversampling + Hysterese
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, (size_t) oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlock);

    hysteresis.assign ((size_t) numChannels, Hysteresis {});
    const double osRate = sampleRate * (double) (1 << oversamplingOrder);
    for (auto& h : hysteresis)
        h.prepare (osRate);

    // Filter
    for (auto* f : { &preEmphasis, &deEmphasis, &lowCut, &headBump, &bumpDip, &hfRolloff, &biasTilt })
        f->prepare (spec);

    // Wow & Flutter
    motionBaseDelay = (float) (motionBaseSec * sampleRate);
    motionDelay.prepare (numChannels, (int) std::ceil (sampleRate * 0.01) + 8);
    airExciter.prepare (sampleRate, numChannels);

    // Slap-Delay
    echoDelay.setMaximumDelayInSamples ((int) std::ceil (sampleRate * 0.85) + 8);
    echoDelay.prepare (spec);
    echoLp.assign ((size_t) numChannels, 0.0f);
    echoHp.assign ((size_t) numChannels, 0.0f);
    echoLpCoef = onePoleCoef (4500.0, sampleRate);
    echoHpCoef = onePoleCoef (120.0, sampleRate);

    noiseLp.assign ((size_t) numChannels, 0.0f);

    // Glättung
    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    autoTrim.reset (sampleRate, 0.05);
    makeupGain.reset (sampleRate, 0.05);
    flutterAmount.reset (sampleRate, 0.1);
    echoTimeSamples.reset (sampleRate, 0.25); // langsame Zeitänderung = Tape-artiger Pitch-Glide
    echoFeedback.reset (sampleRate, 0.05);
    echoMix.reset (sampleRate, 0.05);
    noiseGain.reset (sampleRate, 0.05);

    // Latenz
    latencySamples = (int) std::round (oversampling->getLatencyInSamples()) + (int) std::round (motionBaseDelay);

    dryWet.prepare (spec);
    dryWet.setWetLatency ((float) latencySamples);

    prepared = true;
    lastSpeed = -1;
    lastBias = -1.0f;
    setParameters (params);
    reset();
}

void TapeEngine::reset()
{
    if (! prepared)
        return;

    oversampling->reset();
    for (auto& h : hysteresis)
        h.reset();

    for (auto* f : { &preEmphasis, &deEmphasis, &lowCut, &headBump, &bumpDip, &hfRolloff, &biasTilt })
        f->reset();

    motionDelay.reset();
    airExciter.reset();
    echoDelay.reset();
    std::fill (echoLp.begin(), echoLp.end(), 0.0f);
    std::fill (echoHp.begin(), echoHp.end(), 0.0f);
    std::fill (noiseLp.begin(), noiseLp.end(), 0.0f);
    wowPhase = flutterPhase = flutter2Phase = 0.0;
    driftState = driftTarget = 0.0f;
    driftCounter = 0;
    dryWet.reset();

    agDryPower = agWetPower = 0.0;
    autoTrim.setCurrentAndTargetValue (1.0f);

    for (auto* s : { &inputGain, &outputGain, &makeupGain, &flutterAmount,
                     &echoTimeSamples, &echoFeedback, &echoMix, &noiseGain })
        s->setCurrentAndTargetValue (s->getTargetValue());
}

void TapeEngine::setParameters (const TapeParams& p)
{
    params = p;
    if (! prepared)
        return;

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (p.inputDb));

    // Auto Gain: Input-Gain hinter der Sättigung wieder ausgleichen
    const float autoComp = p.autoGain ? juce::Decibels::decibelsToGain (-p.inputDb) : 1.0f;
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb) * autoComp);

    // Hysterese: Saturation -> Drive, Bias -> Schleifenbreite
    // Unterbias (wenig Bias) = breite Schleife, mehr Verzerrung; Überbias = sauberer.
    const double drive = 0.05 + 0.95 * (double) p.saturation;
    const double width = 0.95 - 0.85 * (double) p.bias;
    for (auto& h : hysteresis)
        h.setParameters (drive, width);
    makeupGain.setTargetValue (hysteresis.empty() ? 1.0f : (float) hysteresis.front().getMakeupGain());

    flutterAmount.setTargetValue (p.flutter);

    echoTimeSamples.setTargetValue ((float) (juce::jlimit (30.0f, 800.0f, p.delayMs) * 0.001 * sampleRate));
    echoFeedback.setTargetValue (juce::jlimit (0.0f, 0.9f, p.delayFeedback));
    echoMix.setTargetValue (p.delayMix);

    // Rauschen: 0 = aus, sonst -80..-50 dBFS; 7.5 ips rauscht ~3 dB mehr
    const float noiseDb = -80.0f + 30.0f * p.noise + (p.speed == 0 ? 3.0f : 0.0f);
    noiseGain.setTargetValue (p.noise <= 0.0f ? 0.0f : juce::Decibels::decibelsToGain (noiseDb));

    dryWet.setWetMixProportion (p.mix);
    airExciter.setAmount (p.air);

    if (p.speed != lastSpeed || std::abs (p.bias - lastBias) > 1.0e-3f)
        updateFilters();
}

void TapeEngine::updateFilters()
{
    lastSpeed = params.speed;
    lastBias  = params.bias;

    const double fs  = sampleRate;
    const float nyq  = (float) (0.45 * fs);
    const bool slow  = params.speed == 0;

    *preEmphasis.state = *Coeffs::makeHighShelf (fs, emphasisFreq, 0.7f, juce::Decibels::decibelsToGain (emphasisDb));
    *deEmphasis.state  = *Coeffs::makeHighShelf (fs, emphasisFreq, 0.7f, juce::Decibels::decibelsToGain (-emphasisDb));

    // Head Bump & HF-Verhalten je Bandgeschwindigkeit
    const float lowCutHz  = slow ? 30.0f  : 22.0f;
    const float bumpHz    = slow ? 50.0f  : 90.0f;
    const float bumpDb    = slow ? 3.0f   : 2.0f;
    const float dipHz     = slow ? 110.0f : 200.0f;
    const float dipDb     = slow ? -1.5f  : -1.0f;
    const float hfHz      = juce::jmin (nyq, slow ? 13000.0f : 19000.0f);

    *lowCut.state    = *Coeffs::makeHighPass (fs, lowCutHz, 0.6f);
    *headBump.state  = *Coeffs::makePeakFilter (fs, bumpHz, 1.2f, juce::Decibels::decibelsToGain (bumpDb));
    *bumpDip.state   = *Coeffs::makePeakFilter (fs, dipHz, 1.0f, juce::Decibels::decibelsToGain (dipDb));
    *hfRolloff.state = *Coeffs::makeLowPass (fs, hfHz, 0.6f);

    // Bias-Tilt: Unterbias = heller, Überbias = dunkler (+/-2 dB ab 8 kHz)
    const float tiltDb = (0.5f - params.bias) * 4.0f;
    *biasTilt.state = *Coeffs::makeHighShelf (fs, juce::jmin (nyq, 8000.0f), 0.7f, juce::Decibels::decibelsToGain (tiltDb));
}

void TapeEngine::process (juce::AudioBuffer<float>& buffer)
{
    jassert (prepared);
    tapeInputSumSq = 0.0;
    tapeInputCount = 0;

    const int numSamples = buffer.getNumSamples();

    // Nicht mehr als die vorbereitete Blockgröße auf einmal verarbeiten
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
        processChunk (chunk);
    }
}

float TapeEngine::getTapeInputMeanSquare() const noexcept
{
    return tapeInputCount > 0 ? (float) (tapeInputSumSq / (double) tapeInputCount) : 0.0f;
}

void TapeEngine::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int chans = juce::jmin (buffer.getNumChannels(), numChannels);
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    dryWet.pushDrySamples (block);

    // Input (+ Pegel "aufs Band" für das Input-VU-Meter, + Eingangspegel für Auto Gain)
    double drySumSq = 0.0;
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = inputGain.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto& x = block.getChannelPointer ((size_t) ch)[n];
            drySumSq += (double) x * (double) x;
            x *= g;
            tapeInputSumSq += (double) x * (double) x;
        }
    }
    tapeInputCount += numSamples * chans;

    // Record-EQ -> Hysterese (oversampled) -> Playback-EQ
    preEmphasis.process (ctx);

    auto osBlock = oversampling->processSamplesUp (block);
    const int osSamples = (int) osBlock.getNumSamples();
    for (int n = 0; n < osSamples; ++n)
    {
        const float makeup = (n % (1 << oversamplingOrder) == 0) ? makeupGain.getNextValue() : makeupGain.getCurrentValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = osBlock.getChannelPointer ((size_t) ch);
            d[n] = (float) hysteresis[(size_t) ch].process ((double) d[n]) * makeup;
        }
    }
    oversampling->processSamplesDown (block);

    deEmphasis.process (ctx);

    // Bandgeschwindigkeit / Bias
    lowCut.process (ctx);
    headBump.process (ctx);
    bumpDip.process (ctx);
    hfRolloff.process (ctx);
    biasTilt.process (ctx);

    processTapeMotion (block);
    processDelay (block);
    addNoise (block);
    airExciter.process (block);

    // Auto Gain: Restabweichung zwischen Ein- und Ausgangspegel langsam ausgleichen
    double wetSumSq = 0.0;
    for (int ch = 0; ch < chans; ++ch)
    {
        const float* d = block.getChannelPointer ((size_t) ch);
        for (int n = 0; n < numSamples; ++n)
            wetSumSq += (double) d[n] * (double) d[n];
    }
    const double count = (double) juce::jmax (1, numSamples * chans);
    updateAutoGain (drySumSq / count, wetSumSq / count, numSamples);

    // Output
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = outputGain.getNextValue() * autoTrim.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
            block.getChannelPointer ((size_t) ch)[n] *= g;
    }

    dryWet.mixWetSamples (block);
}

void TapeEngine::updateAutoGain (double dryMeanSq, double wetMeanSq, int numSamples)
{
    // Pegel über ~0.8 s mitteln; bei Stille einfrieren (sonst würde z. B. Bandrauschen hochgezogen)
    constexpr double silence = 1.0e-7; // ca. -70 dBFS
    if (dryMeanSq > silence)
    {
        if (agDryPower <= 0.0)
        {
            agDryPower = dryMeanSq;
            agWetPower = wetMeanSq;
        }
        else
        {
            const double a = 1.0 - std::exp (-(double) numSamples / (autoGainSeconds * sampleRate));
            agDryPower += a * (dryMeanSq - agDryPower);
            agWetPower += a * (wetMeanSq - agWetPower);
        }
    }

    float target = 1.0f;
    if (params.autoGain && agDryPower > silence && agWetPower > 1.0e-12)
    {
        // Der INPUT-Regler wird bereits sofort über outputGain ausgeglichen – hier nur der Rest
        // (Sättigung/Kompression, Bias, Speed-EQ, AIR, Delay ...)
        const double staticComp = juce::Decibels::decibelsToGain ((double) -params.inputDb);
        const double residual = std::sqrt (agDryPower / (agWetPower * staticComp * staticComp));
        target = (float) juce::jlimit (juce::Decibels::decibelsToGain (-autoGainRangeDb),
                                       juce::Decibels::decibelsToGain (autoGainRangeDb), residual);
    }
    autoTrim.setTargetValue (target);
}

void TapeEngine::processTapeMotion (juce::dsp::AudioBlock<float>& block)
{
    const int numSamples = (int) block.getNumSamples();
    const int chans = (int) block.getNumChannels();

    const double wowInc   = wowFreq / sampleRate;
    const double flutInc  = flutterFreq / sampleRate;
    const double flut2Inc = flutter2Freq / sampleRate;

    // Delay-Amplitude D für Pitch-Abweichung p bei Frequenz f: p = 2*pi*f*D
    const double wowDepth   = maxWowDev / (twoPi * wowFreq) * sampleRate;
    const double flutDepth  = maxFlutterDev / (twoPi * flutterFreq) * sampleRate;
    const double driftDepth = maxDriftSec * sampleRate;
    const float driftCoef   = onePoleCoef (0.8, sampleRate);
    const int driftHold     = juce::jmax (1, (int) (sampleRate * 0.25));

    for (int n = 0; n < numSamples; ++n)
    {
        const double amt = flutterAmount.getNextValue();

        if (--driftCounter <= 0)
        {
            driftTarget = random.nextFloat() * 2.0f - 1.0f;
            driftCounter = driftHold;
        }
        driftState = driftTarget + driftCoef * (driftState - driftTarget);

        const double mod = amt * (wowDepth * std::sin (twoPi * wowPhase)
                                  + flutDepth * std::sin (twoPi * flutterPhase)
                                  + 0.4 * flutDepth * std::sin (twoPi * flutter2Phase)
                                  + driftDepth * driftState);

        wowPhase      += wowInc;   if (wowPhase >= 1.0)      wowPhase -= 1.0;
        flutterPhase  += flutInc;  if (flutterPhase >= 1.0)  flutterPhase -= 1.0;
        flutter2Phase += flut2Inc; if (flutter2Phase >= 1.0) flutter2Phase -= 1.0;

        const float delay = juce::jmax ((float) SincDelay::kHalfTaps, motionBaseDelay + (float) mod);

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            motionDelay.push (ch, d[n]);
            d[n] = motionDelay.read (ch, delay);
        }
    }
}

void TapeEngine::processDelay (juce::dsp::AudioBlock<float>& block)
{
    const int numSamples = (int) block.getNumSamples();
    const int chans = (int) block.getNumChannels();

    // Wenn Delay aus ist, trotzdem Leitung weiter füttern, damit das Einblenden sauber ist
    for (int n = 0; n < numSamples; ++n)
    {
        const float time = echoTimeSamples.getNextValue();
        const float fb   = echoFeedback.getNextValue();
        const float mix  = echoMix.getNextValue();

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            const float dry = d[n];

            float echo = echoDelay.popSample (ch, time);

            // Bandartige Färbung im Echo: Tiefpass + Hochpass
            auto& lp = echoLp[(size_t) ch];
            auto& hp = echoHp[(size_t) ch];
            lp = echo + echoLpCoef * (lp - echo);
            hp = lp + echoHpCoef * (hp - lp);
            echo = lp - hp;

            // Feedback leicht gesättigt -> kein Aufschaukeln
            echoDelay.pushSample (ch, dry + std::tanh (fb * echo));

            d[n] = dry + mix * echo;
        }
    }
}

void TapeEngine::addNoise (juce::dsp::AudioBlock<float>& block)
{
    if (noiseGain.getTargetValue() <= 0.0f && ! noiseGain.isSmoothing())
        return;

    const int numSamples = (int) block.getNumSamples();
    const int chans = (int) block.getNumChannels();
    const float lpCoef = onePoleCoef (params.speed == 0 ? 6000.0 : 9000.0, sampleRate);

    for (int n = 0; n < numSamples; ++n)
    {
        const float g = noiseGain.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            const float white = random.nextFloat() * 2.0f - 1.0f;
            auto& lp = noiseLp[(size_t) ch];
            lp = white + lpCoef * (lp - white);
            // Mischung aus gefiltertem und etwas weißem Rauschen = typischer "Hiss"
            block.getChannelPointer ((size_t) ch)[n] += g * (1.6f * lp + 0.25f * white);
        }
    }
}
