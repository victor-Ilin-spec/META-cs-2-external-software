#pragma once

class AutoStop {
public:
    static void Tick();
    static void NotifyShot();
    static void NotifyKill();
    static bool Holding();
};
