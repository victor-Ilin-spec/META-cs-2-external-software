#pragma once
#include "World.hpp"
#include "core/engine/types/Vec3.hpp"
#include <chrono>
#include <cstdint>
#include <mutex>

#include <vector>

class GrenadePredict {
public:
    static void Tick();
    static void Render();

private:
    struct Trajectory {
        std::vector<Vec3_t> points;
        std::vector<Vec3_t> bounces;
        Vec3_t end{};
        float duration = 0.f;
        int end_tick = -1;
        bool valid = false;
    };

    struct InFlight {
        uintptr_t entity = 0;
        uint32_t weapon_hash = 0;
        Trajectory traj{};
        std::chrono::steady_clock::time_point throw_time{};
        std::chrono::steady_clock::time_point detonate_time{};
        std::chrono::steady_clock::time_point last_seen{};
        bool detonated = false;
        bool effect_started = false;
        bool corrected = false;
    };

    static GrenadePredict& Get();
    void TickImpl();
    void RenderImpl();
    bool CanPredict(const WorldSnapshot& world) const;
    void UpdateWeapon(short weapon_id);
    void SetupThrow(Vec3_t& origin, Vec3_t& velocity) const;
    void UpdateInFlight(const WorldSnapshot& world);
    uint32_t HashFromType(WorldProjectile::Type type) const;
    void Simulate(const Vec3_t& start, const Vec3_t& velocity, Trajectory& out) const;
    void Step(Vec3_t& pos, Vec3_t& vel, bool& detonated, Vec3_t& hit_n, bool& hit) const;
    void Resolve(const Vec3_t& nrm, Vec3_t& pos, Vec3_t& vel, bool& detonated) const;
    bool ShouldDetonate(const Vec3_t& vel, int tick) const;
    static Vec3_t ClipVelocity(const Vec3_t& velocity, const Vec3_t& normal, float overbounce);
    void DrawTrajectory(const Trajectory& traj, float alpha) const;

    std::mutex mtx;

    uint32_t weapon_hash = 0;
    float throw_velocity = 750.f;
    float detonate_time = 1.5f;
    float velocity_threshold = 0.1f;
    std::vector<InFlight> in_flight;
    std::chrono::steady_clock::time_point last_throw{};
    bool was_holding = false;
    bool pin_pulled = false;
    float throw_strength = 1.f;
    Vec3_t eye{};
    Vec3_t angles{};
    Vec3_t player_vel{};
    short local_weapon = -1;

    static constexpr float kGravity = 800.f * 0.4f;
    static constexpr float kElasticity = 0.45f;
    static constexpr int kMaxTicks = 1024;
    static constexpr int kTicksPerPoint = 4;
    static constexpr float kHull = 2.f;
    static constexpr float kPullBack = 6.f;
    static constexpr float kForward = 22.f;
    static constexpr float kInherit = 1.25f;
    static constexpr float kStopSq = 400.f;
    static constexpr float kTick = 1.f / 64.f;
    static constexpr float kMolotovSlopeZ = 0.1736f;
};
