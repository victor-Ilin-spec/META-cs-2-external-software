#include "RCS.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/offsets/Offsets.hpp"
#include "RecoilPatterns.hpp"
#include "config/Current.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <chrono>

namespace {
    constexpr float kRecoilScale = 2.0f;
    constexpr float kYaw = 0.022f;

    FILE* LogFile()
    {
        static FILE* f = nullptr;
        static bool tried = false;
        if (tried)
            return f;
        tried = true;

        char exe[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, exe, MAX_PATH))
            return nullptr;

        char dir[MAX_PATH]{};
        strncpy_s(dir, exe, _TRUNCATE);
        for (int up = 0; up < 5; ++up)
        {
            char* slash = std::strrchr(dir, '\\');
            if (!slash)
                break;
            *slash = '\0';
            char sln[MAX_PATH]{};
            std::snprintf(sln, MAX_PATH, "%s\\cs2-external-esp.sln", dir);
            if (GetFileAttributesA(sln) != INVALID_FILE_ATTRIBUTES)
            {
                char path[MAX_PATH]{};
                std::snprintf(path, MAX_PATH, "%s\\rcs.log", dir);
                if (fopen_s(&f, path, "a") == 0 && f)
                {
                    std::setvbuf(f, nullptr, _IONBF, 0);
                    std::fprintf(f, "\n==== rcs log start exe=%s ====\n", exe);
                }
                return f;
            }
        }
        return nullptr;
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
}

RCS& RCS::Get()
{
    static RCS instance;
    return instance;
}

void RCS::Tick()
{
    Get().Update();
}

bool RCS::OwnsY()
{
    const auto& r = Get();
    return cfg::rcs::enabled && r.spraying;
}

uintptr_t RCS::ResolvePawn() const
{
    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!process || !process->handle_ || !client.base)
        return 0;

    const auto entity_list = process->read<uintptr_t>(client.base + offsets::entityList);
    const auto controller = process->read<uintptr_t>(client.base + offsets::localPlayerController);
    if (!entity_list || !controller)
        return 0;

    const auto pawn_handle = process->read<uint32_t>(controller + offsets::controller::m_hPawn);
    if (!pawn_handle)
        return 0;

    const auto list_entry = process->read<uintptr_t>(
        entity_list + 0x10 + 0x8 * ((pawn_handle & 0x7FFF) >> 9));
    if (!list_entry)
        return 0;

    return process->read<uintptr_t>(list_entry + 0x70 * (pawn_handle & 0x1FF));
}

Vec3_t RCS::ReadPunch(uintptr_t pawn) const
{
    auto process = Engine::GetProcess();
    if (!process)
        return {};

    const auto services = process->read<uintptr_t>(pawn + offsets::pawn::m_pAimPunchServices);
    if (!services)
        return {};

    const int count = process->read<int>(services + offsets::pawn::m_aimPunchCache);
    const auto data = process->read<uintptr_t>(services + offsets::pawn::m_aimPunchCache + 0x8);
    if (count > 0 && count < 0xFFFF && data)
        return process->read<Vec3_t>(data + static_cast<uintptr_t>(count - 1) * sizeof(Vec3_t));

    return process->read<Vec3_t>(services + offsets::pawn::m_aimPunchAngle);
}

float RCS::ReadSensitivity() const
{
    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!process || !client.base || !offsets::sensitivity)
        return 1.0f;

    const auto obj = process->read<uintptr_t>(client.base + offsets::sensitivity);
    if (!obj)
        return 1.0f;

    const float sens = process->read<float>(obj + offsets::sensitivity_value);
    if (!std::isfinite(sens) || sens < 0.05f || sens > 8.0f)
        return 1.0f;

    return sens;
}

void RCS::MoveMouse(float x, float y)
{
    acc_x += x;
    acc_y += y;

    const LONG dx = static_cast<LONG>(acc_x);
    const LONG dy = static_cast<LONG>(acc_y);
    acc_x -= static_cast<float>(dx);
    acc_y -= static_cast<float>(dy);

    if (dx == 0 && dy == 0)
        return;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(INPUT));
}

void RCS::Reset()
{
    old_punch = {};
    last_shots = 0;
    acc_x = 0.f;
    acc_y = 0.f;
    remain_x = 0.f;
    remain_y = 0.f;
    have_clock = false;
    spraying = false;
}

float RCS::Ramp(int shots) const
{
    return shots >= 1 ? 1.f : 0.f;
}

