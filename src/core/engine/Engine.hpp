#pragma once
#include "config/Config.hpp"
#include "core/memory/Memory.hpp"
#include <atomic>
#include <thread>

class Engine {
public:
    ~Engine() { StopImpl(); }
    Engine(const Engine&)            = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(const Engine&) = delete;
    Engine& operator=(Engine&&)      = delete;

   static bool Init();
   static ProcessModule GetClient();
   static ProcessModule GetEngine();
   static std::shared_ptr<pProcess> GetProcess(); // Refactor this so its easier to access
   static void Stop();
   static bool IsGameFocused();
   static bool IsGameUiOpen();
private:
    Engine() {};

    static Engine& GetInstance()
    {
        static Engine i{};
        return i;
    }

    bool InitImpl();

    bool AwaitProcess();
    bool AwaitModules();

    void Thread();
    void StopImpl();

private:
    std::shared_ptr<pProcess> process;
    ProcessModule client;
    ProcessModule engine;
    std::atomic<bool> running{true};
    std::thread worker;
};