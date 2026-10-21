#include "Overlays.hpp"
#include "Radar.hpp"

#include "updater/Updater.hpp"
#include "gui/renderer/Renderer.hpp" // Circular dependency
#include "gui/frontend/menu/Menu.hpp" // Circular dependency
#include "assets/fonts/WeaponIcons.h"
#include "config/Config.hpp"
#include "core/engine/classes/Aimbot.hpp"
#include "core/engine/classes/StrafeHelper.hpp"
#include "core/engine/types/Weapons.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"
#include <cstdio>
#include <cmath>


#include "core/engine/cache/FrameStore.hpp"

bool Overlays::Init() {
    return GetInstance().InitImpl();
}

void Overlays::Render() {
    return GetInstance().RenderImpl();
}

ImFont* Overlays::EspText() {
    return GetInstance().font_esp;
}

ImFont* Overlays::EspIcons() {
    return GetInstance().font_icons;
}

bool Overlays::InitImpl() {
    auto& io = ImGui::GetIO();

    ImFontConfig cfg{};
    cfg.FontDataOwnedByAtlas = false;

	this->font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 12.0f, &cfg);
	this->font_alt = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 14.0f, &cfg);
	this->font_esp = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\calibri.ttf", 12.0f, &cfg);
	if (!this->font_esp)
		this->font_esp = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 12.0f, &cfg);
    this->font_icons = io.Fonts->AddFontFromMemoryTTF(weapon_icon_font, weapon_icon_font_len, 16.0f,  &cfg);
	
	ImFontConfig merge_icon_cfg{};
	merge_icon_cfg.FontDataOwnedByAtlas = false;
	merge_icon_cfg.MergeMode = true;

	static const ImWchar icon_ranges[] = { 0xE000, 0xE046, 0 };
	io.Fonts->AddFontFromMemoryTTF(weapon_icon_font, weapon_icon_font_len, 12.f, &merge_icon_cfg, icon_ranges);

    return true;
 }

void Overlays::RenderImpl() {
    ImGui::PushFont(this->font);
    {
        RenderWatermark();
        RenderNotice();
        RenderFov();
        RenderStrafeDebug();
        RenderMapInfo();
        RenderRadar();
    }
    ImGui::PopFont();
}



void Overlays::RenderFov() {
    if (!cfg::aimbot::draw_fov || Renderer::IsOpen())
        return;

    const auto& snapshot = FrameStore::Get().Latest();
    if (!snapshot.in_match || snapshot.map[0] == '\0')
        return;
    if (snapshot.map[0] == '<')
        return;

    auto& io = ImGui::GetIO();
    auto* d = ImGui::GetBackgroundDrawList();
    const short held = snapshot.local_weapon;
    if (IsGrenade(held) || IsKnife(held))
        return;
    const int zoom = snapshot.local_zoom > 0 ? snapshot.local_zoom : (snapshot.local_scoped ? 1 : 0);
    const float target = Aimbot::FovRadiusPx(io.DisplaySize.x, cfg::aimbot::ActiveFov(held), zoom);
    if (target <= 0.f)
        return;

    static float shown = -1.f;
    if (shown < 0.f)
        shown = target;
    const float dt = (std::max)(io.DeltaTime, 0.f);
    const float k = 1.f - std::exp(-8.f * dt);
    shown += (target - shown) * k;

    d->AddCircle(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        shown,
        IM_COL32(255, 255, 255, 140),
        64,
        1.4f
    );
}

void Overlays::RenderStrafeDebug()
{
    if (!cfg::movement::strafe_helper)
        return;

    const auto dbg = StrafeHelper::Debug();
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "strafe  mdx=%d  held=%s  ground=%d  space=%d  tick=%d",
        static_cast<int>(dbg.dyaw),
        dbg.held > 0 ? "D" : (dbg.held < 0 ? "A" : "-"),
        dbg.on_ground ? 1 : 0,
        dbg.space ? 1 : 0,
        dbg.ticking ? 1 : 0);

    ImGui::GetBackgroundDrawList()->AddText(ImVec2(16.f, 48.f), IM_COL32(255, 220, 80, 230), buf);
}


void Overlays::RenderMapInfo()
{
    const auto& snap = FrameStore::Get().Latest();
    const char* map = snap.map[0] ? snap.map : "menu";
    const bool opt = MapVisibility::Get().Ready();
    char buf[96];
    std::snprintf(buf, sizeof(buf), "map  %s  opt=%s", map, opt ? "ok" : "no");
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(16.f, 32.f), IM_COL32(180, 210, 255, 220), buf);
}

