#pragma once

#include "DspCommon.h"

namespace vv
{
// Topology-preserving-transform state variable filter (Zavalishin / Simper).
// Stable under fast coefficient changes, which matters because the carrier
// bank is retuned continuously when the formant knob moves.
struct SvfCoeffs
{
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
};

inline SvfCoeffs makeSvfCoeffs (float freqHz, float q, float sampleRate)
{
    freqHz = std::clamp (freqHz, 10.0f, 0.45f * sampleRate);
    q = std::max (q, 0.05f);

    SvfCoeffs c;
    const float g = std::tan (kPi * freqHz / sampleRate);
    c.k = 1.0f / q;
    c.a1 = 1.0f / (1.0f + g * (g + c.k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    return c;
}

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() { ic1 = ic2 = 0.0f; }

    inline void tick (const SvfCoeffs& c, float v0, float& band, float& low)
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        band = v1;
        low = v2;
    }

    // Band-pass normalised to 0 dB at the centre frequency.
    inline float bandpass (const SvfCoeffs& c, float x)
    {
        float band, low;
        tick (c, x, band, low);
        return c.k * band;
    }

    inline float lowpass (const SvfCoeffs& c, float x)
    {
        float band, low;
        tick (c, x, band, low);
        return low;
    }

    inline float highpass (const SvfCoeffs& c, float x)
    {
        float band, low;
        tick (c, x, band, low);
        return x - c.k * band - low;
    }
};
} // namespace vv
