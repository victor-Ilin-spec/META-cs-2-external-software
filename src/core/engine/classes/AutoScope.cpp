#include "AutoScope.hpp"

#include "Aimbot.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/types/Weapons.hpp"
#include "core/offsets/Offsets.hpp"

#include <chrono>
#include <cmath>
#include <windows.h>

namespace {
    using clock = std::chrono::steady_clock;

    bool g_hold_after_kill = false;
    bool g_did_swap = false;
    int g_swap = 0;
    clock::time_point g_swap_at{};
    clock::time_point g_swap_lock{};

    void TapScan(WORD scan)
    {
        INPUT in[2]{};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = scan;
        in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
        in[1].type = INPUT_KEYBOARD;
        in[1].ki.wScan = scan;
        in[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    }

    void BeginKillSwap()
    {
        const auto now = clock::now();
        if (g_swap != 0 || g_did_swap || now < g_swap_lock)
            return;
        g_did_swap = true;
        g_hold_after_kill = true;
        g_swap = 1;
        g_swap_at = now;
        g_swap_lock = now + std::chrono::milliseconds(1000);
    }

    void PumpSwap()
    {
        if (g_swap == 0)
            return;
        const auto now = clock::now();
        if (g_swap == 1 && now - g_swap_at >= std::chrono::milliseconds(160))
        {
            TapScan(0x04);
            g_swap = 2;
            g_swap_at = now;
        }
        else if (g_swap == 2 && now - g_swap_at >= std::chrono::milliseconds(120))
        {
            TapScan(0x02);
            g_swap = 3;
            g_swap_at = now;
        }
        else if (g_swap == 3 && now - g_swap_at >= std::chrono::milliseconds(200))
        {
            g_swap = 0;
        }
    }

    void ScopeClick()
    {
        INPUT in[2]{};
        in[0].type = INPUT_MOUSE;
        in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        in[1].type = INPUT_MOUSE;
        in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, in, sizeof(INPUT));
    }

    bool EnemyInFov(const Snapshot& snapshot, float screen_w, float screen_h,
        uintptr_t* out_pawn, int* out_hp)
    {
        const Vec2_t screen(screen_w, screen_h);
        const Vec2_t center(screen_w * 0.5f, screen_h * 0.5f);
        const int zoom = snapshot.local.weapon.zoom_level > 0 ? snapshot.local.weapon.zoom_level
            : (snapshot.local.scoped ? 1 : 0);
        const float fov_px = Aimbot::FovRadiusPx(
            screen_w, cfg::aimbot::ActiveFov(snapshot.local.weapon.item_index), zoom);
        Vec3_t view_right{}, view_up{};
        Aimbot::GetViewAxes(snapshot.game.view_matrix, view_right, view_up);

        float best = fov_px;
        const Player* hit = nullptr;
        struct Cand { float d; const Player* p; Vec3_t pt; };
        Cand cands[8]{};
        int n = 0;
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
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d >= best)
                    continue;
                best = d;
                if (n < 8)
                    cands[n++] = { d, &player, point };
                else
                    cands[7] = { d, &player, point };
            }
        }
        hit = nullptr;
        best = fov_px;
        for (int i = 0; i < n; ++i)
        {
            if (cands[i].d >= best)
                continue;
            best = cands[i].d;
            hit = cands[i].p;
        }
        if (!hit)
            return false;
        if (out_pawn)
            *out_pawn = hit->PawnAddress();
        if (out_hp)
            *out_hp = hit->health;
        return true;
    }
}

void AutoScope::NotifyKill()
{
    if (Cache::Current().local.weapon.item_index != weapon_awp)
        return;
    BeginKillSwap();
}

void AutoScope::NotifyShot()
{
    if (Cache::Current().local.weapon.item_index != weapon_awp)
        return;
    BeginKillSwap();
}

void AutoScope::Tick()
{
    static bool clicked = false;
    static uintptr_t last_tgt = 0;
    static auto click_time = clock::now();

    PumpSwap();

    if (!cfg::aimbot::enabled || !cfg::autofire::enabled)
    {
        clicked = false;
        g_hold_after_kill = false;
        g_swap = 0;
        last_tgt = 0;
        return;
    }

    if (!Engine::IsGameFocused())
    {
        clicked = false;
        return;
    }

    auto process = Engine::GetProcess();
    if (!process || !process->handle_ || !process->hwnd_)
        return;

    const auto& snapshot = Cache::Current();
    const auto local_pawn = snapshot.local.PawnAddress();
    if (!snapshot.local.alive || !local_pawn)
    {
        clicked = false;
        return;
    }

    RECT wnd{};
    if (!GetClientRect(process->hwnd_, &wnd))
        return;

    const float screen_w = static_cast<float>(wnd.right - wnd.left);
    const float screen_h = static_cast<float>(wnd.bottom - wnd.top);
    if (screen_w <= 0.f || screen_h <= 0.f)
        return;

    uintptr_t tgt = 0;
    int hp = 0;
    const bool in_fov = EnemyInFov(snapshot, screen_w, screen_h, &tgt, &hp);

    if (Aimbot::InKillDelay(snapshot, tgt, hp))
        g_hold_after_kill = true;

    if (g_swap == 0 && !in_fov && clock::now() >= g_swap_lock)
    {
        g_did_swap = false;
        g_hold_after_kill = false;
    }

    if (g_swap != 0 || g_hold_after_kill)
    {
        last_tgt = in_fov ? tgt : 0;
        clicked = false;
        return;
    }

    if (!cfg::aimbot::ActiveAutoscope(snapshot.local.weapon.item_index))
    {
        clicked = false;
        last_tgt = in_fov ? tgt : 0;
        return;
    }

    if (!in_fov)
    {
        last_tgt = 0;
        clicked = false;
        return;
    }
    last_tgt = tgt;

    if (snapshot.local.scoped)
    {
        clicked = false;
        return;
    }

    const auto now = clock::now();
    if (!clicked)
    {
        ScopeClick();
        clicked = true;
        click_time = now;
        return;
    }

    if (now - click_time < std::chrono::milliseconds(40))
        return;

    if (now - click_time > std::chrono::milliseconds(80))
        clicked = false;
}