void Overlays::RenderRadar()
{
    if (!cfg::radar::enabled)
        return;

    const auto& snap = FrameStore::Get().Latest();
    if (!snap.in_match)
        return;

    const FramePlayer* local = nullptr;
    for (int i = 0; i < snap.player_count; ++i)
    {
        if (snap.players[i].localplayer)
        {
            local = &snap.players[i];
            break;
        }
    }
    if (!local || local->pos.zero())
        return;

    auto* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
        return;

    const float size = std::clamp(cfg::radar::size, 80.f, 480.f);
    const float range = std::clamp(cfg::radar::range, 40.f, 400.f);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float half = size * 0.5f;
    float cx = cfg::radar::pos_x + half;
    float cy = cfg::radar::pos_y + half;
    cx = std::clamp(cx, half + 4.f, display.x - half - 4.f);
    cy = std::clamp(cy, half + 4.f, display.y - half - 4.f);

    const ImVec2 min(cx - half, cy - half);
    const ImVec2 max(cx + half, cy + half);
    const float opacity = std::clamp(cfg::radar::opacity, 0.05f, 1.f);
    const int bg_a = (int)(255.f * opacity);
    const int border_a = (int)(40.f * opacity);
    dl->AddRectFilled(min, max, IM_COL32(8, 8, 10, bg_a), 6.f);
    dl->AddRect(min, max, IM_COL32(255, 255, 255, border_a), 6.f, 0, 1.f);

    Base_Radar radar;
    radar.SetDrawList(dl);
    radar.SetSize(size);
    radar.SetRange(range);
    radar.SetProportion(std::clamp(cfg::radar::proportion, 400.f, 8000.f));
    radar.SetPos(Vec2_t(cx, cy));
    radar.ShowCrossLine = false;
    radar.PointType = std::clamp(cfg::radar::type, 0, 2);

    const ImColor enemy(cfg::radar::enemy.r, cfg::radar::enemy.g, cfg::radar::enemy.b, cfg::radar::enemy.a);
    const ImColor mate(cfg::radar::teammate.r, cfg::radar::teammate.g, cfg::radar::teammate.b, cfg::radar::teammate.a);

    for (int i = 0; i < snap.player_count; ++i)
    {
        const auto& pl = snap.players[i];
        if (!pl.alive || pl.localplayer || pl.pos.zero())
            continue;
        const bool teammate = !snap.is_deathmatch && pl.team == snap.local_team && pl.team != 0;
        if (teammate && !cfg::radar::show_teammates)
            continue;
        radar.AddPoint(local->pos, local->eye.y, pl.pos, teammate ? mate : enemy, radar.PointType, pl.eye.y);
    }

    radar.Render();
    dl->AddCircleFilled(ImVec2(cx, cy), 3.f, IM_COL32(255, 255, 255, 230));
}

void Overlays::RenderWatermark() {
    if (!cfg::settings::watermark)
        return;

    auto& io = ImGui::GetIO();
    auto* d = ImGui::GetBackgroundDrawList();

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char clock[16];
    std::snprintf(clock, sizeof(clock), "%02u:%02u", st.wHour, st.wMinute);

    const auto& snap = FrameStore::Get().Latest();
    const char* map = snap.map[0] ? snap.map : "menu";
    const std::string line = std::format(
        "META  ·  {}  ·  {}  ·  {:.0f} fps  ·  {}",
        clock,
        map,
        io.Framerate,
        Config::CurrentName()
    );

    const ImVec2 ts = ImGui::CalcTextSize(line.c_str());
    const float pad_x = 12.f;
    const float pad_y = 6.f;
    const ImVec2 p0(io.DisplaySize.x - ts.x - pad_x * 2.f - 14.f, 12.f);
    const ImVec2 p1(io.DisplaySize.x - 14.f, 12.f + ts.y + pad_y * 2.f);

    const auto& ac = cfg::settings::menu_color;
    const ImU32 accent = IM_COL32(
        (int)(std::clamp(ac.r, 0.f, 1.f) * 255.f),
        (int)(std::clamp(ac.g, 0.f, 1.f) * 255.f),
        (int)(std::clamp(ac.b, 0.f, 1.f) * 255.f),
        220);
    const ImU32 accent_soft = IM_COL32(
        (int)(std::clamp(ac.r, 0.f, 1.f) * 255.f),
        (int)(std::clamp(ac.g, 0.f, 1.f) * 255.f),
        (int)(std::clamp(ac.b, 0.f, 1.f) * 255.f),
        70);

    d->AddRectFilled(ImVec2(p0.x + 2.f, p0.y + 3.f), ImVec2(p1.x + 2.f, p1.y + 3.f), IM_COL32(0, 0, 0, 50), 8.f);
    d->AddRectFilled(p0, p1, IM_COL32(16, 17, 22, 210), 8.f);
    d->AddRect(p0, p1, accent_soft, 8.f);
    d->AddRectFilled(p0, ImVec2(p0.x + 3.f, p1.y), accent, 8.f);
    d->AddText(ImVec2(p0.x + pad_x, p0.y + pad_y), IM_COL32(230, 233, 238, 240), line.c_str());
}

void Overlays::RenderNotice() {
    static auto status = Updater::GetStatus();

    if (status.notice.empty())
        return;

    if (!Renderer::IsOpen())
        return;

    auto& io = ImGui::GetIO();
    auto d = ImGui::GetBackgroundDrawList();

    static int margin = 10;
    static int padding = 10;
    auto menu_pos = Menu::GetPos();
    auto menu_size = Menu::GetSize();

    auto max_width = menu_size.x - padding * 2;
    
    auto size = ImGui::CalcTextSize(status.notice.data(), nullptr, false, max_width);

    auto rect_start = ImVec2(menu_pos.x, menu_pos.y - margin - padding * 2 - size.y);
    auto rect_end = ImVec2(menu_pos.x + menu_size.x, menu_pos.y - margin);
    auto pos = ImVec2(rect_start.x + padding, rect_start.y + padding);

    d->AddRectFilled(
        rect_start,
        rect_end,
        IM_COL32(0, 0, 0, 200),
        10.f
    );

    d->AddRect(
        rect_start,
        rect_end,
        IM_COL32(100, 100, 100, 200),
        10.f
    );

    d->AddText(
        pos - ImVec2(0, padding + padding * 0.5),
        IM_COL32(255, 200, 0, 255),
        "Notice"
    );

    d->AddText(
        this->font,
        this->font->LegacySize,
        pos,
        IM_COL32(255, 255, 255, 255),
        status.notice.data(),
        nullptr, 
        max_width
    );
}
