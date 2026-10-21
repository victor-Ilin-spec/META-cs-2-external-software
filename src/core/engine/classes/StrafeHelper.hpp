#pragma once

struct StrafeDebug {
    float dyaw = 0.f;
    float yaw = 0.f;
    int held = 0;
    bool on_ground = false;
    bool space = false;
    bool ticking = false;
};

class StrafeHelper {
public:
    static void Tick();
    static StrafeDebug Debug();
};
