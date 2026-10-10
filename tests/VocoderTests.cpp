// Dependency-free unit tests for the DSP core. Run: ctest or ./vv_tests

#include "../Source/dsp/VocoderEngine.h"
#include "TestSignals.h"

#include <cstdio>
#include <functional>
#include <string>

using namespace vvtest;

namespace
{
int failures = 0;
int checks = 0;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("    FAIL: %s\n", what.c_str());
    }
}

std::string fmt (const char* f, double a, double b = 0.0)
{
    char buf[256];
    std::snprintf (buf, sizeof (buf), f, a, b);
    return buf;
}

constexpr float kSr = 48000.0f;

vv::MidiEvent noteOn (int note, int offset = 0) { return { offset, 0x90, static_cast<uint8_t> (note), 100 }; }
vv::MidiEvent noteOff (int note, int offset = 0) { return { offset, 0x80, static_cast<uint8_t> (note), 0 }; }
vv::MidiEvent cc (int num, int value, int offset = 0) { return { offset, 0xB0, static_cast<uint8_t> (num), static_cast<uint8_t> (value) }; }

// Runs the engine over a whole signal in host-sized blocks; events land in the first block.
struct Render
{
    std::vector<float> left, right;
};

Render runEngine (vv::VocoderEngine& engine, const std::vector<float>& input, std::vector<vv::MidiEvent> events,
                  int blockSize = 256)
{
    Render r;
    r.left.resize (input.size());
    r.right.resize (input.size());
    const int total = static_cast<int> (input.size());
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min (blockSize, total - pos);
        std::vector<vv::MidiEvent> blockEvents;
        for (const auto& e : events)
            if (e.sampleOffset >= pos && e.sampleOffset < pos + n)
                blockEvents.push_back ({ e.sampleOffset - pos, e.status, e.data1, e.data2 });
        engine.process (input.data() + pos, r.left.data() + pos, r.right.data() + pos, n,
                        blockEvents.data(), static_cast<int> (blockEvents.size()));
    }
    return r;
}

vv::VocoderParams dryTestParams()
{
    vv::VocoderParams p;
    p.ensemble = 0.0f; // keep outputs mono-identical and easy to measure
    return p;
}

// ---------------------------------------------------------------------------

void testSvfBandpassUnityAtCentre()
{
    for (float f : { 200.0f, 1000.0f, 6000.0f })
    {
        vv::Svf svf;
        const auto c = vv::makeSvfCoeffs (f, 4.0f, kSr);
        const auto x = sine (f, kSr, 48000);
        std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i)
            y[i] = svf.bandpass (c, x[i]);
        const float gain = rms (y, 24000) / rms (x, 24000);
        check (std::abs (gain - 1.0f) < 0.02f, fmt ("bandpass gain at %.0f Hz = %.3f", f, gain));
    }
}

void testBankBandSelectivity()
{
    vv::VocoderBank bank;
    bank.prepare (kSr);
    vv::VocoderBank::Settings s;
    s.numBands = 16;
    bank.configure (s);

    int correct = 0;
    for (int band = 0; band < bank.getNumBands(); ++band)
    {
        bank.reset();
        const auto x = sine (bank.getCentre (band), kSr, 9600, 0.25f);
        for (size_t i = 0; i < 4800; ++i)
            bank.process (x[i], 0.0f, 1.0f);
        for (int b = 0; b < bank.getNumBands(); ++b)
            bank.takeLevel (b); // discard peaks held during the onset transient
        for (size_t i = 4800; i < x.size(); ++i)
            bank.process (x[i], 0.0f, 1.0f);
        int best = 0;
        float bestLevel = -1.0f;
        for (int b = 0; b < bank.getNumBands(); ++b)
        {
            const float l = bank.takeLevel (b);
            if (l > bestLevel)
            {
                bestLevel = l;
                best = b;
            }
        }
        correct += (best == band) ? 1 : 0;
    }
    check (correct == bank.getNumBands(), fmt ("sine at band centre peaks in its own band (%g/%g)", correct, bank.getNumBands()));
}

