#pragma once

#include <algorithm>
#include <cmath>

namespace tick {

constexpr float kHz = 64.f;
constexpr float kSec = 1.f / kHz;
constexpr float kMs = 1000.f / kHz;

inline float ClampTicks(float ticks)
{
    return std::clamp(ticks, 1.f, 12.f);
}

// Old slider 1–150 was a speed curve, not ticks. Map into 1–12.
inline float FromLegacySlider(float raw)
{
    if (raw <= 12.f)
        return ClampTicks((std::max)(1.f, raw));
    const float t = std::clamp((raw - 1.f) / 149.f, 0.f, 1.f);
    return ClampTicks(1.f + t * 11.f);
}

inline float TauSec(float ticks)
{
    return ClampTicks(ticks) * kSec;
}

inline float StepK(float dt_sec, float ticks)
{
    const float tau = (std::max)(kSec, TauSec(ticks));
    const float k = 1.f - std::exp(-dt_sec / tau);
    const float k_tick_max = 1.f - std::exp(-kSec / tau);
    return std::clamp(k, 0.f, k_tick_max);
}

struct Quantizer {
    float acc = 0.f;

    int Consume(float dt_sec)
    {
        acc += std::clamp(dt_sec, 0.0005f, 0.032f);
        int n = 0;
        while (acc >= kSec && n < 4)
        {
            acc -= kSec;
            ++n;
        }
        return n;
    }
};

} // namespace tick
