#pragma once

#include <cstdint>

namespace Humanize
{
    void Reset();
    void Begin(float x0, float y0, float x1, float y1);
    bool Active();
    bool Tick(float& out_dx, float& out_dy);
    void Tint(float& dx, float& dy, float dt_sec);
}