void testEnvelopeFollowerTimes()
{
    vv::EnvelopeFollower env;
    env.setTimes (0.01f, 0.1f, kSr);
    float v = 0.0f;
    for (int i = 0; i < 480 * 5; ++i) // 5 attack constants of a constant-amplitude signal
        v = env.process (1.0f);
    check (v > 0.95f && v < 1.01f, fmt ("follower settles to input level (%.3f)", v));
    for (int i = 0; i < 4800; ++i) // one release constant
        v = env.process (0.0f);
    check (v > 0.2f && v < 0.55f, fmt ("follower releases about 1/e per release time (%.3f)", v));
}

void testSynthPitchAndOctave()
{
    for (int octave : { 0, 1, -1 })
    {
        vv::CarrierSynth synth;
        synth.prepare (kSr);
        vv::CarrierSynth::Settings s;
        s.subLevel = 0.0f;
        s.detuneCents = 0.0f;
        s.octave = octave;
        synth.setSettings (s);
        synth.handleMidi (0x90, 69, 100);

        std::vector<float> y (48000);
        for (size_t i = 0; i < y.size(); i += vv::VocoderEngine::kChunkSize)
        {
            synth.beginChunk (vv::VocoderEngine::kChunkSize);
            for (size_t j = i; j < i + vv::VocoderEngine::kChunkSize && j < y.size(); ++j)
                y[j] = synth.renderSample();
        }
        const float expected = 440.0f * std::pow (2.0f, static_cast<float> (octave));
        const float f = estimateFrequency (y, kSr, 4800);
        check (std::abs (f - expected) < expected * 0.002f, fmt ("A4 octave shift -> %.2f Hz (expected %.1f)", f, expected));
    }
}

void renderSynth (vv::CarrierSynth& synth, int samples)
{
    for (int i = 0; i < samples; i += vv::VocoderEngine::kChunkSize)
    {
        synth.beginChunk (vv::VocoderEngine::kChunkSize);
        for (int j = 0; j < vv::VocoderEngine::kChunkSize; ++j)
            synth.renderSample();
    }
}

void testSynthVoiceManagement()
{
    vv::CarrierSynth synth;
    synth.prepare (kSr);
    vv::CarrierSynth::Settings s;
    s.releaseSeconds = 0.1f;
    synth.setSettings (s);

    for (int n = 48; n < 48 + 16; ++n)
        synth.handleMidi (0x90, static_cast<uint8_t> (n), 100);
    renderSynth (synth, 4800);
    check (synth.getActiveVoiceCount() == vv::CarrierSynth::kMaxVoices,
           fmt ("16 keys use all %g voices (%g)", vv::CarrierSynth::kMaxVoices, synth.getActiveVoiceCount()));

    for (int n = 48; n < 48 + 16; ++n)
        synth.handleMidi (0x80, static_cast<uint8_t> (n), 0);
    renderSynth (synth, 48000);
    check (synth.getActiveVoiceCount() == 0, fmt ("all voices free after release (%g)", synth.getActiveVoiceCount()));

    // Sustain pedal holds notes past note-off until released.
    synth.handleMidi (0xB0, 64, 127);
    synth.handleMidi (0x90, 60, 100);
    synth.handleMidi (0x80, 60, 0);
    renderSynth (synth, 24000);
    check (synth.getActiveVoiceCount() == 1, "sustain pedal holds the voice");
    synth.handleMidi (0xB0, 64, 0);
    renderSynth (synth, 24000);
    check (synth.getActiveVoiceCount() == 0, "pedal up releases the voice");

    // All-notes-off (CC123) as sent by Ableton when stopping.
    synth.handleMidi (0x90, 60, 100);
    synth.handleMidi (0x90, 64, 100);
    synth.handleMidi (0xB0, 123, 0);
    renderSynth (synth, 24000);
    check (synth.getActiveVoiceCount() == 0, "CC123 releases everything");
}

