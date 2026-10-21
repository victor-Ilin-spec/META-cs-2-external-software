#include "StrafeHelper.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"
#include "core/offsets/Offsets.hpp"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {
    constexpr uint32_t FL_ONGROUND = 1u << 0;
    constexpr uint32_t FL_DUCKING = 1u << 1;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kRad = kPi / 180.f;
    StrafeDebug g_debug{};

    IDirectInput8* g_dinput = nullptr;
    IDirectInputDevice8* g_mouse = nullptr;
    bool g_mouse_ok = false;

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

    bool EnsureMouse()
    {
        if (g_mouse_ok && g_mouse)
            return true;

        if (!g_dinput)
        {
            if (FAILED(DirectInput8Create(
                    GetModuleHandle(nullptr),
                    DIRECTINPUT_VERSION,
                    IID_IDirectInput8,
                    reinterpret_cast<void**>(&g_dinput),
                    nullptr)))
                return false;
        }

        if (!g_mouse)
        {
            if (FAILED(g_dinput->CreateDevice(GUID_SysMouse, &g_mouse, nullptr)))
                return false;

            if (FAILED(g_mouse->SetDataFormat(&c_dfDIMouse)))
                return false;

            HWND hwnd = nullptr;
            if (auto process = Engine::GetProcess(); process)
                hwnd = process->hwnd_;
            if (!hwnd)
                hwnd = GetDesktopWindow();

            if (FAILED(g_mouse->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
                return false;
        }

        const HRESULT hr = g_mouse->Acquire();
        g_mouse_ok = SUCCEEDED(hr) || hr == DI_NOEFFECT || hr == S_FALSE;
        return g_mouse_ok;
    }

    int ReadMouseX()
    {
        if (!EnsureMouse())
            return 0;

        DIMOUSESTATE state{};
        HRESULT hr = g_mouse->GetDeviceState(sizeof(state), &state);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
        {
            g_mouse_ok = false;
            if (!EnsureMouse())
                return 0;
            hr = g_mouse->GetDeviceState(sizeof(state), &state);
        }
        if (FAILED(hr))
            return 0;

        return state.lX;
    }

    int DirFromVelocity(const Vec3_t& vel, float yaw)
    {
        const float speed2 = vel.x * vel.x + vel.y * vel.y;
        if (speed2 < 400.f)
            return 0;

        const float rad = yaw * kRad;
        const float fwd_x = std::cos(rad);
        const float fwd_y = std::sin(rad);
        const float cross = fwd_x * vel.y - fwd_y * vel.x;
        if (cross > 20.f)
            return 1;
        if (cross < -20.f)
            return -1;
        return 0;
    }
}

StrafeDebug StrafeHelper::Debug()
{
    return g_debug;
}
void StrafeHelper::Tick()
{
    static int held = 0;
    static float smooth_x = 0.f;
    static int last_dir = 0;

    auto send_key_sc = [](WORD scan, bool down)
    {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = scan;
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &in, sizeof(INPUT));
    };

    auto set_dir = [&](int dir)
    {
        if (dir == held)
            return;

        if (held == -1) send_key_sc(0x1E, false);
        if (held ==  1) send_key_sc(0x20, false);

        if (dir == -1) send_key_sc(0x1E, true);
        if (dir ==  1) send_key_sc(0x20, true);

        held = dir;
        g_debug.held = held;
        if (dir != 0)
            last_dir = dir;
    };

    auto release = [&]
    {
        set_dir(0);
        smooth_x = 0.f;
        last_dir = 0;
        g_debug.ticking = false;
        g_debug.dyaw = 0.f;
    };

    g_debug.space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    g_debug.ticking = false;

    if (!cfg::movement::strafe_helper)
        return release();

    if (!g_debug.space)
        return release();

    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!process || !process->handle_ || !client.base)
        return release();

    const auto pawn = ResolvePawn(process, client.base);
    if (!pawn)
        return release();

    const auto flags = process->read<uint32_t>(pawn + offsets::pawn::m_fFlags);
    const auto vel = process->read<Vec3_t>(pawn + offsets::pawn::m_vecAbsVelocity);
    g_debug.on_ground = (flags & FL_ONGROUND) != 0;

    const int mouse_x = ReadMouseX();
    smooth_x = smooth_x * 0.45f + static_cast<float>(mouse_x) * 0.55f;
    g_debug.dyaw = smooth_x;
    g_debug.ticking = true;


    const auto angles = process->read<Vec3_t>(pawn + offsets::pawn::m_angEyeAngles);
    g_debug.yaw = angles.y;
    g_debug.ticking = true;

    int want = 0;
    if (smooth_x > 0.7f)
        want = 1;
    else if (smooth_x < -0.7f)
        want = -1;
    else if (last_dir != 0)
        want = last_dir;
    else
        want = DirFromVelocity(vel, angles.y);

    if (want != 0)
        set_dir(want);
}
