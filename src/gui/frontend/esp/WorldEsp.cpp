#include "WorldEsp.hpp"
#include "core/engine/world/World.hpp"
#include "core/engine/world/GrenadePredict.hpp"
#include "core/engine/cache/FrameStore.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#include "imgui.h"

#include <algorithm>
#include <cfloat>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

ImU32 Col(float r, float g, float b, float a = 1.f)
{
    return IM_COL32(
        (int)(std::clamp(r, 0.f, 1.f) * 255.f),
        (int)(std::clamp(g, 0.f, 1.f) * 255.f),
        (int)(std::clamp(b, 0.f, 1.f) * 255.f),
        (int)(std::clamp(a, 0.f, 1.f) * 255.f));
}

void OutlinedText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text)
{
    const float sz = font ? font->LegacySize : 12.f;
    const ImU32 outline = IM_COL32(0, 0, 0, 220);
    if (font)
    {
        dl->AddText(font, sz, ImVec2(pos.x + 1.f, pos.y + 1.f), outline, text);
        dl->AddText(font, sz, pos, col, text);
    }
    else
    {
        dl->AddText(ImVec2(pos.x + 1.f, pos.y + 1.f), outline, text);
        dl->AddText(pos, col, text);
    }
}

enum class ItemCat { rifle, smg, shotgun, sniper, pistol, heavy, grenade, utility };

ItemCat CategoryOf(short id)
{
    switch (id)
    {
    case weapon_ak47: case weapon_m4a1: case weapon_m4a1_silencer:
    case weapon_aug: case weapon_famas: case weapon_galilar: case weapon_sg556:
        return ItemCat::rifle;
    case weapon_mac10: case weapon_mp5sd: case weapon_mp7: case weapon_mp9:
    case weapon_bizon: case weapon_p90: case weapon_ump45:
        return ItemCat::smg;
    case weapon_nova: case weapon_sawedoff: case weapon_xm1014: case weapon_mag7:
        return ItemCat::shotgun;
    case weapon_awp: case weapon_ssg08: case weapon_g3sg1: case weapon_scar20:
        return ItemCat::sniper;
    case weapon_deagle: case weapon_elite: case weapon_fiveseven: case weapon_glock:
    case weapon_hkp2000: case weapon_usp_silencer: case weapon_p250: case weapon_cz75a:
    case weapon_tec9: case weapon_revolver:
        return ItemCat::pistol;
    case weapon_m249: case weapon_negev:
        return ItemCat::heavy;
    case weapon_hegrenade: case weapon_flashbang: case weapon_smokegrenade:
    case weapon_molotov: case weapon_incgrenade: case weapon_decoy:
        return ItemCat::grenade;
    default:
        return ItemCat::utility;
    }
}

bool PassesFilter(short id)
{
    switch (CategoryOf(id))
    {
    case ItemCat::rifle: return cfg::world::item_rifles;
    case ItemCat::smg: return cfg::world::item_smgs;
    case ItemCat::shotgun: return cfg::world::item_shotguns;
    case ItemCat::sniper: return cfg::world::item_snipers;
    case ItemCat::pistol: return cfg::world::item_pistols;
    case ItemCat::heavy: return cfg::world::item_heavy;
    case ItemCat::grenade: return cfg::world::item_grenades;
    case ItemCat::utility: return cfg::world::item_utility;
    }
    return true;
}

ImU32 ProjColor(WorldProjectile::Type t)
{
    switch (t)
    {
    case WorldProjectile::Type::he: return Col(220.f / 255.f, 150.f / 255.f, 150.f / 255.f, 0.86f);
    case WorldProjectile::Type::flash: return Col(230.f / 255.f, 220.f / 255.f, 150.f / 255.f, 0.86f);
    case WorldProjectile::Type::smoke: return Col(160.f / 255.f, 200.f / 255.f, 180.f / 255.f, 0.86f);
    case WorldProjectile::Type::molotov:
    case WorldProjectile::Type::inferno: return Col(220.f / 255.f, 170.f / 255.f, 130.f / 255.f, 0.86f);
    case WorldProjectile::Type::decoy: return Col(170.f / 255.f, 175.f / 255.f, 200.f / 255.f, 0.78f);
    default: return Col(195.f / 255.f, 200.f / 255.f, 215.f / 255.f, 0.78f);
    }
}

