#include "Aimbot.hpp"

#include "Humanize.hpp"
#include "RCS.hpp"
#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"
#include "core/engine/TickTime.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/classes/Player.hpp"
#include "core/engine/classes/AutoStop.hpp"
#include "core/engine/types/Weapons.hpp"
#include "core/offsets/Offsets.hpp"
#include "core/engine/classes/Hitbox.hpp"


#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kDefaultFov = 90.f;

static Vec3_t Normalize3(Vec3_t v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f)
        return { 1.f, 0.f, 0.f };
    return { v.x / len, v.y / len, v.z / len };
}

static void MoveMouse(LONG dx, LONG dy)
{
    if (dx == 0 && dy == 0)
        return;
    if (!Engine::IsGameFocused() || Engine::IsGameUiOpen() || Renderer::IsOpen())
        return;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(INPUT));
}

static std::atomic<bool> g_held{ false };
static std::atomic<bool> g_block{ false };
static std::atomic<bool> g_hook_run{ false };
static HHOOK g_hook = nullptr;
static DWORD g_hook_tid = 0;
static std::thread g_hook_thread;

static void Click()
{
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

static LRESULT CALLBACK MouseHook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION)
    {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        const bool injected = info && (info->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED));
        if (!injected)
        {
            if (wp == WM_LBUTTONDOWN)
            {
                g_held.store(true, std::memory_order_relaxed);
                if (cfg::aimbot::enabled && Engine::IsGameFocused() &&
                    !Engine::IsGameUiOpen() && !Renderer::IsOpen())
                {
                    g_block.store(true, std::memory_order_relaxed);
                    return 1;
                }
            }
            else if (wp == WM_LBUTTONUP)
            {
                g_held.store(false, std::memory_order_relaxed);
                if (g_block.exchange(false, std::memory_order_relaxed))
                    return 1;
            }
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

static void EnsureMouseGate();

bool Aimbot::Held()
{
    EnsureMouseGate();
    if (g_hook)
        return g_held.load(std::memory_order_relaxed);
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

static void EnsureMouseGate()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_hook_run.store(true, std::memory_order_relaxed);
        g_hook_thread = std::thread([] {
            g_hook_tid = GetCurrentThreadId();
            g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, GetModuleHandleW(nullptr), 0);
            MSG msg{};
            while (g_hook_run.load(std::memory_order_relaxed) && GetMessageW(&msg, nullptr, 0, 0) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (g_hook)
            {
                UnhookWindowsHookEx(g_hook);
                g_hook = nullptr;
            }
        });
    });
}


static bool BoneAt(const Player& player, int bone, Vec3_t& out)
{
    if (player.bone_list.size() <= static_cast<size_t>(bone))
        return false;
    out = player.bone_list[bone].pos;
    return !out.zero();
}

void Aimbot::GetViewAxes(const view_matrix_t& vm, Vec3_t& right, Vec3_t& up)
{
    right = Normalize3({ vm.matrix[0][0], vm.matrix[0][1], vm.matrix[0][2] });
    up = Normalize3({ vm.matrix[1][0], vm.matrix[1][1], vm.matrix[1][2] });
}


static void AddSparseCapsule(std::vector<Vec3_t>& pts, const Vec3_t& a, const Vec3_t& b,
    float radius, const Vec3_t& view_right, const Vec3_t& view_up, bool ring)
{
    const Vec3_t d = b - a;
    const float len = d.length();
    const Vec3_t mid = (len < 1.f) ? a : (a + d * 0.5f);
    pts.push_back(mid);
    if (!ring || radius <= 0.25f)
        return;

    const float r = radius;
    if (len < 1.f)
    {
        pts.push_back(mid + view_right * r);
        pts.push_back(mid - view_right * r);
        return;
    }

    const Vec3_t axis = d / len;
    Vec3_t side = view_right - axis * view_right.dot(axis);
    if (side.length_sqr() < 1e-4f)
        side = view_up - axis * view_up.dot(axis);
    pts.push_back(mid + Normalize3(side) * r);
    pts.push_back(mid - Normalize3(side) * r);
}

static void AddBoneCapsule(std::vector<Vec3_t>& pts, std::vector<AimSeg>* segs,
    const Player& player, int ba, int bb, float radius,
    const Vec3_t& view_right, const Vec3_t& view_up, bool rings, bool fat)
{
    Vec3_t pa{}, pb{};
    if (!BoneAt(player, ba, pa) || !BoneAt(player, bb, pb))
        return;
    if (segs)
        segs->push_back({ pa, pb, radius });
    if (rings)
        AddSparseCapsule(pts, pa, pb, radius, view_right, view_up, fat);
    else
        pts.push_back(pa + (pb - pa) * 0.5f);
}