void testMonoLegato()
{
    vv::CarrierSynth synth;
    synth.prepare (kSr);
    vv::CarrierSynth::Settings s;
    s.mono = true;
    s.subLevel = 0.0f;
    s.detuneCents = 0.0f;
    synth.setSettings (s);

    synth.handleMidi (0x90, 57, 100); // A3
    synth.handleMidi (0x90, 69, 100); // A4 (legato on top)
    renderSynth (synth, 4800);
    check (synth.getActiveVoiceCount() == 1, "mono mode uses one voice");

    synth.handleMidi (0x80, 69, 0); // back to the held A3
    std::vector<float> y (24000);
    for (size_t i = 0; i < y.size(); i += vv::VocoderEngine::kChunkSize)
    {
        synth.beginChunk (vv::VocoderEngine::kChunkSize);
        for (size_t j = i; j < i + vv::VocoderEngine::kChunkSize; ++j)
            y[j] = synth.renderSample();
    }
    const float f = estimateFrequency (y, kSr, 2400);
    check (std::abs (f - 220.0f) < 1.0f, fmt ("mono returns to previous held note (%.2f Hz)", f));
}

void testSwitchingPolyToMonoWhileHolding()
{
    vv::CarrierSynth synth;
    synth.prepare (kSr);
    vv::CarrierSynth::Settings s;
    s.releaseSeconds = 0.05f;
    synth.setSettings (s);
    for (int n : { 48, 52, 55 })
        synth.handleMidi (0x90, static_cast<uint8_t> (n), 100);
    renderSynth (synth, 2400);

    s.mono = true; // user flips Mode while the chord is down
    synth.setSettings (s);
    for (int n : { 48, 52, 55 })
        synth.handleMidi (0x80, static_cast<uint8_t> (n), 0);
    renderSynth (synth, 24000);
    check (synth.getActiveVoiceCount() == 0, fmt ("no stuck voices after poly->mono switch (%g active)", synth.getActiveVoiceCount()));
}

void testSilenceWithoutMidi()
{
    vv::VocoderEngine engine;
    engine.setParams (dryTestParams());
    engine.prepare (kSr, 256);
    const auto voice = vowel ({ 700, 1200, 2600 }, 140.0f, kSr, 48000);
    const auto out = runEngine (engine, voice, {});
    check (peak (out.left) < 1.0e-6f, fmt ("voice without keys is silent (peak %.2e)", peak (out.left)));
}

void testSilenceWithoutVoice()
{
    vv::VocoderEngine engine;
    engine.setParams (dryTestParams());
    engine.prepare (kSr, 256);
    const std::vector<float> silence (48000, 0.0f);
    const auto out = runEngine (engine, silence, { noteOn (48), noteOn (55), noteOn (60) });
    check (peak (out.left) < 1.0e-6f, fmt ("keys without voice are silent (peak %.2e)", peak (out.left)));
}

void testGateBlocksBackgroundNoise()
{
    vv::VocoderEngine engine;
    auto p = dryTestParams();
    p.gateDb = -50.0f;
    engine.setParams (p);
    engine.prepare (kSr, 256);
    auto hiss = fricative (kSr, 48000, -70.0f); // room noise well below the gate
    const auto out = runEngine (engine, hiss, { noteOn (48) });
    check (rms (out.left, 24000) < vv::dbToGain (-90.0f), fmt ("noise under the gate stays closed (%.1f dB)", vv::gainToDb (rms (out.left, 24000))));
}

