#include "Bhop.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"
#include "core/offsets/Offsets.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
    constexpr uint32_t PLUS_JUMP = 65537;
    constexpr uint32_t MINUS_JUMP = 256;
    constexpr uint32_t FL_ONGROUND = 1u << 0;
    constexpr uint32_t FL_DUCKING = 1u << 1;

    uintptr_t ResolvePawn(const std::shared_ptr<pProcess>& process, uintptr_t client_base)
    {
        const auto entity_list = process->read<uintptr_t>(client_base + offsets::entityList);
        const auto controller = process->read<uintptr_t>(client_base + offsets::localPlayerController);
        if (!entity_list || !controller)
            return 0;

        const auto pawn_handle = process->read<uint32_t>(controller + offsets::controller::m_hPawn);
        if (!pawn_handle)
            return 0;

        const auto pawn_list_entry = process->read<uintptr_t>(
            entity_list + 0x10 + 0x8 * ((pawn_handle & 0x7FFF) >> 9));
        if (!pawn_list_entry)
            return 0;

        return process->read<uintptr_t>(pawn_list_entry + 0x70 * (pawn_handle & 0x1FF));
    }

    std::filesystem::path LogPath()
    {
        char exe[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        return std::filesystem::path(exe).parent_path() / "bhop_log.txt";
    }

    void LogLine(const char* line)
    {
        static std::ofstream out;
        if (!out.is_open())
        {
            out.open(LogPath(), std::ios::out | std::ios::trunc);
            if (!out)
                return;
            out << "# hop ms air_ms take land peak dspd duck jump why\n";
            out << "# take=speed at leave  land=speed at touch  peak=max in air  dspd=land-take\n";
            out << "# why: ok | gained | lost_on_land | lost_in_air\n";
        }
        out << line;
        out.flush();
    }
}

void Bhop::Tick()
{
    static int duck_pulse = 0;
    static bool was_ground = true;
    static int hop = 0;
    static float take_spd = 0.f;
    static float peak_spd = 0.f;
    static float min_air = 0.f;
    static long long take_ms = 0;
    static float cached_ground = 64.f;
    static int ground_age = 8;

    auto now_ms = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    if (!cfg::movement::bhop || !(GetAsyncKeyState(VK_SPACE) & 0x8000))
    {
        duck_pulse = 0;
        was_ground = true;
        return;
    }

    auto process = Engine::GetProcess();
    if (!process || !process->handle_)
        return;

    auto client = Engine::GetClient();
    if (!client.base)
        return;

    const auto pawn = ResolvePawn(process, client.base);
    if (!pawn)
        return;

    const auto flags = process->read<uint32_t>(pawn + offsets::pawn::m_fFlags);
    const auto pos = process->read<Vec3_t>(pawn + offsets::pawn::m_vOldOrigin);
    const auto vel = process->read<Vec3_t>(pawn + offsets::pawn::m_vecAbsVelocity);
    const auto force_jump = client.base + offsets::buttons::jump;
    const bool on_ground = (flags & FL_ONGROUND) != 0;
    const bool ducked = (flags & FL_DUCKING) != 0
        || (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
        || (GetAsyncKeyState(VK_RCONTROL) & 0x8000);

    if (++ground_age >= 3)
    {
        ground_age = 0;
        float hit = 0.f;
        if (MapVisibility::Get().GroundDistance(pos, 220.f, hit))
            cached_ground = std::max(0.f, hit - 4.f);
        else if (on_ground)
            cached_ground = 0.f;
    }

    const float speed_xy = std::sqrt(vel.x * vel.x + vel.y * vel.y);

    if (!on_ground)
    {
        duck_pulse = 0;
        process->write<uint32_t>(force_jump, MINUS_JUMP);
        if (speed_xy > peak_spd)
            peak_spd = speed_xy;
        if (speed_xy < min_air)
            min_air = speed_xy;
    }
    else if (ducked)
    {
        process->write<uint32_t>(force_jump, duck_pulse == 0 ? PLUS_JUMP : MINUS_JUMP);
        duck_pulse = (duck_pulse + 1) % 3;
    }
    else
    {
        duck_pulse = 0;
        process->write<uint32_t>(force_jump, PLUS_JUMP);
    }

    const auto now = now_ms();
    if (was_ground && !on_ground)
    {
        ++hop;
        take_spd = speed_xy;
        peak_spd = speed_xy;
        min_air = speed_xy;
        take_ms = now;
    }
    else if (!was_ground && on_ground)
    {
        const long long air = now - take_ms;
        if (air >= 200)
        {
            const float dspd = speed_xy - take_spd;
            const char* why = "ok";
            if (min_air + 40.f < take_spd && min_air + 20.f < speed_xy)
                why = "lost_in_air";
            else if (dspd < -25.f)
                why = "lost_on_land";
            else if (dspd > 8.f)
                why = "gained";

            char line[256];
            std::snprintf(line, sizeof(line),
                "%d %lld %lld %.1f %.1f %.1f %+.1f %d %d %s  vz=%.1f h=%.1f\n",
                hop, now, air, take_spd, speed_xy, peak_spd, dspd,
                ducked ? 1 : 0, 1, why, vel.z, cached_ground);
            LogLine(line);
        }
    }

    was_ground = on_ground;
}