struct BodyCap { int a; int b; float r; bool fat; };

static constexpr BodyCap kBody[] = {
    { bone_index::neck,       bone_index::head,        4.2f, true  },
    { bone_index::neck,       bone_index::spine_2,     5.2f, false },
    { bone_index::spine_2,    bone_index::spine_1,     6.0f, true  },
    { bone_index::spine_1,    bone_index::spine_0,     5.8f, false },
    { bone_index::spine_0,    bone_index::pelvis,      5.6f, true  },
    { bone_index::neck,       bone_index::shoulder_L,  3.4f, false },
    { bone_index::neck,       bone_index::shoulder_R,  3.4f, false },
    { bone_index::shoulder_L, bone_index::elbow_L,     3.1f, false },
    { bone_index::shoulder_R, bone_index::elbow_R,     3.1f, false },
    { bone_index::elbow_L,    bone_index::hand_L,      2.5f, false },
    { bone_index::elbow_R,    bone_index::hand_R,      2.5f, false },
    { bone_index::pelvis,     bone_index::hip_L,       4.0f, false },
    { bone_index::pelvis,     bone_index::hip_R,       4.0f, false },
    { bone_index::hip_L,      bone_index::knee_L,      3.6f, false },
    { bone_index::hip_R,      bone_index::knee_R,      3.6f, false },
    { bone_index::knee_L,     bone_index::foot_heel_L, 3.0f, false },
    { bone_index::knee_R,     bone_index::foot_heel_R, 3.0f, false },
};

static bool WantGroup(int a, int b, bool all, bool head, bool chest, bool stomach, bool arms, bool legs)
{
    if (all)
        return true;
    const bool is_head = (a == bone_index::neck && b == bone_index::head);
    const bool is_chest = (b == bone_index::spine_2) || (a == bone_index::spine_2 && b == bone_index::spine_1);
    const bool is_stomach = (a == bone_index::spine_1 || a == bone_index::spine_0);
    const bool is_arm =
        b == bone_index::shoulder_L || b == bone_index::shoulder_R ||
        b == bone_index::elbow_L || b == bone_index::elbow_R ||
        b == bone_index::hand_L || b == bone_index::hand_R;
    const bool is_leg =
        b == bone_index::hip_L || b == bone_index::hip_R ||
        b == bone_index::knee_L || b == bone_index::knee_R ||
        b == bone_index::foot_heel_L || b == bone_index::foot_heel_R;
    return (head && is_head) || (chest && is_chest) || (stomach && is_stomach) ||
        (arms && is_arm) || (legs && is_leg);
}

static void CollectCapsules(std::vector<Vec3_t>& pts, std::vector<AimSeg>* segs,
    const Player& player, short weapon_id, const Vec3_t& view_right, const Vec3_t& view_up)
{
    bool nearest = cfg::aimbot::hb_nearest;
    bool head = cfg::aimbot::hb_head;
    bool chest = cfg::aimbot::hb_chest;
    bool stomach = cfg::aimbot::hb_stomach;
    bool arms = cfg::aimbot::hb_arms;
    bool legs = cfg::aimbot::hb_legs;
    if (const auto* tune = cfg::aimbot::ActiveTune(weapon_id))
    {
        nearest = tune->hb_nearest;
        head = tune->hb_head;
        chest = tune->hb_chest;
        stomach = tune->hb_stomach;
        arms = tune->hb_arms;
        legs = tune->hb_legs;
    }

    const bool rings = cfg::aimbot::multipoints;
    const bool all = nearest;
    const float scale = rings ? std::clamp(cfg::aimbot::multipoint_scale, 0.35f, 1.f) : 1.f;
    const int count = hitbox::ClampCount(cfg::aimbot::multipoint_count);

    if (rings)
    {
        std::vector<HitboxRaw> boxes;
        hitbox::FallbackPlayer(boxes);
        pts.reserve(pts.size() + boxes.size() * static_cast<size_t>(count));

        for (const auto& box : boxes)
        {
            if (!all && !hitbox::GroupEnabled(box.group, head, chest, stomach, arms, legs))
                continue;
            const int bone = box.bone;
            if (bone < 0 || player.bone_list.size() <= static_cast<size_t>(bone))
                continue;
            const auto& b = player.bone_list[bone];
            if (b.pos.zero())
                continue;
            if (segs)
                segs->push_back({ b.pos, b.pos, box.radius * scale });
            hitbox::GenerateInto(pts, box, b.pos, b.qx, b.qy, b.qz, b.qw, count, scale);
        }
        if (!pts.empty())
            return;
    }

    for (const auto& c : kBody)
    {
        if (!WantGroup(c.a, c.b, all, head, chest, stomach, arms, legs))
            continue;
        AddBoneCapsule(pts, segs, player, c.a, c.b, c.r * scale, view_right, view_up, rings, c.fat);
    }

    if (pts.empty() && (all || head))
        AddBoneCapsule(pts, segs, player, bone_index::neck, bone_index::head, 4.2f * scale,
            view_right, view_up, rings, true);
}

