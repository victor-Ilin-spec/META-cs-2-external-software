#include "Menu.hpp"

#include "gui/newmenu/includes.hh"
#include "gui/renderer/Renderer.hpp"
#include "gui/renderer/window/Window.hpp"

bool Menu::Init()
{
    return GetInstance().InitImpl();
}

void Menu::Render()
{
    return GetInstance().RenderImpl();
}

void Menu::RenderStartupHelp()
{
    return GetInstance().RenderStartupHelpImpl();
}

ImVec2 Menu::GetPos()
{
    return GetInstance().pos;
}

ImVec2 Menu::GetSize()
{
    return GetInstance().size;
}

bool Menu::InitImpl()
{
    LOGF(INFO, "Menu: render setup");
    try
    {
        if (!ImGui::GetCurrentContext())
        {
            LOGF(FATAL, "Menu: ImGui context is null");
            return false;
        }

        LOGF(INFO, "Menu: g_render->setup begin");
        g_render->setup();
        LOGF(INFO, "Menu: g_render->setup done");

        if (!g_font->f_default.get_font() || !g_font->f_childs.get_font())
        {
            LOGF(FATAL, "Menu: font load failed, menu disabled");
            isSetup = false;
            return false;
        }

        LOGF(INFO, "Menu: framework initialize");
        framework::g_menu->initialize();
        LOGF(INFO, "Menu: framework initialize done");
        isSetup = true;
        LOGF(INFO, "Successfully initialized menu...");
        return true;
    }
    catch (const std::exception& e)
    {
        LOGF(FATAL, "Menu init exception: {}", e.what());
        isSetup = false;
        return false;
    }
    catch (...)
    {
        LOGF(FATAL, "Menu init unknown exception");
        isSetup = false;
        return false;
    }
}

void Menu::RenderImpl()
{
    if (!isSetup)
        return;

    try
    {
        framework::g_ctx->m_open = Renderer::IsOpen();
        g_render->set_draw_list(ImGui::GetForegroundDrawList());
        if (g_render->draw_list())
            framework::g_menu->runtime();

        Window::vsync = cfg::settings::vsync;

        static bool last_streamproof = cfg::settings::streamproof;
        if (last_streamproof != cfg::settings::streamproof)
        {
            last_streamproof = cfg::settings::streamproof;
            Window::SetAffinity(
                Window::hwnd,
                cfg::settings::streamproof ? WindowAffinity::Invisible : WindowAffinity::Disabled);
        }
    }
    catch (const std::exception& e)
    {
        LOGF(FATAL, "Menu render exception: {}", e.what());
    }
    catch (...)
    {
        LOGF(FATAL, "Menu render unknown exception");
    }
}

void Menu::RenderStartupHelpImpl()
{
    static bool has_opened_menu = false;
    if (has_opened_menu)
        return;

    auto& io = ImGui::GetIO();
    auto d = ImGui::GetBackgroundDrawList();
    if (Renderer::IsOpen())
        has_opened_menu = true;

    const char* help =
        "To OPEN the menu, Use Insert or Right Shift keys"
        "\n\t\t\t\tTo CLOSE, press the End key";
    const auto size = ImGui::CalcTextSize(help);
    d->AddText(ImVec2(io.DisplaySize.x / 2 - size.x / 2, 80), IM_COL32(255, 255, 255, 255), help);
}