const char* ProjName(WorldProjectile::Type t)
{
    switch (t)
    {
    case WorldProjectile::Type::he: return "HE";
    case WorldProjectile::Type::flash: return "Flash";
    case WorldProjectile::Type::smoke: return "Smoke";
    case WorldProjectile::Type::molotov: return "Molotov";
    case WorldProjectile::Type::inferno: return "Fire";
    case WorldProjectile::Type::decoy: return "Decoy";
    default: return "Nade";
    }
}

short ProjIconId(WorldProjectile::Type t)
{
    switch (t)
    {
    case WorldProjectile::Type::he: return weapon_hegrenade;
    case WorldProjectile::Type::flash: return weapon_flashbang;
    case WorldProjectile::Type::smoke: return weapon_smokegrenade;
    case WorldProjectile::Type::molotov:
    case WorldProjectile::Type::inferno: return weapon_molotov;
    case WorldProjectile::Type::decoy: return weapon_decoy;
    default: return weapon_hegrenade;
    }
}

void DrawTimer(ImDrawList* dl, float x, float y, float remaining, float frac)
{
    const float w = 28.f;
    const float h = 3.f;
    dl->AddRectFilled(ImVec2(x - w * 0.5f, y), ImVec2(x + w * 0.5f, y + h), IM_COL32(15, 16, 22, 150));
    const float fill = w * std::clamp(frac, 0.f, 1.f);
    const ImU32 hi = IM_COL32(140, 150, 235, 255);
    const ImU32 lo = IM_COL32(220, 100, 100, 255);
    const ImU32 c = frac > 0.35f ? hi : lo;
    dl->AddRectFilled(ImVec2(x - w * 0.5f, y), ImVec2(x - w * 0.5f + fill, y + h), c);
    (void)remaining;
}

void DrawInferno(ImDrawList* dl, const view_matrix_t& matrix, const Vec2_t& screen, const WorldProjectile& proj)
{
    constexpr float radius = 60.f;
    constexpr int ring = 12;
    std::vector<ImVec2> pts;
    pts.reserve(static_cast<size_t>(proj.fire_count) * ring);
    for (int i = 0; i < proj.fire_count; ++i)
    {
        const auto& c = proj.fire_points[static_cast<size_t>(i)];
        for (int k = 0; k < ring; ++k)
        {
            const float a = (k / static_cast<float>(ring)) * 6.2831853f;
            Vec3_t w{ c.x + std::cosf(a) * radius, c.y + std::sinf(a) * radius, c.z };
            Vec2_t s{};
            if (matrix.wts(w, screen, s, false))
                pts.push_back(ImVec2(s.x, s.y));
        }
    }
    if (pts.size() < 3)
        return;

    std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });

    auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<ImVec2> hull;
    hull.reserve(pts.size() * 2);
    for (const auto& p : pts)
    {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), p) <= 0.f)
            hull.pop_back();
        hull.push_back(p);
    }
    const size_t lower = hull.size();
    for (int i = static_cast<int>(pts.size()) - 1; i >= 0; --i)
    {
        const auto& p = pts[static_cast<size_t>(i)];
        while (hull.size() > lower && cross(hull[hull.size() - 2], hull.back(), p) <= 0.f)
            hull.pop_back();
        hull.push_back(p);
    }
    if (hull.size() < 4)
        return;
    hull.pop_back();
    dl->AddConvexPolyFilled(hull.data(), static_cast<int>(hull.size()), IM_COL32(220, 170, 130, 50));
    dl->AddPolyline(hull.data(), static_cast<int>(hull.size()), IM_COL32(220, 170, 130, 150), ImDrawFlags_Closed, 2.f);
}

