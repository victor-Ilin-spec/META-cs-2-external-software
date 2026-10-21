#include "AutoFire.hpp"

#include "Aimbot.hpp"
#include "Bones.hpp"
#include "Humanize.hpp"
#include "RCS.hpp"
#include "AutoStop.hpp"
#include "core/engine/Engine.hpp"
#include "core/engine/TickTime.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/types/Weapons.hpp"
#include "core/offsets/Offsets.hpp"


#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
    uintptr_t ResolveHandle(const std::shared_ptr<pProcess>& process, uintptr_t entity_list, uint32_t handle)
    {
        if (!process || !entity_list || !handle)
            return 0;

        const auto bucket = process->read<uintptr_t>(
            entity_list + 0x10 + 0x8 * ((handle & 0x7FFF) >> 9));
        if (!bucket)
            return 0;

        return process->read<uintptr_t>(bucket + 0x70 * (handle & 0x1FF));
    }

    void MoveMouse(LONG dx, LONG dy)
    {
        if (dx == 0 && dy == 0)
            return;

        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        SendInput(1, &input, sizeof(INPUT));
    }

    void Click()
    {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }


    struct AutoFireSettings {
        int hit_chance = 70;
        int reaction_time = 100;
        int before_shot = 0;
        int between_shots = 45;
        bool block_noscope = true;
    };

    AutoFireSettings ResolveAutoFire(short id)
    {
        AutoFireSettings s{
            cfg::autofire::hit_chance,
            cfg::autofire::reaction_time,
            cfg::autofire::before_shot_delay,
            cfg::autofire::between_shots,
            cfg::autofire::block_noscope
        };
        if (const auto* tune = cfg::aimbot::ActiveTune(id))
        {
            s.hit_chance = tune->af_hit_chance;
            s.reaction_time = tune->af_reaction_time;
            s.before_shot = tune->af_before_shot;
            s.between_shots = tune->af_between_shots;
            s.block_noscope = tune->af_block_noscope;
        }
        return s;
    }


    Vec3_t Eye(const Player& p)
    {
        return p.EyePos();
    }

    float TargetRadius(const Player& p)
    {
        if (p.bone_list.size() > bone_index::head && p.bone_list.size() > bone_index::pelvis)
        {
            const Vec3_t& h = p.bone_list[bone_index::head].pos;
            const Vec3_t& pel = p.bone_list[bone_index::pelvis].pos;
            if (!h.zero() && !pel.zero())
                return (std::max)(7.f, (h - pel).length() * 0.28f);
        }
        return 10.f;
    }

    constexpr float kDegToRad = 3.14159265358979323846f / 180.f;

    void AngleVectors(const Vec3_t& angles, Vec3_t& forward, Vec3_t& right, Vec3_t& up)
    {
        const float pitch = angles.x * kDegToRad;
        const float yaw = angles.y * kDegToRad;
        const float sp = std::sin(pitch);
        const float cp = std::cos(pitch);
        const float sy = std::sin(yaw);
        const float cy = std::cos(yaw);
        forward = { cp * cy, cp * sy, -sp };
        right = { -sy, cy, 0.f };
        up = { sp * cy, sp * sy, cp };
    }

    uint32_t MixSeed(uint32_t tick, int recoil, int sample)
    {
        uint32_t x = tick ^ (static_cast<uint32_t>(recoil) * 1664525u) ^ (static_cast<uint32_t>(sample) * 1013904223u);
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x;
    }

    float UnitFromSeed(uint32_t& seed)
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0x00FFFFFFu) * (1.f / 16777216.f);
    }

    void SpreadSample(uint32_t seed, float inaccuracy, float turning, float recoil, float& sx, float& sy)
    {
        const float r1 = UnitFromSeed(seed);
        const float r2 = UnitFromSeed(seed);
        const float mag = (std::max)(0.f, inaccuracy) + (std::max)(0.f, turning);
        const float radius = mag * std::sqrt((std::max)(r1, 0.f)) * (1.f + std::clamp(recoil, 0.f, 16.f) * 0.015f);
        const float theta = r2 * 6.28318530718f;
        sx = std::cos(theta) * radius;
        sy = std::sin(theta) * radius;
    }

    bool RayHitsPoint(const Vec3_t& eye, const Vec3_t& dir, const Vec3_t& point, float radius)
    {
        if (point.zero() || radius <= 0.f)
            return false;
        const float t = (point - eye).dot(dir);
        if (t < 0.f)
            return false;
        return (eye + dir * t - point).length() <= radius;
    }

    bool RayHitsTarget(const Vec3_t& eye, const Vec3_t& dir, const Player& target, const Vec3_t& aim_point, float radius)
    {
        if (RayHitsPoint(eye, dir, aim_point, radius))
            return true;

        const int n = static_cast<int>(target.bone_list.size());
        const int check[] = {
            bone_index::head, bone_index::neck, bone_index::spine_2,
            bone_index::spine_1, bone_index::spine_0, bone_index::pelvis
        };
        for (int id : check)
        {
            if (id < 0 || id >= n)
                continue;
            const Vec3_t& p = target.bone_list[id].pos;
            const float r = (id == bone_index::head || id == bone_index::neck) ? 6.f : radius;
            if (RayHitsPoint(eye, dir, p, r))
                return true;
        }
        return false;
    }

    bool HitChanceOk(const Player& local, const Player& target, const Vec3_t& aim_point,
        float inaccuracy, float turning, int recoil, uint32_t tick, int hc)
    {
        if (hc <= 0)
            return true;

        const Vec3_t eye = Eye(local);
        const Vec3_t aim = aim_point.zero() ? Eye(target) : aim_point;
        Vec3_t to_aim = aim - eye;
        const float dist = to_aim.length();
        if (dist < 8.f)
            return true;

        Vec3_t forward = to_aim * (1.f / dist);
        Vec3_t up(0.f, 0.f, 1.f);
        Vec3_t right = forward.cross(up);
        float rl = right.length();
        if (rl < 1.0e-4f)
        {
            up = { 0.f, 1.f, 0.f };
            right = forward.cross(up);
            rl = right.length();
        }
        if (rl < 1.0e-4f)
            return true;
        right = right * (1.f / rl);
        up = right.cross(forward);

        const float body_r = std::clamp(TargetRadius(target), 6.f, 10.f);
        const int samples = 20;
        int hits = 0;
        for (int i = 0; i < samples; ++i)
        {
            float sx = 0.f, sy = 0.f;
            SpreadSample(MixSeed(tick, recoil, i), inaccuracy, turning, static_cast<float>(recoil), sx, sy);
            Vec3_t dir = forward + (right * sx) + (up * sy);
            const float len = dir.length();
            if (len < 1.0e-4f)
                continue;
            dir = dir * (1.f / len);
            if (RayHitsPoint(eye, dir, aim, body_r))
                ++hits;
        }

        const float chance = 100.f * static_cast<float>(hits) / static_cast<float>(samples);
        return chance + 0.01f >= static_cast<float>(hc);
    }

}

