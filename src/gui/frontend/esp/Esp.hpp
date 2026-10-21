#pragma once

class Esp {
public:
    Esp(const Esp&) = delete;
    Esp& operator=(const Esp&) = delete;

    static void Render();

private:
    Esp() = default;

    static Esp& Get()
    {
        static Esp i;
        return i;
    }

    void RenderImpl();
};