const std::vector<Vec3_t>& Aimbot::BuildAimPoints(const Player& player, short weapon_id,
    const Vec3_t& view_right, const Vec3_t& view_up)
{
    thread_local std::vector<Vec3_t> pts;
    pts.clear();
    pts.reserve(96);
    CollectCapsules(pts, nullptr, player, weapon_id, view_right, view_up);
    return pts;
}

void Aimbot::BuildAimCapsules(const Player& player, short weapon_id, std::vector<AimSeg>& out)
{
    std::vector<Vec3_t> unused;
    unused.reserve(8);
    Vec3_t z{};
    CollectCapsules(unused, &out, player, weapon_id, z, z);
}


bool Aimbot::InKillDelay(const Snapshot& snapshot, uintptr_t current_target, int current_hp)
{
    static uintptr_t last_target = 0;
    static int last_hp = 0;
    static auto kill_delay_until = std::chrono::steady_clock::now();

    if (!cfg::aimbot::kill_delay)
    {
        if (last_target != 0)
        {
            bool last_alive = false;
            for (const auto& player : snapshot.players)
            {
                if (player.PawnAddress() == last_target && player.alive && player.health > 0)
                {
                    last_alive = true;
                    break;
                }
            }
            if (!last_alive)
                AutoStop::NotifyKill();
        }
        last_target = current_target;
        last_hp = current_hp;
        return false;
    }

    const auto now = std::chrono::steady_clock::now();

    if (now < kill_delay_until)
        return true;

    if (last_target != 0)
    {
        bool last_alive = false;
        for (const auto& player : snapshot.players)
        {
            if (player.PawnAddress() == last_target && player.alive && player.health > 0)
            {
                last_alive = true;
                break;
            }
        }

        if (!last_alive)
        {
            kill_delay_until = now + std::chrono::milliseconds(
                std::clamp(cfg::aimbot::kill_delay_ms, 0, 1000));
            last_target = 0;
            last_hp = 0;
            AutoStop::NotifyKill();
            return true;
        }
    }

    if (current_target != 0 &&
        last_target == current_target &&
        last_hp > 0 && current_hp <= 0)
    {
        kill_delay_until = now + std::chrono::milliseconds(
            std::clamp(cfg::aimbot::kill_delay_ms, 0, 1000));
        last_target = 0;
        last_hp = 0;
        AutoStop::NotifyKill();
        return true;
    }

    if (current_target != 0)
    {
        last_target = current_target;
        last_hp = current_hp;
    }
    return false;
}

int Aimbot::SelectedBone()
{
    switch (cfg::aimbot::bone) {
    case 1: return bone_index::neck;
    case 2: return bone_index::chest;
    default: return bone_index::head;
    }
}

float Aimbot::FovRadiusPx(float screen_w, float fov_deg, int zoom)
{
    const float used = fov_deg > 0.f ? fov_deg : cfg::aimbot::fov;
    const float aim = (std::max)(used, 0.1f) * (kPi / 180.f) * 0.5f;
    const float view = kDefaultFov * (kPi / 180.f) * 0.5f;
    const float px = std::tan(aim) / std::tan(view) * screen_w;
    if (zoom >= 2)
        return px * 6.f;
    if (zoom >= 1)
        return px * 2.f;
    return px;
}

Vec2_t Aimbot::CurveStep(float dx, float dy, float dt, float smooth)
{
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.001f)
        return {};

    const float ticks = tick::FromLegacySlider(smooth);
    const float k = tick::StepK(dt, ticks);
    return { dx * k, dy * k };
}