float vocodedLevelDb (int bands)
{
    vv::VocoderEngine engine;
    auto p = dryTestParams();
    p.numBands = bands;
    p.air = 0.0f;
    engine.setParams (p);
    engine.prepare (kSr, 256);
    const auto voice = vowel ({ 700, 1200, 2600 }, 140.0f, kSr, 96000, -20.0f);
    const auto out = runEngine (engine, voice, { noteOn (48) });
    return vv::gainToDb (rms (out.left, 24000));
}

void testOutputLevelCalibrated()
{
    const float level = vocodedLevelDb (16);
    std::printf ("    16 bands: vowel at -20 dBFS RMS + C3 -> %.1f dBFS RMS\n", level);
    check (level > -26.0f && level < -14.0f, fmt ("default output level near input level (%.1f dB)", level));

    float lo = 1.0e9f, hi = -1.0e9f;
    for (int bands : { 8, 10, 12, 16, 20, 24 })
    {
        const float l = vocodedLevelDb (bands);
        std::printf ("    %2d bands -> %.1f dBFS\n", bands, l);
        lo = std::min (lo, l);
        hi = std::max (hi, l);
    }
    check (hi - lo < 6.0f, fmt ("band count changes level by < 6 dB (spread %.1f dB)", hi - lo));
}

void testFormantShiftMovesSpectrum()
{
    auto brightnessFor = [] (float semis)
    {
        vv::VocoderEngine engine;
        auto p = dryTestParams();
        p.formantSemitones = semis;
        p.air = 0.0f;
        p.sibilance = 0.0f;
        engine.setParams (p);
        engine.prepare (kSr, 256);
        const auto voice = vowel ({ 700, 1200, 2600 }, 140.0f, kSr, 48000);
        const auto out = runEngine (engine, voice, { noteOn (48) });
        return highBandRatio (out.left, 1500.0f, kSr, 24000);
    };
    const float down = brightnessFor (-12.0f), mid = brightnessFor (0.0f), up = brightnessFor (12.0f);
    check (down < mid && mid < up, fmt ("formant shift raises brightness (-12: %.3f, +12: %.3f)", down, up));
}

void testSibilanceAddsNoiseForFricatives()
{
    auto hfFor = [] (float sibilance)
    {
        vv::VocoderEngine engine;
        auto p = dryTestParams();
        p.sibilance = sibilance;
        p.air = 0.0f;
        engine.setParams (p);
        engine.prepare (kSr, 256);
        const auto sss = fricative (kSr, 48000, -26.0f);
        const auto out = runEngine (engine, sss, { noteOn (48) });
        return rms (out.left, 12000);
    };
    const float without = hfFor (0.0f), with = hfFor (1.0f);
    check (with > without * 2.0f, fmt ("sibilance makes 's' louder (%.2e -> %.2e)", without, with));
}

void testSampleAccurateMidi()
{
    vv::VocoderEngine engine;
    auto p = dryTestParams();
    p.carrierListen = true;
    p.ampAttackMs = 1.0f;
    engine.setParams (p);
    engine.prepare (kSr, 512);
    std::vector<float> in (512, 0.0f), l (512), r (512);
    const vv::MidiEvent ev = noteOn (60, 100);
    engine.process (in.data(), l.data(), r.data(), 512, &ev, 1);

    float before = 0.0f, after = 0.0f;
    for (int i = 0; i < 100; ++i)
        before = std::max (before, std::abs (l[static_cast<size_t> (i)]));
    for (int i = 100; i < 512; ++i)
        after = std::max (after, std::abs (l[static_cast<size_t> (i)]));
    check (before == 0.0f && after > 0.01f, fmt ("note starts exactly at its offset (before %.2e, after %.3f)", before, after));
    check (engine.getMidiEventCount() == 1, "MIDI activity counter increments");
}