void RCS::Update()
{
    static bool last_on = false;
    static bool last_lmb = false;
    static short last_weap = -1;
    static DWORD last_tick_ms = 0;

    const auto now = std::chrono::steady_clock::now();
    float dt = 0.008f;
    if (have_clock)
        dt = std::chrono::duration<float>(now - last_t).count();
    last_t = now;
    have_clock = true;
    dt = std::clamp(dt, 0.0005f, 0.032f);

    if (!cfg::rcs::enabled)
    {
        if (last_on)
            Log("off shots=%d punch=%.3f,%.3f", last_shots, old_punch.x, old_punch.y);
        last_on = false;
        last_lmb = false;
        Reset();
        return;
    }
    if (!last_on)
        Log("on smooth=%.2f str=%.2f,%.2f react=%d tau-dt",
            cfg::rcs::smooth, cfg::rcs::strength_x, cfg::rcs::strength_y, cfg::rcs::reaction_ms);
    last_on = true;

    auto process = Engine::GetProcess();
    if (!process || !process->handle_)
        return;

    const uintptr_t pawn = ResolvePawn();
    if (!pawn)
    {
        Reset();
        return;
    }

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const int shots = process->read<int>(pawn + offsets::pawn::m_iShotsFired);
    const Vec3_t punch = ReadPunch(pawn);
    const auto& snapshot = Cache::Current();
    const short weap = snapshot.local.weapon.item_index;
    const auto* profile = RecoilPatterns::Find(weap);

    if (!lmb)
    {
        if (last_lmb)
            Log("lmb_up shots=%d punch=%.3f,%.3f remain=%.2f,%.2f",
                shots, punch.x, punch.y, remain_x, remain_y);
        last_lmb = false;
        old_punch = punch;
        last_shots = shots;
        remain_x = 0.f;
        remain_y = 0.f;
        acc_x = 0.f;
        acc_y = 0.f;
        spraying = false;
        return;
    }

    if (!last_lmb)
        Log("lmb_down weap=%d(%s) shots=%d punch=%.3f,%.3f",
            weap, profile ? profile->name : "none", shots, punch.x, punch.y);
    last_lmb = true;

    if (weap != last_weap)
    {
        Log("weap %d -> %d(%s)", last_weap, weap, profile ? profile->name : "none");
        last_weap = weap;
        remain_x = 0.f;
        remain_y = 0.f;
        spraying = false;
    }

    if (shots < last_shots)
    {
        Log("shots rewind %d -> %d", last_shots, shots);
        remain_x = 0.f;
        remain_y = 0.f;
        spraying = false;
        last_shots = shots;
        old_punch = punch;
        return;
    }

    if (shots > last_shots)
    {
        if (!spraying)
        {
            spraying = true;
            spray_t = now;
            Log("spray_start shots=%d punch=%.3f,%.3f", shots, punch.x, punch.y);
        }
        last_shot_t = now;
    }
    else if (spraying)
    {
        const float quiet_ms = std::chrono::duration<float, std::milli>(now - last_shot_t).count();
        if (quiet_ms > 140.f)
        {
            Log("spray_pause %.0fms remain was %.2f,%.2f", quiet_ms, remain_x, remain_y);
            remain_x = 0.f;
            remain_y = 0.f;
            spraying = false;
            old_punch = punch;
            last_shots = shots;
            return;
        }
    }

    const float dp = punch.x - old_punch.x;
    const float dyaw = punch.y - old_punch.y;
    const float ramp = Ramp(shots);
    const float str_y = std::clamp(cfg::rcs::strength_y, 0.30f, 1.f) * ramp;
    const float str_x = std::clamp(cfg::rcs::strength_x, 0.20f, 1.f) * ramp;
    const float sens = (std::max)(0.05f, ReadSensitivity());

    if (dp < -0.001f)
    {
        const float add_y = ((dp * -1.f) * kRecoilScale / sens) / kYaw * str_y;
        if (std::isfinite(add_y))
            remain_y += add_y;
    }
    if (std::fabs(punch.y) > std::fabs(old_punch.y) + 0.001f)
    {
        const float add_x = ((dyaw * -1.f) * kRecoilScale / sens) / -kYaw * str_x;
        if (std::isfinite(add_x))
            remain_x += add_x;
    }

    if (std::fabs(dp) >= 0.001f || std::fabs(dyaw) >= 0.001f)
        Log("punch_d shots=%d d=%.3f,%.3f punch=%.3f,%.3f ramp=%.2f remain=%.2f,%.2f",
            shots, dp, dyaw, punch.x, punch.y, ramp, remain_x, remain_y);

    old_punch = punch;
    last_shots = shots;

    if (!spraying)
        return;

    const float reacted_ms = std::chrono::duration<float, std::milli>(now - spray_t).count();
    if (reacted_ms < static_cast<float>((std::max)(0, cfg::rcs::reaction_ms)))
    {
        const DWORD tick = GetTickCount();
        if (tick - last_tick_ms >= 40)
        {
            last_tick_ms = tick;
            Log("delay shots=%d left=%.0f remain=%.2f,%.2f",
                shots, static_cast<float>(cfg::rcs::reaction_ms) - reacted_ms, remain_x, remain_y);
        }
        return;
    }

    const float tau = (20.f + std::clamp(cfg::rcs::smooth, 1.f, 12.f) * 18.f) / 1000.f;
    const float k = 1.f - std::exp(-dt / (std::max)(0.012f, tau));
    const float step_x = remain_x * k;
    const float step_y = remain_y * k;
    remain_x -= step_x;
    remain_y -= step_y;

    if (!std::isfinite(step_x) || !std::isfinite(step_y))
        return;

    const float send_gap_ms = std::chrono::duration<float, std::milli>(now - last_send_t).count();
    if (send_gap_ms < 8.f && std::fabs(acc_x + step_x) < 1.f && std::fabs(acc_y + step_y) < 1.f)
    {
        acc_x += step_x;
        acc_y += step_y;
        return;
    }

    const float acc_in_x = acc_x;
    const float acc_in_y = acc_y;
    MoveMouse(step_x, step_y);
    last_send_t = now;

    const DWORD tick = GetTickCount();
    if (tick - last_tick_ms >= 40)
    {
        last_tick_ms = tick;
        Log("tick shots=%d punch=%.3f,%.3f remain=%.2f,%.2f step=%.2f,%.2f sent=%ld,%ld k=%.3f dt=%.1f",
            shots, punch.x, punch.y, remain_x, remain_y, step_x, step_y,
            static_cast<long>(acc_in_x + step_x), static_cast<long>(acc_in_y + step_y),
            k, dt * 1000.f);
    }
}