void Aimbot::Tick()
{
    EnsureMouseGate();

    static float acc_x = 0.f;
    static float acc_y = 0.f;
    static auto last_t = std::chrono::steady_clock::now();
    static tick::Quantizer q{};
    static bool shot = false;

    auto reset = [&] {
        acc_x = 0.f;
        acc_y = 0.f;
        q.acc = 0.f;
    };
    auto reset_all = [&] {
        reset();
        Humanize::Reset();
        shot = false;
    };

    if (!cfg::aimbot::enabled || !Engine::IsGameFocused() || Engine::IsGameUiOpen() || Renderer::IsOpen())
    {
        reset_all();
        return;
    }

    if (!Held())
    {
        reset();
        shot = false;
        if (!cfg::autofire::enabled)
            Humanize::Reset();
        return;
    }

    auto process = Engine::GetProcess();
    if (!process || !process->handle_ || !process->hwnd_)
    {
        reset_all();
        return;
    }

    RECT client{};
    if (!GetClientRect(process->hwnd_, &client))
    {
        reset_all();
        return;
    }

    const float screen_w = static_cast<float>(client.right - client.left);
    const float screen_h = static_cast<float>(client.bottom - client.top);
    if (screen_w <= 0.f || screen_h <= 0.f)
    {
        reset_all();
        return;
    }

    const auto& snapshot = Cache::Current();
    if (!snapshot.local.alive || IsKnife(snapshot.local.weapon.item_index))
    {
        reset_all();
        return;
    }
    const float used_fov = cfg::aimbot::ActiveFov(snapshot.local.weapon.item_index);
    const int zoom = snapshot.local.weapon.zoom_level > 0 ? snapshot.local.weapon.zoom_level
        : (snapshot.local.scoped ? 1 : 0);
    const float fov_px = FovRadiusPx(screen_w, used_fov, zoom);
    const Vec2_t screen(screen_w, screen_h);
    const Vec2_t center(screen_w * 0.5f, screen_h * 0.5f);
    Vec3_t view_right{}, view_up{};
    GetViewAxes(snapshot.game.view_matrix, view_right, view_up);
    const Player* best = nullptr;
    float best_dist = fov_px;
    Vec2_t best_screen{};

    for (const auto& player : snapshot.players)
    {
        if (!player.alive || player.localplayer || player.SameEntity(snapshot.local))
            continue;
        if (!player.can_hit)
            continue;
        if (!snapshot.globals.is_deathmatch && player.team == snapshot.local.team)
            continue;
        for (const auto& point : BuildAimPoints(player, snapshot.local.weapon.item_index, view_right, view_up))
        {
            Vec2_t out{};
            if (!snapshot.game.view_matrix.wts(point, screen, out, false))
                continue;


            const float dx = out.x - center.x;
            const float dy = out.y - center.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= best_dist)
                continue;

            best_dist = dist;
            best = &player;
            best_screen = out;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    const uintptr_t current_target = best ? best->PawnAddress() : 0;
    const int current_hp = best ? best->health : 0;
    if (InKillDelay(snapshot, current_target, current_hp))
    {
        reset();
        return;
    }

    if (!best)
    {
        reset();
        return;
    }

    const short weapon_id = snapshot.local.weapon.item_index;
    const float used_smooth = cfg::aimbot::ActiveSmooth(weapon_id);
    const float on_px = (std::max)(3.f, (std::min)(6.f, fov_px * 0.05f));

    const bool rcs_owns_y = RCS::OwnsY() && std::fabs(best_screen.y - center.y) < 8.f;

    float dt = std::chrono::duration<float>(now - last_t).count();
    last_t = now;
    const int n = q.Consume(dt);
    if (n > 0)
    {
        float delta_x = best_screen.x - center.x;
        float delta_y = rcs_owns_y ? 0.f : (best_screen.y - center.y);
        const float tick_dt = static_cast<float>(n) * tick::kSec;

        Vec2_t step = CurveStep(delta_x, delta_y, tick_dt, used_smooth);
        Humanize::Tint(step.x, step.y, tick_dt);

        acc_x += step.x;
        acc_y += step.y;

        const LONG mx = static_cast<LONG>(acc_x);
        const LONG my = static_cast<LONG>(acc_y);

        acc_x -= static_cast<float>(mx);
        acc_y -= static_cast<float>(my);

        MoveMouse(mx, my);
    }

    if (!shot && best_dist <= on_px)
    {
        Click();
        shot = true;
        g_block.store(false, std::memory_order_relaxed);
        if (g_held.load(std::memory_order_relaxed))
        {
            INPUT hold{};
            hold.type = INPUT_MOUSE;
            hold.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &hold, sizeof(INPUT));
        }
    }
}
