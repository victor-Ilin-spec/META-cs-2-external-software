#include "GrenadePredict.hpp"
#include "Schema.hpp"
#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/cache/FrameStore.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"
#include "core/engine/types/Matrix.hpp"
#include "core/offsets/Offsets.hpp"

#include "core/engine/types/Vec2.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

Vec3_t AnglesToForward(const Vec3_t& ang)
{
    const float pitch = ang.x * (kPi / 180.f);
    const float yaw = ang.y * (kPi / 180.f);
    return {
        std::cosf(pitch) * std::cosf(yaw),
        std::cosf(pitch) * std::sinf(yaw),
        -std::sinf(pitch)
    };
}

} // namespace

GrenadePredict& GrenadePredict::Get()
{
    static GrenadePredict i;
    return i;
}

void GrenadePredict::Tick()
{
    auto& self = Get();
    std::lock_guard<std::mutex> lock(self.mtx);
    self.TickImpl();
}

void GrenadePredict::Render()
{
    auto& self = Get();
    std::lock_guard<std::mutex> lock(self.mtx);
    self.RenderImpl();
}

void GrenadePredict::TickImpl()
{
    if (!cfg::world::grenade_pred)
    {
        in_flight.clear();
        return;
    }

    const auto world = World::Copy();
    const auto& snap = Cache::Current();
    if (!snap.local.alive)
    {
        in_flight.clear();
        was_holding = false;
        return;
    }

    local_weapon = snap.local.weapon.item_index;
    eye = snap.local.EyePos();
    angles = snap.local.eye_angles;
    player_vel = snap.local.vel;

    pin_pulled = false;
    throw_strength = 1.f;
    auto p = Engine::GetProcess();
    if (p && snap.local.PawnAddress())
    {
        const int off_ws = offsets::pawn::m_pWeaponServices;
        const int off_aw = offsets::pawn::m_hActiveWeapon;
        const auto ws = p->read<uintptr_t>(snap.local.PawnAddress() + off_ws);
        if (ws)
        {
            const auto handle = p->read<uint32_t>(ws + off_aw);
            const auto el = snap.game.entity_list;
            if (el && handle)
            {
                const auto bucket = p->read<uintptr_t>(el + 0x10 + 0x8 * ((handle & 0x7FFF) >> 9));
                if (bucket)
                {
                    const auto weapon = p->read<uintptr_t>(bucket + 0x70 * (handle & 0x1FF));
                    const int off_pin = SCHEMA("C_BaseCSGrenade", "m_bPinPulled"_hash);
                    const int off_str = SCHEMA("C_BaseCSGrenade", "m_flThrowStrength"_hash);
                    if (weapon && off_pin)
                        pin_pulled = p->read<bool>(weapon + off_pin);
                    if (weapon && off_str && pin_pulled)
                    {
                        throw_strength = std::clamp(p->read<float>(weapon + off_str), 0.f, 1.f);
                        if (std::fabsf(throw_strength - 0.5f) <= 0.1f)
                            throw_strength = 0.5f;
                    }
                }
            }
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (was_holding && !pin_pulled)
        last_throw = now;
    was_holding = pin_pulled;

    UpdateWeapon(local_weapon);
    if (MapVisibility::Get().Ready())
        UpdateInFlight(world);
}

void GrenadePredict::RenderImpl()
{
    if (!cfg::world::grenade_pred)
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto world = World::Copy();

    std::erase_if(in_flight, [&](const InFlight& g) {
        if (!g.detonated)
            return false;
        const float fade = std::chrono::duration<float>(now - g.detonate_time).count();
        if (fade <= 0.5f)
            return false;
        for (const auto& proj : world.projectiles)
        {
            if (proj.entity == g.entity)
                return false;
        }
        return true;
    });

    for (auto& gren : in_flight)
    {
        if (!gren.traj.valid)
            continue;
        if (!gren.detonated)
        {
            const float elapsed = std::chrono::duration<float>(now - gren.throw_time).count();
            if (elapsed >= gren.traj.duration || gren.effect_started)
            {
                gren.detonated = true;
                gren.detonate_time = now;
            }
        }
        float alpha = 1.f;
        if (gren.detonated)
        {
            const float elapsed = std::chrono::duration<float>(now - gren.detonate_time).count();
            alpha = std::clamp(1.f - elapsed / 0.5f, 0.f, 1.f);
        }
        if (alpha > 0.f)
            DrawTrajectory(gren.traj, alpha);
    }

    if (!MapVisibility::Get().Ready() || !CanPredict(world))
        return;

    Vec3_t origin{}, velocity{};
    SetupThrow(origin, velocity);
    Trajectory traj{};
    Simulate(origin, velocity, traj);
    if (traj.valid)
        DrawTrajectory(traj, 1.f);
}

bool GrenadePredict::CanPredict(const WorldSnapshot&) const
{
    if (!IsGrenade(local_weapon))
        return false;
    if (!pin_pulled)
    {
        const float since = std::chrono::duration<float>(std::chrono::steady_clock::now() - last_throw).count();
        if (since < 1.f)
            return false;
    }
    return true;
}

void GrenadePredict::UpdateWeapon(short weapon_id)
{
    switch (weapon_id)
    {
    case weapon_molotov:
    case weapon_incgrenade:
        weapon_hash = "weapon_molotov"_hash;
        throw_velocity = 750.f;
        detonate_time = 2.f;
        velocity_threshold = 0.f;
        break;
    case weapon_decoy:
        weapon_hash = "weapon_decoy"_hash;
        throw_velocity = 750.f;
        detonate_time = 2.f;
        velocity_threshold = 0.2f;
        break;
    case weapon_smokegrenade:
        weapon_hash = "weapon_smokegrenade"_hash;
        throw_velocity = 750.f;
        detonate_time = 1.5f;
        velocity_threshold = 0.1f;
        break;
    case weapon_flashbang:
        weapon_hash = "weapon_flashbang"_hash;
        throw_velocity = 750.f;
        detonate_time = 1.5f;
        velocity_threshold = 0.1f;
        break;
    default:
        weapon_hash = "weapon_hegrenade"_hash;
        throw_velocity = 750.f;
        detonate_time = 1.5f;
        velocity_threshold = 0.1f;
        break;
    }
}

void GrenadePredict::SetupThrow(Vec3_t& origin, Vec3_t& velocity) const
{
    auto ang = angles;
    if (ang.x > 90.f)
        ang.x -= 360.f;
    else if (ang.x < -90.f)
        ang.x += 360.f;
    ang.x -= (90.f - std::fabsf(ang.x)) * 10.f / 90.f;

    float strength = pin_pulled ? throw_strength : 1.f;
    Vec3_t eye_pos = eye;
    eye_pos.z += strength * 12.f - 12.f;
    const Vec3_t forward = AnglesToForward(ang);

    MapVisibility::TraceHit trace{};
    MapVisibility::Get().TraceSegment(eye_pos, eye_pos + forward * kForward, kHull, trace);
    origin = (trace.hit ? trace.end : (eye_pos + forward * kForward)) - forward * kPullBack;

    const float throw_vel = std::clamp(throw_velocity * 0.9f, 15.f, 750.f);
    const float throw_speed = (strength * 0.7f + 0.3f) * throw_vel;
    velocity = forward * throw_speed + player_vel * kInherit;
}

void GrenadePredict::UpdateInFlight(const WorldSnapshot& world)
{
    const auto now = std::chrono::steady_clock::now();
    std::unordered_set<uintptr_t> alive;
    for (const auto& proj : world.projectiles)
    {
        if (proj.type == WorldProjectile::Type::inferno)
            continue;
        if (cfg::world::grenade_local_only && world.local_pawn_handle
            && proj.thrower_handle != static_cast<uint32_t>(world.local_pawn_handle))
            continue;

        alive.insert(proj.entity);
        InFlight* existing = nullptr;
        for (auto& gren : in_flight)
        {
            if (gren.entity == proj.entity)
            {
                existing = &gren;
                break;
            }
        }

        if (existing)
        {
            existing->last_seen = now;
            if (!existing->corrected && proj.initial_vel.length_sqr() >= 1.f)
            {
                const auto saved = weapon_hash;
                weapon_hash = existing->weapon_hash;
                Simulate(proj.initial_pos, proj.initial_vel, existing->traj);
                weapon_hash = saved;
                existing->corrected = true;
            }
            if (!existing->detonated && proj.effect_tick_begin > 0)
            {
                existing->effect_started = true;
                existing->detonated = true;
                existing->detonate_time = now;
            }
            continue;
        }

        if (proj.initial_vel.length_sqr() < 1.f || proj.effect_tick_begin > 0)
            continue;

        InFlight gren{};
        gren.entity = proj.entity;
        gren.weapon_hash = HashFromType(proj.type);
        gren.throw_time = now;
        gren.last_seen = now;
        gren.corrected = true;
        const auto saved = weapon_hash;
        weapon_hash = gren.weapon_hash;
        Simulate(proj.initial_pos, proj.initial_vel, gren.traj);
        weapon_hash = saved;
        in_flight.push_back(std::move(gren));
    }

    for (auto& gren : in_flight)
    {
        if (gren.detonated || alive.contains(gren.entity))
            continue;
        const float missing = std::chrono::duration<float>(now - gren.last_seen).count();
        if (missing >= 0.1f)
        {
            gren.detonated = true;
            gren.detonate_time = now;
        }
    }
}

uint32_t GrenadePredict::HashFromType(WorldProjectile::Type type) const
{
    switch (type)
    {
    case WorldProjectile::Type::he: return "weapon_hegrenade"_hash;
    case WorldProjectile::Type::flash: return "weapon_flashbang"_hash;
    case WorldProjectile::Type::smoke: return "weapon_smokegrenade"_hash;
    case WorldProjectile::Type::molotov: return "weapon_molotov"_hash;
    case WorldProjectile::Type::decoy: return "weapon_decoy"_hash;
    default: return 0;
    }
}

void GrenadePredict::Simulate(const Vec3_t& start, const Vec3_t& velocity, Trajectory& out) const
{
    out.points.clear();
    out.bounces.clear();
    out.valid = false;
    out.end_tick = -1;
    out.points.reserve(kMaxTicks / kTicksPerPoint);

    Vec3_t pos = start;
    Vec3_t vel = velocity;
    int bounce_count = 0;
    int tick_timer = 0;

    for (int tick = 0; tick < kMaxTicks; ++tick)
    {
        if (tick_timer == 0)
            out.points.push_back(pos);

        bool impact = false;
        bool hit = false;
        Vec3_t nrm{};
        Step(pos, vel, impact, nrm, hit);
        if (hit)
        {
            ++bounce_count;
            out.bounces.push_back(pos);
        }

        const bool stopped = std::fabsf(vel.x) < 20.f && std::fabsf(vel.y) < 20.f && vel.length_sqr() < kStopSq;
        if (impact || ShouldDetonate(vel, tick) || bounce_count > 20 || stopped)
        {
            out.end_tick = tick;
            out.end = pos;
            out.duration = static_cast<float>(tick) * kTick;
            break;
        }
        if (hit || ++tick_timer >= kTicksPerPoint)
            tick_timer = 0;
    }

    if (!out.points.empty() && out.end_tick >= 0 && out.points.back().dist_to_3d(out.end) > 1.f)
        out.points.push_back(out.end);
    out.valid = out.end_tick >= 0;
}

void GrenadePredict::Step(Vec3_t& pos, Vec3_t& vel, bool& detonated, Vec3_t& hit_n, bool& hit) const
{
    detonated = false;
    hit = false;
    const float new_z = vel.z - kGravity * kTick;
    const Vec3_t move{
        vel.x * kTick,
        vel.y * kTick,
        (vel.z + new_z) * 0.5f * kTick
    };
    vel.z = new_z;

    MapVisibility::TraceHit trace{};
    if (MapVisibility::Get().TraceSegment(pos, pos + move, kHull, trace) && trace.hit)
    {
        pos = trace.end;
        hit_n = trace.normal;
        hit = true;
        Resolve(trace.normal, pos, vel, detonated);
        return;
    }
    pos = pos + move;
}

void GrenadePredict::Resolve(const Vec3_t& nrm, Vec3_t& pos, Vec3_t& vel, bool& detonated) const
{
    detonated = false;
    const bool molotov = weapon_hash == "weapon_molotov"_hash;
    if (molotov && (nrm.z >= kMolotovSlopeZ || vel.length_sqr() < kStopSq))
    {
        detonated = true;
        vel = {};
        return;
    }

    Vec3_t next = ClipVelocity(vel, nrm, 2.f);
    next *= kElasticity;
    if (nrm.z > 0.7f)
    {
        if (next.length_sqr() > 96000.f)
        {
            const float l = next.normalized().dot(nrm);
            if (l > 0.5f)
                next *= (1.5f - l);
        }
        if (next.length_sqr() < kStopSq)
        {
            vel = {};
            return;
        }
    }
    vel = next;
    (void)pos;
}

bool GrenadePredict::ShouldDetonate(const Vec3_t& vel, int tick) const
{
    switch (weapon_hash)
    {
    case "weapon_smokegrenade"_hash:
    case "weapon_decoy"_hash:
    {
        const float speed2d = std::sqrtf(vel.x * vel.x + vel.y * vel.y);
        const int check = static_cast<int>(0.2f / kTick);
        return speed2d < velocity_threshold && (tick % check) == 0;
    }
    case "weapon_molotov"_hash:
        return static_cast<float>(tick) * kTick > detonate_time;
    case "weapon_flashbang"_hash:
    case "weapon_hegrenade"_hash:
        return static_cast<float>(tick - 8) * kTick > detonate_time;
    default:
        return false;
    }
}

Vec3_t GrenadePredict::ClipVelocity(const Vec3_t& velocity, const Vec3_t& normal, float overbounce)
{
    const float backoff = velocity.dot(normal) * overbounce;
    Vec3_t out{
        velocity.x - normal.x * backoff,
        velocity.y - normal.y * backoff,
        velocity.z - normal.z * backoff
    };
    if (std::fabsf(out.x) < 0.1f) out.x = 0.f;
    if (std::fabsf(out.y) < 0.1f) out.y = 0.f;
    if (std::fabsf(out.z) < 0.1f) out.z = 0.f;
    return out;
}

void GrenadePredict::DrawTrajectory(const Trajectory& traj, float alpha) const
{
    if (!traj.valid || traj.points.size() < 2)
        return;

    const auto& frame = FrameStore::Get().Latest();
    const Vec2_t screen = ImGui::GetIO().DisplaySize;
    auto* dl = ImGui::GetBackgroundDrawList();
    const auto& col = cfg::world::grenade_color;
    const int cr = static_cast<int>(std::clamp(col.r, 0.f, 1.f) * 255.f);
    const int cg = static_cast<int>(std::clamp(col.g, 0.f, 1.f) * 255.f);
    const int cb = static_cast<int>(std::clamp(col.b, 0.f, 1.f) * 255.f);
    const int ca = static_cast<int>(std::clamp(col.a * alpha, 0.f, 1.f) * 255.f);

    for (size_t i = 0; i + 1 < traj.points.size(); ++i)
    {
        Vec2_t a{}, b{};
        if (!frame.matrix.wts(traj.points[i], screen, a, false)
            || !frame.matrix.wts(traj.points[i + 1], screen, b, false))
            continue;
        const float t = static_cast<float>(i) / static_cast<float>(traj.points.size() - 1);
        const int seg_a = static_cast<int>(ca * (1.f - t * 0.6f));
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), IM_COL32(cr, cg, cb, seg_a), 2.f);
    }

    for (const auto& bounce : traj.bounces)
    {
        Vec2_t s{};
        if (!frame.matrix.wts(bounce, screen, s, false))
            continue;
        dl->AddRectFilled(ImVec2(s.x - 3.f, s.y - 3.f), ImVec2(s.x + 3.f, s.y + 3.f), IM_COL32(0, 0, 0, ca));
        dl->AddRectFilled(ImVec2(s.x - 2.f, s.y - 2.f), ImVec2(s.x + 2.f, s.y + 2.f), IM_COL32(cr, cg, cb, ca));
    }

    if (traj.end_tick >= 0)
    {
        Vec2_t s{};
        if (frame.matrix.wts(traj.end, screen, s, false))
        {
            dl->AddRectFilled(ImVec2(s.x - 5.f, s.y - 5.f), ImVec2(s.x + 5.f, s.y + 5.f), IM_COL32(0, 0, 0, ca));
            dl->AddRectFilled(ImVec2(s.x - 4.f, s.y - 4.f), ImVec2(s.x + 4.f, s.y + 4.f), IM_COL32(cr, cg, cb, ca));
        }
    }
}
