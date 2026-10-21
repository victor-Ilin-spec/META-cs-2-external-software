#pragma once

#include "core/engine/types/Vec3.hpp"

#include <chrono>

class RCS {
public:
    static void Tick();
    static bool OwnsY();

private:
    static RCS& Get();

    void Update();
    void MoveMouse(float x, float y);
    void Reset();

    uintptr_t ResolvePawn() const;
    Vec3_t ReadPunch(uintptr_t pawn) const;
    float ReadSensitivity() const;
    float Ramp(int shots) const;

    Vec3_t old_punch{};
    float acc_x = 0.f;
    float acc_y = 0.f;
    float remain_x = 0.f;
    float remain_y = 0.f;
    int last_shots = 0;
    std::chrono::steady_clock::time_point last_t{};
    std::chrono::steady_clock::time_point spray_t{};
    std::chrono::steady_clock::time_point last_shot_t{};
    std::chrono::steady_clock::time_point last_send_t{};
    bool have_clock = false;
    bool spraying = false;
};
