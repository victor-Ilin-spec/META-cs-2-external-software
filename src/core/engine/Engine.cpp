#include "Engine.hpp"

#include "core/offsets/Dumper.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"

#include "core/engine/classes/Bhop.hpp"
#include "core/engine/classes/StrafeHelper.hpp"
#include "core/engine/classes/Aimbot.hpp"
#include "core/engine/classes/Triggerbot.hpp"
#include "core/engine/classes/AutoFire.hpp"
#include "core/engine/classes/AutoStop.hpp"
#include "core/engine/classes/AutoScope.hpp"
#include "core/engine/classes/AutoAccept.hpp"
#include "core/engine/classes/RCS.hpp"
#include "gui/renderer/Renderer.hpp"
#include "core/engine/world/World.hpp"
#include "core/engine/world/GrenadePredict.hpp"

#include <immintrin.h>

bool Engine::Init() {
    return GetInstance().InitImpl();
}

ProcessModule Engine::GetClient() {
    return GetInstance().client;
}

ProcessModule Engine::GetEngine() {
    return GetInstance().engine;
}

std::shared_ptr<pProcess> Engine::GetProcess() {
    return GetInstance().process;
}

bool Engine::IsGameFocused() {
    auto p = GetInstance().process;
    if (!p || !p->pid_)
        return false;

    p->UpdateHWND();
    if (!p->hwnd_ || !IsWindow(p->hwnd_))
        return false;

    HWND root = GetAncestor(p->hwnd_, GA_ROOT);
    if (!root)
        root = p->hwnd_;
    if (IsIconic(p->hwnd_) || IsIconic(root))
        return false;

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(root, &wp) && wp.showCmd == SW_SHOWMINIMIZED)
        return false;

    const HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    DWORD fg_pid = 0;
    GetWindowThreadProcessId(fg, &fg_pid);
    if (fg_pid != p->pid_)
        return false;

    HWND fg_root = GetAncestor(fg, GA_ROOT);
    if (IsIconic(fg) || (fg_root && IsIconic(fg_root)))
        return false;

    return true;
}

bool Engine::IsGameUiOpen() {
    if (!IsGameFocused())
        return false;

    auto p = GetInstance().process;
    if (!p || !p->hwnd_)
        return false;

    const DWORD game_tid = GetWindowThreadProcessId(p->hwnd_, nullptr);
    const DWORD self_tid = GetCurrentThreadId();
    const bool attached = game_tid && game_tid != self_tid &&
        AttachThreadInput(self_tid, game_tid, TRUE);

    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    const bool ok = GetCursorInfo(&ci) != FALSE;

    if (attached)
        AttachThreadInput(self_tid, game_tid, FALSE);

    return ok && (ci.flags & CURSOR_SHOWING) != 0;
}

void Engine::Stop() {
    return GetInstance().StopImpl();
}

bool Engine::InitImpl() {
    process = std::make_shared<pProcess>();

    if (!this->AwaitProcess()) {
        LOGF(FATAL, "Could not find process, please make sure the game is open");
        return false;
    }

    if (!this->AwaitModules()) {
        LOGF(FATAL, "Game took too long to load, please open me again once its fully loaded");
        return false;
    }

    if (!Dumper::Init()) {
        LOGF(FATAL, "Failed to dump game offsets");
        return false;
    }
    MapVisibility::EnsureMapsDir();


    if (!Config::Read())
        LOGF(WARNING, "Failed to parse config, using default values");

#ifdef _DEBUG
    if (!cfg::dev::console)
        LogHelper::Free();
#endif

    worker = std::thread(&Engine::Thread, this);

    LOGF(INFO, "Successfully initialized engine...");
    return true;
}

void Engine::Thread() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while (running) {
        auto start = steady_clock::now();

        Cache::Refresh();
        AutoAccept::Tick();
        AutoStop::Tick();

        if (IsGameFocused() && !Renderer::IsOpen() && !IsGameUiOpen()) {
            Bhop::Tick();
            StrafeHelper::Tick();
            Aimbot::Tick();
            Triggerbot::Tick();
            AutoScope::Tick();
            AutoFire::Tick();
            RCS::Tick();
        }

        World::Tick();
        GrenadePredict::Tick();

        if (cfg::settings::free_cpu)
        {
            const auto until = start + 1ms;
            while (steady_clock::now() < until)
                _mm_pause();
        }
    }
}

void Engine::StopImpl() {
    running = false;

    if (worker.joinable())
        worker.join();

    if (process && process->handle_) {
        process->Close();
        process->handle_ = nullptr;
    }
}

bool Engine::AwaitProcess() {
    if (!process || process->handle_) // Process not initialized, or already attached
        return false;

    int attempts = 0;
    do {
        if (process->AttachProcess("cs2.exe"))
            break;

        if (process->pid_ && !process->handle_) {
            LOGF(FATAL, "Insufficient permissions to open a handle to the process. Try running as Administrator.");
            return false;
        }

        if (!attempts)
            LOGF(INFO, "Waiting 50s for the game to open...");

        if (attempts > 10)
            return false;
        attempts++;

        std::this_thread::sleep_for(5s);
    } while (true);

    return true;
}

bool Engine::AwaitModules() {
    if (!process || !process->handle_) // Process not initialized, or not attached
        return false;

    LOGF(INFO, "Waiting for the game to open...");

    int attempts = 0;
    do {
        this->client = process->GetModule("client.dll");
        this->engine = process->GetModule("engine2.dll");

        if (this->client.base && this->engine.base)
            break;

        if (attempts > 10)
            return false;
        attempts++;

        std::this_thread::sleep_for(5s);
    } while (true);

    return true;
}
