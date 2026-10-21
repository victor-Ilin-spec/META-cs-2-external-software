#include "Renderer.hpp"
#include "window/Window.hpp"

#include "core/engine/Engine.hpp"
#include "gui/frontend/menu/Menu.hpp"
#include "gui/frontend/esp/Esp.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#include "gui/frontend/esp/WorldEsp.hpp"
#include <chrono>
#include <thread>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")






bool Renderer::Init() {
    return GetInstance().InitImpl();
}

void Renderer::Thread() {
    return GetInstance().ThreadImpl();
}

void Renderer::Destroy() {
    return GetInstance().DestroyImpl();
}   

void Renderer::Unload() {
    return GetInstance().UnloadImpl();
}

void Renderer::UnloadImpl() {
    LOGF(INFO, "Unload requested from menu");
    try { Config::Write(); } catch (...) {}
    Window::shouldRun = false;
    isRunning = false;
    TerminateProcess(GetCurrentProcess(), 0);
}

bool Renderer::IsOpen() {
    return GetInstance().isOpen;
}

bool Renderer::IsFocused() {
    return GetInstance().isFocused;
}

bool Renderer::InitImpl() {
    LOGF(INFO, "Renderer: spawn overlay window");
    if (!Window::SpawnWindow()) {
        LOGF(FATAL, "Failed to create window");
        return false;
    }

    LOGF(INFO, "Renderer: create d3d device");
    if (!Window::CreateDevice()) {
        LOGF(FATAL, "Failed to create device");
        return false;
    }

    LOGF(INFO, "Renderer: create imgui");
    if (!Window::CreateImGui()) {
        LOGF(FATAL, "Failed to create ImGui");
        return false;
    }

    LOGF(INFO, "Renderer: init menu");
    if (!Menu::Init())
        LOGF(WARNING, "Menu failed to initialize, overlay will run without menu");

    LOGF(INFO, "Renderer: init overlays");
    Overlays::Init();

    if (auto process = Engine::GetProcess(); process && process->hwnd_)
        SetForegroundWindow(process->hwnd_);

    if (cfg::settings::streamproof)
        Window::SetAffinity(Window::hwnd, WindowAffinity::Invisible);

    Window::vsync = false;


    LOGF(INFO, "Successfully initialized renderer...");
    return true;
}

void Renderer::DestroyImpl() {
    isRunning = false; // Prepare to stop thread loop
    LOGF(VERBOSE, "Successfully programed renderer destruction...");
}

void Renderer::ThreadImpl() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    LOGF(INFO, "Renderer: entering main loop");

    timeBeginPeriod(1);
    using clock = std::chrono::steady_clock;
    constexpr auto frame = std::chrono::nanoseconds(1'000'000'000 / 300);
    auto next = clock::now();

    while (isRunning && Window::shouldRun) {
        try {
            Window::vsync = false;
            Render();
            const bool toggled = HandleState();
            if (!toggled)
                HandleWindowOrder();
        }
        catch (const std::exception& e) {
            LOGF(FATAL, "Renderer loop exception: {}", e.what());
        }
        catch (...) {
            LOGF(FATAL, "Renderer loop unknown exception");
        }

        next += frame;
        const auto now = clock::now();
        if (next < now)
            next = now;
        else
            std::this_thread::sleep_until(next);
    }

    timeEndPeriod(1);
    LOGF(INFO, "Renderer: leaving main loop isRunning={} shouldRun={}", isRunning, Window::shouldRun);
    Window::DestroyImGui();
    Window::DestroyDevice();
    Window::DespawnWindow();
}



void Renderer::Render() {
    Window::StartRender();

    Esp::Render();
    WorldEsp::Render();

    Overlays::Render();

    Menu::RenderStartupHelp();
    if (isOpen) Menu::Render();

    Window::EndRender();
}

bool Renderer::HandleState() {
    isRunning = Window::shouldRun;

    static bool was_insert = false;
    static bool was_rshift = false;
    static bool was_end = false;

    const bool raw_insert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    const bool raw_rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
    const bool raw_end = (GetAsyncKeyState(VK_END) & 0x8000) != 0;

    const bool insert_edge = raw_insert && !was_insert;
    const bool rshift_edge = raw_rshift && !was_rshift;
    const bool end_edge = raw_end && !was_end;

    was_insert = raw_insert;
    was_rshift = raw_rshift;
    was_end = raw_end;

    const bool focused =
        Engine::IsGameFocused() || GetForegroundWindow() == Window::hwnd;

    if (!focused)
        return false;

    const bool should_toggle = insert_edge || rshift_edge || end_edge;
    if (!should_toggle)
        return false;

    this->isOpen = !this->isOpen;

    if (this->isOpen) {
        SetForegroundWindow(Window::hwnd);
        Window::SetClickthrough(Window::hwnd, false);
    } else {
        Window::SetClickthrough(Window::hwnd, true);
        if (auto process = Engine::GetProcess(); process && process->hwnd_)
            SetForegroundWindow(process->hwnd_);
    }

    LOGF(VERBOSE, "Toggled menu to {}", this->isOpen);
    std::thread(Config::Write).detach();

    if (end_edge)
        this->isRunning = false;

    return true;
}

bool Renderer::HandleWindowOrder() {
    auto p = Engine::GetProcess();
    if (!p)
        return false;

    if (!p->hwnd_ || !IsWindow(p->hwnd_))
    {
        if (!p->UpdateHWND() || !IsWindow(p->hwnd_))
            return false;
    }

    static bool overlay_visible = true;
    auto foreground = GetForegroundWindow();
    this->isFocused = (foreground == Window::hwnd || foreground == p->hwnd_);

    if (!this->isFocused && overlay_visible) {
        LOGF(VERBOSE, "Hiding overlay window as the game its not focused");
        ShowWindow(Window::hwnd, SW_HIDE);
        overlay_visible = false;
        return true;
    }

    if (!overlay_visible && this->isFocused) {  
        LOGF(VERBOSE, "Showing overlay window as the game is focused");
        ShowWindow(Window::hwnd, SW_SHOW);
        overlay_visible = true;
        return true;
    }

    static RECT last_rect = { 0, 0, 0, 0 };

    RECT window_rect;
    if (!GetWindowRect(p->hwnd_, &window_rect))
        return false;

    // All good, no movements from the client
    if (memcmp(&window_rect, &last_rect, sizeof(RECT)) == 0)
        return true;

    RECT client_rect;
    if (!GetClientRect(p->hwnd_, &client_rect))
        return false;

    POINT top_left = { client_rect.left, client_rect.top };
    POINT bottom_right = { client_rect.right, client_rect.bottom };

    ClientToScreen(p->hwnd_, &top_left);
    ClientToScreen(p->hwnd_, &bottom_right);

    RECT screen_rect = { top_left.x, top_left.y, bottom_right.x, bottom_right.y };

    SetWindowPos(
        Window::hwnd,
        HWND_TOPMOST,
        screen_rect.left,
        screen_rect.top,
        screen_rect.right - screen_rect.left,
        screen_rect.bottom - screen_rect.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    last_rect = window_rect;

    return true;
}