void testRobustnessAcrossRatesAndExtremes()
{
    vv::Random rng { 4242u };
    for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int len = static_cast<int> (sr * 0.5);
        std::vector<float> noise (static_cast<size_t> (len));
        for (auto& v : noise)
            v = rng.nextBipolar();

        for (int variant = 0; variant < 3; ++variant)
        {
            vv::VocoderParams p;
            p.inputGainDb = 24.0f;
            p.outputGainDb = variant == 0 ? 12.0f : 0.0f;
            p.numBands = variant == 1 ? 8 : 24;
            p.resonance = variant == 2 ? 4.0f : 0.5f;
            p.formantSemitones = variant == 0 ? 12.0f : -12.0f;
            p.attackMs = 0.5f;
            p.releaseMs = 5.0f;
            p.brightnessDbPerOct = 12.0f;
            p.pulseLevel = p.subLevel = p.noiseLevel = p.sibilance = p.air = p.ensemble = p.dryVoice = 1.0f;
            p.detuneCents = 50.0f;
            p.octave = 2;
            p.vibratoDepth = 1.0f;
            p.mono = variant == 2;
            p.glideMs = variant == 2 ? 500.0f : 0.0f;

            vv::VocoderEngine engine;
            engine.setParams (p);
            engine.prepare (sr, 512);
            std::vector<vv::MidiEvent> ev;
            for (int n = 24; n < 108; n += 7)
                ev.push_back (noteOn (n));
            ev.push_back ({ 0, 0xE0, 0x7F, 0x7F }); // full bend up
            const auto out = runEngine (engine, noise, ev, 333);

            bool finite = true;
            for (size_t i = 0; i < out.left.size(); ++i)
                finite = finite && std::isfinite (out.left[i]) && std::isfinite (out.right[i]);
            check (finite, fmt ("finite output at %.0f Hz (variant %g)", sr, variant));
            check (peak (out.left) <= 1.0f && peak (out.right) <= 1.0f, fmt ("soft clipper bounds output at %.0f Hz (variant %g)", sr, variant));
        }
    }
}

void testEnsembleKeepsLevel()
{
    vv::Ensemble ens;
    ens.prepare (kSr);
    const auto x = sine (440.0f, kSr, 48000, 0.5f);
    std::vector<float> l (x.size()), r (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        ens.process (x[i], 1.0f, l[i], r[i]);
    const float ratioDb = vv::gainToDb (rms (l, 4800) / rms (x, 4800));
    check (std::abs (ratioDb) < 3.0f, fmt ("full ensemble within 3 dB of dry (%.2f dB)", ratioDb));

    double diff = 0.0;
    for (size_t i = 4800; i < x.size(); ++i)
        diff += std::abs (l[i] - r[i]);
    check (diff > 1.0, "ensemble produces stereo width");
}
} // namespace

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        { "svf band-pass unity at centre", testSvfBandpassUnityAtCentre },
        { "bank band selectivity", testBankBandSelectivity },
        { "envelope follower times", testEnvelopeFollowerTimes },
        { "synth pitch and octave", testSynthPitchAndOctave },
        { "synth voice management", testSynthVoiceManagement },
        { "mono legato", testMonoLegato },
        { "poly->mono switch while holding", testSwitchingPolyToMonoWhileHolding },
        { "silence without MIDI", testSilenceWithoutMidi },
        { "silence without voice", testSilenceWithoutVoice },
        { "gate blocks background noise", testGateBlocksBackgroundNoise },
        { "output level is calibrated", testOutputLevelCalibrated },
        { "formant shift moves spectrum", testFormantShiftMovesSpectrum },
        { "sibilance adds noise for fricatives", testSibilanceAddsNoiseForFricatives },
        { "sample-accurate MIDI", testSampleAccurateMidi },
        { "robust across rates and extremes", testRobustnessAcrossRatesAndExtremes },
        { "ensemble keeps level", testEnsembleKeepsLevel },
    };

    for (const auto& [name, fn] : tests)
    {
        const int before = failures;
        std::printf ("[ RUN  ] %s\n", name);
        fn();
        std::printf ("%s %s\n", failures == before ? "[  OK  ]" : "[ FAIL ]", name);
    }

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
