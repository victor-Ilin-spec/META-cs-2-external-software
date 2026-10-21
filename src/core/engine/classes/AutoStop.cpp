#include "AutoStop.hpp"

#include "Aimbot.hpp"
#include "AutoScope.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/types/Weapons.hpp"
#include "gui/renderer/Renderer.hpp"


#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

namespace {
    constexpr WORD kScanShift = 0x2A;

    bool g_shift = false;
    bool g_holding = false;
    std::string g_last_skip;

    FILE* LogFile()
    {
        static FILE* f = nullptr;
        static bool tried = false;
        if (tried)
            return f;
        tried = true;

        char path[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
            return nullptr;
        char* slash = strrchr(path, '\\');
        if (!slash)
            return nullptr;
        std::snprintf(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - path), "autostop.log");
        if (fopen_s(&f, path, "a") != 0)
            f = nullptr;
        if (f)
        {
            std::setvbuf(f, nullptr, _IONBF, 0);
            std::fprintf(f, "\n==== autostop log start ====\n");
        }
        return f;
    }

    void Log(const char* fmt, ...)
    {
        FILE* f = LogFile();
        if (!f)
            return;

        SYSTEMTIME st{};
        GetLocalTime(&st);
        std::fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        va_list args;
        va_start(args, fmt);
        std::vfprintf(f, fmt, args);
        va_end(args);
        std::fputc('\n', f);
    }

    void SendScan(WORD scan, bool down)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = scan;
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    }

    void SetShift(bool want, const char* why)
    {
        if (g_shift == want)
            return;
        if (want && (GetAsyncKeyState(VK_SHIFT) & 0x8000))
            return;
        SendScan(kScanShift, want);
        g_shift = want;
        Log("shift %s why=%s", want ? "DOWN" : "UP", why);
    }

    void Skip(const char* why)
    {
        g_holding = false;
        if (g_last_skip == why)
            return;
        g_last_skip = why;
        Log("skip %s", why);
        SetShift(false, why);
    }

    bool EnemyInFov(const Snapshot& snapshot, float screen_w, float screen_h, float* out_dist)
    {
        const Vec2_t screen(screen_w, screen_h);
        const Vec2_t center(screen_w * 0.5f, screen_h * 0.5f);
        const float used_fov = cfg::aimbot::ActiveFov(snapshot.local.weapon.item_index);
        const int zoom = snapshot.local.weapon.zoom_level > 0 ? snapshot.local.weapon.zoom_level
            : (snapshot.local.scoped ? 1 : 0);
        const float fov_px = Aimbot::FovRadiusPx(screen_w, used_fov, zoom);
        Vec3_t right{}, up{};
        auto matrix = snapshot.game.view_matrix;
        Aimbot::GetViewAxes(matrix, right, up);

        float best = fov_px;
        bool hit = false;
        for (const auto& player : snapshot.players)
        {
            if (!player.alive || player.localplayer || player.SameEntity(snapshot.local) || player.health <= 0)
                continue;
            if (!player.can_hit)
                continue;
            if (!snapshot.globals.is_deathmatch && player.team == snapshot.local.team)
                continue;

            for (const auto& point : Aimbot::BuildAimPoints(player, snapshot.local.weapon.item_index, right, up))
            {
                Vec2_t out{};
                if (!matrix.wts(point, screen, out, true))
                    continue;
                const float dx = out.x - center.x;
                const float dy = out.y - center.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= best)
                    continue;
                best = dist;
                hit = true;
            }
        }
        if (out_dist)
            *out_dist = hit ? best : -1.f;
        return hit;
    }
}

void AutoStop::NotifyShot()
{
    AutoScope::NotifyShot();
}

void AutoStop::NotifyKill()
{
    Log("event KILL");
    SetShift(false, "kill");
    g_last_skip.clear();
    AutoScope::NotifyKill();
}

void AutoStop::Tick()
{
    if (!cfg::aimbot::enabled || !cfg::autofire::enabled || !cfg::autofire::early_stop)
        return Skip("disabled");

    if (!Engine::IsGameFocused() || Renderer::IsOpen() || Engine::IsGameUiOpen())
        return Skip("unfocused");

    auto process = Engine::GetProcess();
    if (!process || !process->handle_ || !process->hwnd_)
        return Skip("no_process");

    RECT wnd{};
    if (!GetClientRect(process->hwnd_, &wnd))
        return Skip("no_rect");

    const float screen_w = static_cast<float>(wnd.right - wnd.left);
    const float screen_h = static_cast<float>(wnd.bottom - wnd.top);
    if (screen_w <= 0.f || screen_h <= 0.f)
        return Skip("bad_screen");

    const auto& snapshot = Cache::Current();
    if (!snapshot.local.alive || IsKnife(snapshot.local.weapon.item_index))
        return Skip(IsKnife(snapshot.local.weapon.item_index) ? "knife" : "dead");

    float aim_dist = -1.f;
    if (!EnemyInFov(snapshot, screen_w, screen_h, &aim_dist))
        return Skip("no_fov");

    g_holding = true;
    if (g_last_skip != "hold")
    {
        g_last_skip = "hold";
        Log("hold aim=%.1f", aim_dist);
    }
    SetShift(true, "fov");
}

bool AutoStop::Holding()
{
    return g_holding;
}
