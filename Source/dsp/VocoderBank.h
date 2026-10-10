#pragma once

#include "EnvelopeFollower.h"
#include "Svf.h"

#include <array>

namespace vv
{
// Classic channel-vocoder core: matched analysis (modulator) and synthesis
// (carrier) band-pass banks, one envelope follower per band acting as the VCA
// control. Each band is 4th order (two cascaded 2nd-order sections), like the
// 24 dB/oct band filters of the 70s hardware.
class VocoderBank
{
public:
    static constexpr int kMaxBands = 24;
    // Band span borrowed from the Sennheiser VSM-201 (also Surge XT's default).
    static constexpr float kLowestBandHz = 180.0f;
    static constexpr float kHighestBandHz = 7500.0f;

    struct Settings
    {
        int numBands = 16;
        float resonance = 1.0f;      // Q multiplier, 1 = adjacent bands cross at about -3 dB
        float formantRatio = 1.0f;   // carrier band frequency / modulator band frequency
        float attackSeconds = 0.004f;
        float releaseSeconds = 0.06f;
        float brightnessDbPerOct = 3.0f; // tilt around 1 kHz, compensates speech/saw roll-off
    };

    void prepare (float newSampleRate)
    {
        sampleRate = newSampleRate;
        configured = false;
        configure (settings);
        reset();
    }

    void reset()
    {
        for (size_t b = 0; b < kMaxBands; ++b)
        {
            modA[b].reset();
            modB[b].reset();
            carA[b].reset();
            carB[b].reset();
            envelopes[b].reset();
            levels[b] = 0.0f;
        }
    }

    // Cheap to call every block: only recomputes what actually changed.
    void configure (const Settings& s)
    {
        const int n = std::clamp (s.numBands, 2, kMaxBands);
        const bool countChanged = ! configured || n != settings.numBands;
        const bool layoutChanged = countChanged || differs (s.resonance, settings.resonance);
        const bool carrierChanged = layoutChanged || differs (s.formantRatio, settings.formantRatio);
        const bool envChanged = layoutChanged || differs (s.attackSeconds, settings.attackSeconds)
                                || differs (s.releaseSeconds, settings.releaseSeconds);
        const bool gainChanged = layoutChanged || differs (s.brightnessDbPerOct, settings.brightnessDbPerOct);
        const auto count = static_cast<size_t> (n);

        settings = s;
        settings.numBands = n;
        configured = true;

        if (layoutChanged)
        {
            const float top = std::min (kHighestBandHz, 0.42f * sampleRate);
            const float ratio = std::pow (top / kLowestBandHz, 1.0f / static_cast<float> (n - 1));
            // -3 dB bandwidth of one band equal to the band spacing...
            const float qTarget = 1.0f / (std::sqrt (ratio) - 1.0f / std::sqrt (ratio));
            // ...then widen each section so the cascade of two keeps that bandwidth.
            sectionQ = qTarget * 0.6436f * settings.resonance;

            for (size_t b = 0; b < count; ++b)
            {
                centres[b] = kLowestBandHz * std::pow (ratio, static_cast<float> (b));
                modCoeffs[b] = makeSvfCoeffs (centres[b], sectionQ, sampleRate);
            }
        }

        if (carrierChanged)
            for (size_t b = 0; b < count; ++b)
                carCoeffs[b] = makeSvfCoeffs (centres[b] * settings.formantRatio, sectionQ, sampleRate);

        if (envChanged)
            for (size_t b = 0; b < count; ++b)
            {
                // Low bands cannot follow faster than their own period without ripple.
                const float period = 1.0f / centres[b];
                envelopes[b].setTimes (std::max (settings.attackSeconds, period),
                                       std::max (settings.releaseSeconds, 2.0f * period),
                                       sampleRate);
            }

        if (gainChanged)
        {
            // Summing N bands of a broadband signal through N envelopes scales
            // the output by ~1/sqrt(N); compensate so band count is a timbre
            // choice, not a volume knob.
            const float countComp = std::sqrt (static_cast<float> (n));
            for (size_t b = 0; b < count; ++b)
                bandGain[b] = countComp * dbToGain (settings.brightnessDbPerOct * std::log2 (centres[b] / 1000.0f));
        }

        if (countChanged)
            reset();
    }

    // modulator: voice sample, carrier: synth sample, gate: 0..1 global gate gain.
    inline float process (float modulator, float carrier, float gate)
    {
        float sum = 0.0f;
        const auto count = static_cast<size_t> (settings.numBands);

        for (size_t b = 0; b < count; ++b)
        {
            float m = modA[b].bandpass (modCoeffs[b], modulator);
            m = modB[b].bandpass (modCoeffs[b], m);
            const float env = envelopes[b].process (m) * gate;
            levels[b] = std::max (levels[b], env);

            // Apply the envelope before the carrier band filter so envelope
            // ripple sidebands are filtered out instead of heard as buzz.
            float c = carrier * env * bandGain[b];
            c = carA[b].bandpass (carCoeffs[b], c);
            c = carB[b].bandpass (carCoeffs[b], c);
            sum += c;
        }

        return sum;
    }

    int getNumBands() const { return settings.numBands; }
    float getCentre (int band) const { return centres[static_cast<size_t> (band)]; }

    // Peak envelope per band since the last call; used for the UI meter.
    float takeLevel (int band)
    {
        const float l = levels[static_cast<size_t> (band)];
        levels[static_cast<size_t> (band)] = 0.0f;
        return l;
    }

private:
    float sampleRate = 44100.0f;
    bool configured = false;
    Settings settings;
    float sectionQ = 1.0f;

    std::array<float, kMaxBands> centres {}, bandGain {}, levels {};
    std::array<SvfCoeffs, kMaxBands> modCoeffs {}, carCoeffs {};
    std::array<Svf, kMaxBands> modA {}, modB {}, carA {}, carB {};
    std::array<EnvelopeFollower, kMaxBands> envelopes {};
};
} // namespace vv