void AutoFire::Tick()
{
    static float acc_x = 0.f;
    static float acc_y = 0.f;
    static auto last_t = std::chrono::steady_clock::now();
    static tick::Quantizer q{};
    static bool was_on_target = false;
    static uintptr_t last_target = 0;
    static auto on_target_since = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point last_shot{};
    static auto ready_since = std::chrono::steady_clock::now();
    static bool waiting_before_shot = false;
    static int hc_ok = 0;

    auto reset_aim = [&] {
        acc_x = 0.f;
        acc_y = 0.f;
        q.acc = 0.f;
        Humanize::Reset();
    };

    auto reset_target = [&] {
        was_on_target = false;
        last_target = 0;
        waiting_before_shot = false;
        hc_ok = 0;
        reset_aim();
    };

    if (!cfg::aimbot::enabled || !cfg::autofire::enabled)
    {
        reset_target();
        return;
    }

    if (!Engine::IsGameFocused())
    {
        reset_target();
        return;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) || Aimbot::Held())
        return;

    auto process = Engine::GetProcess();
    if (!process || !process->handle_ || !process->hwnd_)
    {
        reset_target();
        return;
    }

    RECT wnd{};
    if (!GetClientRect(process->hwnd_, &wnd))
    {
        reset_target();
        return;
    }

    const float screen_w = static_cast<float>(wnd.right - wnd.left);
    const float screen_h = static_cast<float>(wnd.bottom - wnd.top);
    if (screen_w <= 0.f || screen_h <= 0.f)
    {
        reset_target();
        return;
    }

    const auto& snapshot = Cache::Current();
    const auto local_pawn = snapshot.local.PawnAddress();
    if (!snapshot.local.alive || !local_pawn || IsKnife(snapshot.local.weapon.item_index))
    {
        reset_target();
        return;
    }

    const Vec2_t screen(screen_w, screen_h);
    const Vec2_t center(screen_w * 0.5f, screen_h * 0.5f);
    const float used_fov = cfg::aimbot::ActiveFov(snapshot.local.weapon.item_index);
    const int zoom = snapshot.local.weapon.zoom_level > 0 ? snapshot.local.weapon.zoom_level
        : (snapshot.local.scoped ? 1 : 0);
    const float fov_px = Aimbot::FovRadiusPx(screen_w, used_fov, zoom);
    Vec3_t view_right{}, view_up{};
    Aimbot::GetViewAxes(snapshot.game.view_matrix, view_right, view_up);

    struct Cand {
        float dist;
        const Player* player;
        Vec2_t screen_pt;
        Vec3_t world;
    };
    std::vector<Cand> cands;
    cands.reserve(24);

    for (const auto& player : snapshot.players)
    {
        if (!player.alive || player.localplayer || player.SameEntity(snapshot.local) || player.health <= 0)
            continue;
        if (!player.can_hit)
            continue;

        if (!snapshot.globals.is_deathmatch && player.team == snapshot.local.team)
            continue;

        for (const auto& point : Aimbot::BuildAimPoints(player, snapshot.local.weapon.item_index, view_right, view_up))
        {
            if (point.zero())
                continue;
            Vec2_t out{};
            if (!snapshot.game.view_matrix.wts(point, screen, out, false))
                continue;
            const float dx = out.x - center.x;
            const float dy = out.y - center.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= fov_px)
                continue;
            cands.push_back({ dist, &player, out, point });
        }
    }

    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.dist < b.dist;
    });

    const Player* best = nullptr;
    float best_dist = fov_px;
    Vec2_t best_screen{};
    Vec3_t best_point{};
    if (!cands.empty())
    {
        best = cands[0].player;
        best_dist = cands[0].dist;
        best_screen = cands[0].screen_pt;
        best_point = cands[0].world;
    }

    const uintptr_t current_target = best ? best->PawnAddress() : 0;
    const int current_hp = best ? best->health : 0;
    const auto now = std::chrono::steady_clock::now();
    if (Aimbot::InKillDelay(snapshot, current_target, current_hp))
    {
        reset_target();
        return;
    }

    if (!best)
    {
        reset_target();
        return;
    }

    if (!was_on_target || current_target != last_target)
    {
        was_on_target = true;
        last_target = current_target;
        on_target_since = now;
        waiting_before_shot = false;
        reset_aim();
    }

    const auto af = ResolveAutoFire(snapshot.local.weapon.item_index);
    const bool rcs_owns_y = RCS::OwnsY() && std::fabs(best_screen.y - center.y) < 8.f;
    {
        float dt = std::chrono::duration<float>(now - last_t).count();
        last_t = now;
        const int n = q.Consume(dt);
        if (n > 0)
        {
            float delta_x = best_screen.x - center.x;
            float delta_y = rcs_owns_y ? 0.f : (best_screen.y - center.y);
            const float tick_dt = static_cast<float>(n) * tick::kSec;
            Vec2_t step = Aimbot::CurveStep(
                delta_x, delta_y, tick_dt,
                cfg::aimbot::ActiveSmooth(snapshot.local.weapon.item_index));
            Humanize::Tint(step.x, step.y, tick_dt);
            acc_x += step.x;
            acc_y += step.y;
            const LONG mx = static_cast<LONG>(acc_x);
            const LONG my = static_cast<LONG>(acc_y);
            acc_x -= static_cast<float>(mx);
            acc_y -= static_cast<float>(my);
            MoveMouse(mx, my);
        }
    }

    const float on_px = (std::max)(3.f, (std::min)(6.f, fov_px * 0.05f));
    const bool locked = best_dist <= on_px;


    const bool stopped = !cfg::autofire::early_stop || AutoStop::Holding();
    if (!stopped || !locked)
    {
        waiting_before_shot = false;
        hc_ok = 0;
        return;
    }

    auto client = Engine::GetClient();
    if (!process->handle_ || !client.base)
        return;
    const auto weapon_services = process->read<uintptr_t>(local_pawn + offsets::pawn::m_pWeaponServices);
    if (!weapon_services)
        return;

    const auto entity_list = process->read<uintptr_t>(client.base + offsets::entityList);
    if (!entity_list)
        return;

    const auto weapon = ResolveHandle(
        process, entity_list,
        process->read<uint32_t>(weapon_services + offsets::pawn::m_hActiveWeapon));
    if (!weapon)
        return;

    short item = snapshot.local.weapon.item_index;
    if (item <= 0)
    {
        item = static_cast<short>(process->read<uint16_t>(
            weapon + offsets::pawn::m_AttributeManager +
            offsets::pawn::m_Item + offsets::pawn::m_iItemDefinitionIndex));
    }

    const float inaccuracy = process->read<float>(weapon + offsets::pawn::m_fAccuracyPenalty);
    const float spread = process->read<float>(weapon + offsets::pawn::m_flTurningInaccuracy);
    const int recoil = process->read<int>(weapon + offsets::pawn::m_iRecoilIndex);
    uint32_t tick = 0;
    if (offsets::globalVars)
    {
        const auto gv = process->read<uintptr_t>(client.base + offsets::globalVars);
        if (gv)
            tick = static_cast<uint32_t>(process->read<int>(gv + offsets::global::tickCount));
    }
    const bool is_sniper = (item == weapon_awp || item == weapon_ssg08 ||
                            item == weapon_scar20 || item == weapon_g3sg1);
    const bool is_scope_weapon = is_sniper || item == weapon_aug || item == weapon_sg556;
    const bool scoped_flag = snapshot.local.scoped;
    const bool scoped = scoped_flag || (is_scope_weapon && inaccuracy < 0.02f);
    if ((af.block_noscope && is_sniper && !scoped) ||
        (cfg::autofire::autoscope && is_scope_weapon && !scoped))
        return;

    const bool stop_ready = cfg::autofire::early_stop && AutoStop::Holding();
    if (!stop_ready)
    {
        const int reaction_ms = std::clamp(af.reaction_time, 0, 300);
        if (now - on_target_since < std::chrono::milliseconds(reaction_ms))
            return;

        const int before_ms = std::clamp(cfg::autofire::before_shot_delay, 0, 500);
        if (before_ms > 0)
        {
            if (!waiting_before_shot)
            {
                waiting_before_shot = true;
                ready_since = now;
                return;
            }
            if (now - ready_since < std::chrono::milliseconds(before_ms))
                return;
        }
    }

    const int hc = std::clamp(af.hit_chance, 0, 100);
    const int between_ms = std::clamp(af.between_shots, 0, 1000);
    if (hc > 0)
    {
        if (!HitChanceOk(snapshot.local, *best, best_point, inaccuracy, spread, recoil, tick, hc))
        {
            hc_ok = 0;
            return;
        }
        if (++hc_ok < 2)
            return;
    }
    else if (between_ms > 0 && now - last_shot < std::chrono::milliseconds(between_ms))
    {
        return;
    }

    Click();
    last_shot = now;
    waiting_before_shot = false;
    hc_ok = 0;
    AutoStop::NotifyShot();
}
