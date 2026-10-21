/*
    This project is licensed under CC BY-NC 4.0
    https://creativecommons.org/licenses/by-nc/4.0*
*/

#include <iostream>
#include <exception>
#include <limits>
#include "updater/Updater.hpp"
#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"

#include <external/exception.hpp>

namespace {
    const char* g_last_step = "startup";
    void Step(const char* name)
    {
        g_last_step = name;
        LOGF(INFO, "step: {}", name);
    }

    void HoldConsole(const char* why, int code = 0)
    {
        LOGF(FATAL, "why exiting: {}", why);
        LOGF(FATAL, "last successful step: {}", g_last_step);
        if (code)
            LOGF(FATAL, "error code: {}", code);
        LOGF(INFO, "Press Enter to close...");
        std::cin.clear();
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        std::cin.get();
    }
}

int main()
{
    c_exception_handler::setup();

    LogHelper::Init();
    LOGF(INFO, "Compiled {}, Welcome to META!", __TIMESTAMP__);

    try
    {
        if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
            LOGF(WARNING, "Could not set application process priority to HIGH");

        Step("updater");
        if (!Updater::Init() || !Updater::Process()) {
            LOGF(FATAL, "Updater failed to run, the application has not verified its status, execution its not recommended");
            LOGF(INFO, "Press any key to ignore and continue execution...");
            std::cin.get();
        }

        Step("engine");
        if (!Engine::Init()) {
            HoldConsole("Engine failed to initialize");
            LogHelper::Destroy();
            return 1;
        }

        Step("renderer");
        if (!Renderer::Init()) {
            Engine::Stop();
            HoldConsole("Renderer failed to initialize");
            LogHelper::Destroy();
            return 1;
        }

        Step("main loop");
        LOGF(INFO, "Everything setup and ready, just... make sure you are not in \"Full Screen\"!");
        Renderer::Thread();
        Engine::Stop();

        LOGF(INFO, "Unloaded");
        LogHelper::Destroy();
        LogHelper::Free();
        ExitProcess(0);
    }
    catch (const std::exception& e)
    {
        LOGF(FATAL, "unhandled exception: {}", e.what());
        HoldConsole(e.what());
        LogHelper::Destroy();
        return 1;
    }
    catch (...)
    {
        HoldConsole("unknown exception");
        LogHelper::Destroy();
        return 1;
    }
}
