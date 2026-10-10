#pragma once

#include "DspCommon.h"

namespace vv
{
// RMS envelope follower with separate attack/release, followed by one extra
// smoothing pole to suppress the 2f ripple of the squared signal. Times are
// expressed in the amplitude domain (the mean-square domain runs twice as fast).
struct EnvelopeFollower
{
    float meanSquare = 0.0f, smoothed = 0.0f;
    float attackCoef = 1.0f, releaseCoef = 1.0f, smoothCoef = 1.0f;

    void setTimes (float attackSeconds, float releaseSeconds, float sampleRate)
    {
        attackCoef = onePoleCoef (0.5f * attackSeconds, sampleRate);
        releaseCoef = onePoleCoef (0.5f * releaseSeconds, sampleRate);
        smoothCoef = onePoleCoef (0.5f * attackSeconds, sampleRate);
    }

    void reset() { meanSquare = smoothed = 0.0f; }

    inline float process (float x)
    {
        const float x2 = x * x;
        meanSquare += (x2 > meanSquare ? attackCoef : releaseCoef) * (x2 - meanSquare);
        if (meanSquare < 1.0e-18f)
            meanSquare = 0.0f; // keep denormals out of the feedback path
        smoothed += smoothCoef * (std::sqrt (meanSquare) - smoothed);
        return smoothed;
    }
};
} // namespace vv
