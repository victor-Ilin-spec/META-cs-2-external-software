#pragma once
#include "core/engine/types/Vec3.hpp"
#include "core/engine/types/Weapons.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

struct WorldItem {
    uintptr_t entity = 0;
    Vec3_t origin{};
    short weapon_id = -1;
    int ammo = 0;
    int max_ammo = 0;
};

struct WorldProjectile {
    enum class Type : uint8_t {
        unknown,
        he,
        flash,
        smoke,
        molotov,
        inferno,
        decoy
    };

    uintptr_t entity = 0;
    Vec3_t origin{};
    Vec3_t velocity{};
    Vec3_t initial_pos{};
    Vec3_t initial_vel{};
    uint32_t thrower_handle = 0;
    int bounces = 0;
    int effect_tick_begin = 0;
    bool detonated = false;
    bool smoke_active = false;
    float expire_time = 0.f;
    float curtime = 0.f;
    Type type = Type::unknown;
    std::array<Vec3_t, 64> fire_points{};
    int fire_count = 0;
};

struct WorldSnapshot {
    std::vector<WorldItem> items;
    std::vector<WorldProjectile> projectiles;
    float curtime = 0.f;
    uintptr_t local_pawn_handle = 0;
};

class World {
public:
    static void Tick();
    static WorldSnapshot Copy();

private:
    static World& Get();
    void TickImpl();
    WorldSnapshot CopyImpl() const;

    mutable std::mutex mtx;
    WorldSnapshot current{};
};
