#include "AutoAccept.hpp"

#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"
#include "core/offsets/Offsets.hpp"

#include <Windows.h>
#include <chrono>

namespace {
    bool AcceptPoint(POINT& out)
    {
        auto process = Engine::GetProcess();
        if (!process || !process->hwnd_ || !IsWindow(process->hwnd_))
            return false;

        HWND wnd = GetAncestor(process->hwnd_, GA_ROOT);
        if (!wnd)
            wnd = process->hwnd_;

        RECT rc{};
        if (!GetClientRect(wnd, &rc))
            return false;

        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0)
            return false;

        POINT pt{ w / 2, static_cast<int>(h / 2.5f) };
        if (!ClientToScreen(wnd, &pt))
            return false;

        out = pt;
        return true;
    }
}

void AutoAccept::Tick()
{
    using clock = std::chrono::steady_clock;
    static bool already_accepted = false;
    static int phase = 0;
    static clock::time_point next_poll{};
    static clock::time_point phase_until{};

    const auto now = clock::now();
    if (now < next_poll)
        return;

    if (!cfg::settings::auto_accept || offsets::autoAccept == 0)
    {
        already_accepted = false;
        phase = 0;
        next_poll = now + std::chrono::milliseconds(500);
        return;
    }

    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!process || !process->handle_ || !client.base)
        return;

    BYTE buf[6]{};
    if (!process->read_raw(client.base + offsets::autoAccept, buf, sizeof(buf)))
        return;

    bool match_found = false;
    for (int i = 0; i < 6; ++i)
    {
        if (buf[i] != 0)
        {
            match_found = true;
            break;
        }
    }

    if (!match_found)
    {
        already_accepted = false;
        phase = 0;
        next_poll = now + std::chrono::milliseconds(400);
        return;
    }

    if (already_accepted)
    {
        next_poll = now + std::chrono::milliseconds(400);
        return;
    }

    if (phase == 0)
    {
        phase = 1;
        phase_until = now + std::chrono::milliseconds(1500 + (GetTickCount() % 1500));
        next_poll = now + std::chrono::milliseconds(50);
        return;
    }

    if (now < phase_until)
    {
        next_poll = now + std::chrono::milliseconds(50);
        return;
    }

    if (!Engine::IsGameFocused() || Renderer::IsOpen())
    {
        next_poll = now + std::chrono::milliseconds(50);
        return;
    }

    if (phase == 1)
    {
        POINT pt{};
        if (!AcceptPoint(pt))
        {
            next_poll = now + std::chrono::milliseconds(50);
            return;
        }
        SetCursorPos(pt.x, pt.y);
        phase = 2;
        phase_until = now + std::chrono::milliseconds(80);
        next_poll = now + std::chrono::milliseconds(16);
        return;
    }

    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    already_accepted = true;
    phase = 0;
    next_poll = now + std::chrono::milliseconds(400);
}
