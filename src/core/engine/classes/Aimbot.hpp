#pragma once

#include "core/engine/types/Types.hpp"

#include <vector>

struct Snapshot;
class Player;

struct AimSeg {
    Vec3_t a{};
    Vec3_t b{};
    float r = 0.f;
};

class Aimbot {
public:
    static void Tick();
    static bool Held();
    static float FovRadiusPx(float screen_w, float fov_deg = -1.f, int zoom = 0);
    static Vec2_t CurveStep(float dx, float dy, float dt, float smooth);
    static int SelectedBone();
    static bool InKillDelay(const Snapshot& snapshot, uintptr_t current_target, int current_hp);
    static void GetViewAxes(const view_matrix_t& vm, Vec3_t& right, Vec3_t& up);
    static const std::vector<Vec3_t>& BuildAimPoints(const Player& player, short weapon_id,
        const Vec3_t& view_right, const Vec3_t& view_up);
    static void BuildAimCapsules(const Player& player, short weapon_id, std::vector<AimSeg>& out);
};
