#pragma once

class Menu {
public:
    ~Menu() = default;
    Menu(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu& operator=(Menu&&) = delete;

    static bool Init();
    static void Render();
    static void RenderStartupHelp();

    static ImVec2 GetPos();
    static ImVec2 GetSize();

private:
    Menu() = default;

    static Menu& GetInstance()
    {
        static Menu i{};
        return i;
    }

    bool InitImpl();
    void RenderImpl();
    void RenderStartupHelpImpl();

    bool isSetup = false;
    ImVec2 pos{ 200.f, 200.f };
    ImVec2 size{ 650.f, 480.f };
};
