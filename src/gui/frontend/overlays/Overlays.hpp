#pragma once

#include "imgui.h"

class Overlays {
public:
    ~Overlays() = default;
    Overlays(const Overlays&) = delete;
    Overlays(Overlays&&) = delete;
    Overlays& operator=(const Overlays&) = delete;
    Overlays& operator=(Overlays&&) = delete;

    static bool Init();
    static void Render();
    static ImFont* EspText();
    static ImFont* EspIcons();
private:
    ImFont* font;
    ImFont* font_alt;
    ImFont* font_icons;
    ImFont* font_esp;

private:
    Overlays() {};

    static Overlays& GetInstance()
    {
        static Overlays i{};
        return i;
    }

    bool InitImpl();
    void RenderImpl();

    void RenderNotice();
    void RenderWatermark();
    void RenderFov();
    void RenderStrafeDebug();
    void RenderMapInfo();
    void RenderRadar();
};