void DrawItems(const WorldSnapshot& world, const FrameData& frame)
{
    if (!cfg::world::items)
        return;

    auto* dl = ImGui::GetBackgroundDrawList();
    ImFont* text = Overlays::EspText();
    ImFont* icons = Overlays::EspIcons();
    const Vec2_t screen = ImGui::GetIO().DisplaySize;
    const Vec3_t eye = frame.local_eye;

    for (const auto& item : world.items)
    {
        if (!PassesFilter(item.weapon_id))
            continue;
        const float dist_m = eye.dist_to_3d(item.origin) * 0.01905f;
        if (dist_m > cfg::world::item_distance)
            continue;
        Vec2_t s{};
        if (!frame.matrix.wts(item.origin, screen, s, true))
            continue;

        float y = s.y;
        if (cfg::world::item_icon)
        {
            const char* ico = Weapon::IconFromIndex(item.weapon_id);
            if (ico && ico[0] && icons)
            {
                const ImVec2 ts = icons->CalcTextSizeA(icons->LegacySize, FLT_MAX, 0.f, ico);
                OutlinedText(dl, icons, ImVec2(s.x - ts.x * 0.5f, y), Col(0.76f, 0.78f, 0.84f, 0.9f), ico);
                y += ts.y - 2.f;
            }
        }
        if (cfg::world::item_name)
        {
            const char* name = Weapon::NameFromIndex(item.weapon_id);
            const ImVec2 ts = text
                ? text->CalcTextSizeA(text->LegacySize, FLT_MAX, 0.f, name)
                : ImGui::CalcTextSize(name);
            OutlinedText(dl, text, ImVec2(s.x - ts.x * 0.5f, y), Col(0.76f, 0.78f, 0.84f, 0.85f), name);
            y += ts.y - 2.f;
        }
        if (cfg::world::item_ammo && item.max_ammo > 0)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", item.ammo);
            const ImVec2 ts = text
                ? text->CalcTextSizeA(text->LegacySize, FLT_MAX, 0.f, buf)
                : ImGui::CalcTextSize(buf);
            const ImU32 c = item.ammo > 0 ? Col(140.f / 255.f, 150.f / 255.f, 235.f / 255.f, 0.78f)
                                          : Col(180.f / 255.f, 80.f / 255.f, 80.f / 255.f, 0.78f);
            OutlinedText(dl, text, ImVec2(s.x - ts.x * 0.5f, y), c, buf);
        }
    }
}

void DrawProjectiles(const WorldSnapshot& world, const FrameData& frame)
{
    if (!cfg::world::projectiles)
        return;

    auto* dl = ImGui::GetBackgroundDrawList();
    ImFont* text = Overlays::EspText();
    ImFont* icons = Overlays::EspIcons();
    const Vec2_t screen = ImGui::GetIO().DisplaySize;

    for (const auto& proj : world.projectiles)
    {
        if (proj.origin.length_sqr() < 1.f || proj.detonated)
            continue;

        if (proj.type == WorldProjectile::Type::inferno && cfg::world::inferno_bounds && proj.fire_count > 0)
            DrawInferno(dl, frame.matrix, screen, proj);

        Vec2_t s{};
        if (!frame.matrix.wts(proj.origin, screen, s, true))
            continue;

        const ImU32 col = ProjColor(proj.type);
        float y = s.y;
        if (cfg::world::proj_icon)
        {
            const char* ico = Weapon::IconFromIndex(ProjIconId(proj.type));
            if (ico && ico[0] && icons)
            {
                const ImVec2 ts = icons->CalcTextSizeA(icons->LegacySize, FLT_MAX, 0.f, ico);
                OutlinedText(dl, icons, ImVec2(s.x - ts.x * 0.5f, y), col, ico);
                y += ts.y - 2.f;
            }
        }
        if (cfg::world::proj_name)
        {
            const char* name = ProjName(proj.type);
            const ImVec2 ts = text
                ? text->CalcTextSizeA(text->LegacySize, FLT_MAX, 0.f, name)
                : ImGui::CalcTextSize(name);
            OutlinedText(dl, text, ImVec2(s.x - ts.x * 0.5f, y), col, name);
            y += ts.y + 1.f;
        }
        if (cfg::world::proj_timer)
        {
            if (proj.type == WorldProjectile::Type::inferno && proj.expire_time > 0.f)
            {
                const float remaining = (std::max)(0.f, proj.expire_time - world.curtime);
                DrawTimer(dl, s.x, y, remaining, remaining / 7.f);
            }
            else if (proj.type == WorldProjectile::Type::smoke && proj.smoke_active)
            {
                const float start = static_cast<float>(proj.effect_tick_begin) * (1.f / 64.f);
                const float remaining = (std::max)(0.f, 18.f - (world.curtime - start));
                DrawTimer(dl, s.x, y, remaining, remaining / 18.f);
            }
            else if (proj.type == WorldProjectile::Type::decoy && proj.effect_tick_begin > 0)
            {
                const float start = static_cast<float>(proj.effect_tick_begin) * (1.f / 64.f);
                const float remaining = (std::max)(0.f, 15.f - (world.curtime - start));
                DrawTimer(dl, s.x, y, remaining, remaining / 15.f);
            }
        }
    }
}

} // namespace

void WorldEsp::Render()
{
    if (!cfg::world::items && !cfg::world::projectiles && !cfg::world::grenade_pred)
        return;

    const auto world = World::Copy();
    const auto& frame = FrameStore::Get().Latest();
    DrawItems(world, frame);
    DrawProjectiles(world, frame);
    GrenadePredict::Render();
}
