#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vv
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float gain) { return 20.0f * std::log10 (std::max (gain, 1.0e-9f)); }

// Coefficient for the one-pole smoother y += c * (x - y) reaching ~63% after timeSeconds.
inline float onePoleCoef (float timeSeconds, float sampleRate)
{
    if (timeSeconds <= 0.0f)
        return 1.0f;
    return 1.0f - std::exp (-1.0f / (timeSeconds * sampleRate));
}

// Change detection for parameter values (exact == trips -Wfloat-equal).
inline bool differs (float a, float b) { return std::abs (a - b) > 1.0e-7f * std::max (1.0f, std::abs (a)); }

inline float smoothstep (float edge0, float edge1, float x)
{
    const float t = std::clamp ((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Gentle saturation that is exactly linear below the knee, so normal levels are untouched.
inline float softClip (float x)
{
    constexpr float knee = 0.8f;
    const float a = std::abs (x);
    if (a <= knee)
        return x;
    const float over = (a - knee) / (1.0f - knee);
    return std::copysign (knee + (1.0f - knee) * std::tanh (over), x);
}

// Cheap LCG noise: deterministic, allocation-free and good enough for audio noise.
struct Random
{
    uint32_t state = 0x12345678u;

    float nextBipolar()
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float> (static_cast<int32_t> (state)) * (1.0f / 2147483648.0f);
    }

    float nextUnipolar() { return 0.5f * (nextBipolar() + 1.0f); }
};

// Per-sample one-pole parameter smoother used for gains, so knob moves never click.
struct SmoothedValue
{
    float current = 0.0f, target = 0.0f, coef = 1.0f;

    void prepare (float sampleRate, float timeSeconds = 0.02f) { coef = onePoleCoef (timeSeconds, sampleRate); }
    void setTarget (float t) { target = t; }
    void snap (float v) { current = target = v; }
    float next() { return current += coef * (target - current); }
};
} // namespace vv
