#pragma once

// Synthetic test material: no audio files needed in the repo.

#include "../Source/dsp/Svf.h"

#include <cmath>
#include <vector>

namespace vvtest
{
inline float rms (const std::vector<float>& x, size_t from = 0)
{
    double acc = 0.0;
    for (size_t i = from; i < x.size(); ++i)
        acc += static_cast<double> (x[i]) * x[i];
    const size_t n = x.size() > from ? x.size() - from : 1;
    return static_cast<float> (std::sqrt (acc / static_cast<double> (n)));
}

inline float peak (const std::vector<float>& x)
{
    float p = 0.0f;
    for (float v : x)
        p = std::max (p, std::abs (v));
    return p;
}

inline void normaliseRms (std::vector<float>& x, float targetDb)
{
    const float r = rms (x);
    if (r <= 0.0f)
        return;
    const float g = vv::dbToGain (targetDb) / r;
    for (auto& v : x)
        v *= g;
}

inline std::vector<float> sine (float freq, float sampleRate, int numSamples, float amp = 1.0f)
{
    std::vector<float> x (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
        x[static_cast<size_t> (i)] = amp * std::sin (vv::kTwoPi * freq * static_cast<float> (i) / sampleRate);
    return x;
}

// Glottal-pulse-like source (band-limited-ish saw at f0) through three formant
// resonators: a crude but spectrally plausible sung vowel.
struct Vowel
{
    float f1, f2, f3;
};

inline std::vector<float> vowel (Vowel v, float f0, float sampleRate, int numSamples, float levelDb = -20.0f)
{
    vv::Svf r1, r2, r3;
    const auto c1 = vv::makeSvfCoeffs (v.f1, 8.0f, sampleRate);
    const auto c2 = vv::makeSvfCoeffs (v.f2, 10.0f, sampleRate);
    const auto c3 = vv::makeSvfCoeffs (v.f3, 12.0f, sampleRate);

    std::vector<float> x (static_cast<size_t> (numSamples));
    float phase = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        // slight vibrato keeps it from being perfectly static
        const float f = f0 * (1.0f + 0.01f * std::sin (vv::kTwoPi * 5.0f * static_cast<float> (i) / sampleRate));
        phase += f / sampleRate;
        phase -= std::floor (phase);
        const float src = 1.0f - 2.0f * phase;
        x[static_cast<size_t> (i)] = r1.bandpass (c1, src) + 0.5f * r2.bandpass (c2, src) + 0.25f * r3.bandpass (c3, src);
    }
    normaliseRms (x, levelDb);
    return x;
}

// "sss": high-passed white noise.
inline std::vector<float> fricative (float sampleRate, int numSamples, float levelDb = -26.0f)
{
    vv::Random rng { 777u };
    vv::Svf a, b;
    const auto c = vv::makeSvfCoeffs (5000.0f, 0.707f, sampleRate);
    std::vector<float> x (static_cast<size_t> (numSamples));
    for (auto& v : x)
        v = b.highpass (c, a.highpass (c, rng.nextBipolar()));
    normaliseRms (x, levelDb);
    return x;
}

// Fraction of energy above cutoffHz (2-pole high-pass split): a cheap brightness proxy.
inline float highBandRatio (const std::vector<float>& x, float cutoffHz, float sampleRate, size_t from = 0)
{
    vv::Svf hp;
    const auto c = vv::makeSvfCoeffs (cutoffHz, 0.707f, sampleRate);
    std::vector<float> h (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        h[i] = hp.highpass (c, x[i]);
    const float total = rms (x, from);
    return total > 0.0f ? rms (h, from) / total : 0.0f;
}

// Frequency estimate from positive-going zero crossings with linear interpolation.
inline float estimateFrequency (const std::vector<float>& x, float sampleRate, size_t from = 0)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = static_cast<double> (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    if (crossings < 2)
        return 0.0f;
    return static_cast<float> ((crossings - 1) * sampleRate / (last - first));
}
} // namespace vvtest
