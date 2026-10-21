#pragma once

#include "core/engine/types/Matrix.hpp"
#include "core/engine/types/Vec3.hpp"

#include <cstdint>

// Render-facing snapshot. No heap, no RPM buffers, no mutex.
// Logic thread writes a free slot; render only reads the published one.
struct FrameBone {
    Vec3_t pos{};
    float qx = 0.f;
    float qy = 0.f;
    float qz = 0.f;
    float qw = 1.f;
};

struct FramePlayer {
    static constexpr int kBones = 30;

    uintptr_t pawn = 0;
    uintptr_t controller = 0;
    int index = -1;
    int team = 0;
    int health = 0;
    int armor = 0;
    int ammo = 0;
    int pawn_handle = 0;
    short weapon_id = -1;
    bool alive = false;
    bool localplayer = false;
    bool visible = false;
    bool spotted = false;
    bool can_hit = false;
    bool scoped = false;
    bool flashed = false;
    bool defusing = false;
    bool has_c4 = false;
    bool reloading = false;
    bool has_helmet = false;
    bool has_defuser = false;
    bool immune = false;
    Vec3_t pos{};
    Vec3_t vel{};
    Vec3_t eye{};
    char name[32]{};
    int bone_count = 0;
    FrameBone bones[kBones]{};
};

struct FrameData {
    static constexpr int kMaxPlayers = 64;

    uint64_t seq = 0;
    int tick = 0;
    view_matrix_t matrix{};

    int local_index = -1;
    int local_team = 0;
    int local_health = 0;
    short local_weapon = -1;
    Vec3_t local_eye{};
    bool local_alive = false;
    bool local_scoped = false;
    int local_zoom = 0;
    bool in_match = false;
    bool is_deathmatch = false;
    char map[32]{};

    int player_count = 0;
    FramePlayer players[kMaxPlayers]{};
};
