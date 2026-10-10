#include "VocoderEngine.h"

namespace vv
{
namespace
{
    // Calibrated by tests/VocoderTests.cpp ("output level is calibrated"): a vowel-like
    // voice at -20 dBFS RMS gives about -23.5 dBFS with one note and about -19 dBFS with
    // a triad, leaving headroom before the soft clipper for loud singing and big chords.
    constexpr float kMakeupGain = 8.0f;
    constexpr float kUnvoicedNoiseGain = 0.6f;
    constexpr float kMeterReleaseSeconds = 0.3f;

    void decayMeter (std::atomic<float>& meter, float blockPeak, float decay)
    {
        const float held = meter.load (std::memory_order_relaxed) * decay;
        meter.store (std::max (blockPeak, held), std::memory_order_relaxed);
    }
} // namespace

void VocoderEngine::prepare (double newSampleRate, int /*maxBlockSize*/)
{
    sampleRate = static_cast<float> (newSampleRate);

    bank.prepare (sampleRate);
    synth.prepare (sampleRate);
    ensemble.prepare (sampleRate);

    rumbleCoeffs = makeSvfCoeffs (80.0f, 0.707f, sampleRate);
    sibilanceHpCoeffs = makeSvfCoeffs (4500.0f, 0.707f, sampleRate);
    sibilanceLpCoeffs = makeSvfCoeffs (1500.0f, 0.707f, sampleRate);
    airCoeffs = makeSvfCoeffs (std::min (6500.0f, 0.42f * sampleRate), 0.707f, sampleRate);

    gateFollower.setTimes (0.001f, 0.06f, sampleRate);
    hfFollower.setTimes (0.002f, 0.03f, sampleRate);
    lfFollower.setTimes (0.002f, 0.03f, sampleRate);
    gateAttackCoef = onePoleCoef (0.002f, sampleRate);
    gateReleaseCoef = onePoleCoef (0.08f, sampleRate);
    formantCoef = 1.0f - std::exp (-static_cast<float> (kChunkSize) / (0.03f * sampleRate));

    for (auto* s : { &inputGain, &outputGain, &sibilanceAmt, &airAmt, &dryAmt, &ensembleAmt })
        s->prepare (sampleRate);

    reset();
}

void VocoderEngine::reset()
{
    bank.reset();
    synth.reset();
    ensemble.reset();
    for (auto* f : { &rumbleFilter, &sibilanceHp, &sibilanceLp, &airA, &airB })
        f->reset();
    gateFollower.reset();
    hfFollower.reset();
    lfFollower.reset();
    gateGain = 0.0f;
    formantSemis = params.formantSemitones;

    applyParams();
    inputGain.snap (inputGain.target);
    outputGain.snap (outputGain.target);
    sibilanceAmt.snap (sibilanceAmt.target);
    airAmt.snap (airAmt.target);
    dryAmt.snap (dryAmt.target);
    ensembleAmt.snap (ensembleAmt.target);
}

void VocoderEngine::applyParams()
{
    CarrierSynth::Settings s;
    s.sawLevel = params.sawLevel;
    s.pulseLevel = params.pulseLevel;
    s.pulseWidth = params.pulseWidth;
    s.subLevel = params.subLevel;
    s.noiseLevel = params.noiseLevel;
    s.detuneCents = params.detuneCents;
    s.octave = params.octave;
    s.attackSeconds = params.ampAttackMs * 0.001f;
    s.releaseSeconds = params.ampReleaseMs * 0.001f;
    s.glideSeconds = params.glideMs * 0.001f;
    s.mono = params.mono;
    s.bendRangeSemitones = params.bendRangeSemitones;
    s.vibratoRateHz = params.vibratoRateHz;
    s.vibratoDepth = params.vibratoDepth;
    synth.setSettings (s);

    VocoderBank::Settings b;
    b.numBands = params.numBands;
    b.resonance = params.resonance;
    b.formantRatio = std::pow (2.0f, formantSemis / 12.0f);
    b.attackSeconds = params.attackMs * 0.001f;
    b.releaseSeconds = params.releaseMs * 0.001f;
    b.brightnessDbPerOct = params.brightnessDbPerOct;
    bank.configure (b);

    inputGain.setTarget (dbToGain (params.inputGainDb));
    outputGain.setTarget (dbToGain (params.outputGainDb));
    sibilanceAmt.setTarget (params.sibilance);
    airAmt.setTarget (params.air);
    dryAmt.setTarget (params.dryVoice);
    ensembleAmt.setTarget (params.ensemble);
}

void VocoderEngine::process (const float* modulator, float* outL, float* outR, int numSamples,
                             const MidiEvent* events, int numEvents)
{
    applyParams();

    int pos = 0, ev = 0;
    while (pos < numSamples)
    {
        while (ev < numEvents && events[ev].sampleOffset <= pos)
        {
            synth.handleMidi (events[ev].status, events[ev].data1, events[ev].data2);
            midiCounter.fetch_add (1, std::memory_order_relaxed);
            ++ev;
        }

        int end = std::min (numSamples, pos + kChunkSize);
        if (ev < numEvents)
            end = std::min (end, std::max (events[ev].sampleOffset, pos + 1));

        renderChunk (modulator != nullptr ? modulator + pos : nullptr, outL + pos, outR + pos, end - pos);
        pos = end;
    }

    for (; ev < numEvents; ++ev) // out-of-range offsets: apply rather than drop
    {
        synth.handleMidi (events[ev].status, events[ev].data1, events[ev].data2);
        midiCounter.fetch_add (1, std::memory_order_relaxed);
    }

    const float decay = std::exp (-static_cast<float> (numSamples) / (kMeterReleaseSeconds * sampleRate));
    const int n = bank.getNumBands();
    meterBandCount.store (n, std::memory_order_relaxed);
    for (int b = 0; b < n; ++b)
        decayMeter (bandMeters[static_cast<size_t> (b)], bank.takeLevel (b), decay);
    decayMeter (inputMeter, inputPeak, decay);
    decayMeter (outputMeter, outputPeak, decay);
    inputPeak = outputPeak = 0.0f;
    activeVoices.store (synth.getActiveVoiceCount(), std::memory_order_relaxed);
}

void VocoderEngine::renderChunk (const float* modulator, float* outL, float* outR, int numSamples)
{
    synth.beginChunk (numSamples);

    const float formantTarget = params.formantSemitones;
    if (std::abs (formantTarget - formantSemis) > 1.0e-3f)
    {
        formantSemis += formantCoef * (formantTarget - formantSemis);
        applyParams(); // retunes only the carrier bank
    }

    const bool gateOff = params.gateDb <= -89.5f;
    const float gateThreshold = dbToGain (params.gateDb);

    for (int i = 0; i < numSamples; ++i)
    {
        float m = (modulator != nullptr ? modulator[i] : 0.0f) * inputGain.next();
        m = rumbleFilter.highpass (rumbleCoeffs, m);
        inputPeak = std::max (inputPeak, std::abs (m));

        // Soft-knee gate over the last 6 dB below the threshold: background
        // noise between phrases should not open the bank and drone.
        const float level = gateFollower.process (m);
        const float gateTarget = gateOff ? 1.0f : smoothstep (0.5f * gateThreshold, gateThreshold, level);
        gateGain += (gateTarget > gateGain ? gateAttackCoef : gateReleaseCoef) * (gateTarget - gateGain);

        // Unvoiced detector: fricatives ("s", "f", "t") carry more energy above
        // 4.5 kHz than below 1.5 kHz. Then noise replaces the missing carrier
        // highs, as on the Korg VC-10 / Roland VP-330.
        const float hf = hfFollower.process (sibilanceHp.highpass (sibilanceHpCoeffs, m));
        const float lf = lfFollower.process (sibilanceLp.lowpass (sibilanceLpCoeffs, m));
        const float unvoiced = smoothstep (0.35f, 1.0f, hf / (lf + 1.0e-5f)) * gateGain;

        const float activity = synth.getActivity();
        float carrier = synth.renderSample();
        carrier += noise.nextBipolar() * kUnvoicedNoiseGain * sibilanceAmt.next() * unvoiced * activity;

        const float vocoded = bank.process (m, carrier, gateGain) * kMakeupGain;

        // "Air": the voice's own top octave mixed back in for intelligibility,
        // only while keys are sounding.
        const float air = airB.highpass (airCoeffs, airA.highpass (airCoeffs, m)) * airAmt.next() * activity * gateGain;

        const float wet = (params.carrierListen ? carrier : vocoded + air) * outputGain.next();

        float l, r;
        ensemble.process (wet, ensembleAmt.next(), l, r);

        const float dry = m * dryAmt.next();
        l = softClip (l + dry);
        r = softClip (r + dry);
        outL[i] = l;
        outR[i] = r;
        outputPeak = std::max (outputPeak, std::max (std::abs (l), std::abs (r)));
    }
}
} // namespace vv
