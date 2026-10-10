#pragma once

#include "DspCommon.h"

#include <vector>

namespace vv
{
// String-machine style ensemble (Roland VP-330 / Solina): three modulated
// delay taps, each driven by a slow + fast LFO pair spaced 120 degrees apart,
// spread left / centre / right. A gentle low-pass on the wet path imitates
// the bandwidth of the original bucket-brigade delay lines.
class Ensemble
{
public:
    void prepare (float newSampleRate)
    {
        sampleRate = newSampleRate;
        size_t size = 1;
        while (size < static_cast<size_t> (0.05f * sampleRate))
            size <<= 1;
        buffer.assign (size, 0.0f);
        mask = size - 1;
        wetLowpass = onePoleCoef (1.0f / (kTwoPi * 7000.0f), sampleRate);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        slowPhase = fastPhase = 0.0f;
        lpL = lpR = 0.0f;
    }

    inline void process (float x, float mix, float& outL, float& outR)
    {
        buffer[writePos] = x;

        float taps[3];
        for (int i = 0; i < 3; ++i)
        {
            const float offset = static_cast<float> (i) / 3.0f;
            const float delayMs = kBaseMs
                                  + kSlowDepthMs * std::sin (kTwoPi * (slowPhase + offset))
                                  + kFastDepthMs * std::sin (kTwoPi * (fastPhase + offset));
            taps[i] = read (delayMs * 0.001f * sampleRate);
        }

        slowPhase += kSlowHz / sampleRate;
        fastPhase += kFastHz / sampleRate;
        slowPhase -= std::floor (slowPhase);
        fastPhase -= std::floor (fastPhase);
        writePos = (writePos + 1) & mask;

        const float wetL = taps[0] + 0.5f * taps[1];
        const float wetR = taps[2] + 0.5f * taps[1];
        lpL += wetLowpass * (wetL - lpL);
        lpR += wetLowpass * (wetR - lpR);

        const float dryGain = 1.0f - 0.5f * mix;
        const float wetGain = 0.6f * mix;
        outL = x * dryGain + lpL * wetGain;
        outR = x * dryGain + lpR * wetGain;
    }

private:
    static constexpr float kBaseMs = 8.0f;
    static constexpr float kSlowDepthMs = 2.5f;
    static constexpr float kFastDepthMs = 0.3f;
    static constexpr float kSlowHz = 0.63f;
    static constexpr float kFastHz = 5.9f;

    // 4-point Hermite interpolation of the delay line.
    inline float read (float delaySamples) const
    {
        const float pos = static_cast<float> (writePos) - delaySamples;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const auto i = static_cast<size_t> (static_cast<long> (fl) + static_cast<long> (buffer.size()));

        const float xm1 = buffer[(i - 1) & mask];
        const float x0 = buffer[i & mask];
        const float x1 = buffer[(i + 1) & mask];
        const float x2 = buffer[(i + 2) & mask];

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    float sampleRate = 44100.0f;
    std::vector<float> buffer;
    size_t mask = 0, writePos = 0;
    float slowPhase = 0.0f, fastPhase = 0.0f;
    float wetLowpass = 1.0f, lpL = 0.0f, lpR = 0.0f;
};
} // namespace vv
