#include "Esp.hpp"

#include "core/engine/cache/FrameStore.hpp"

#include "core/engine/classes/Aimbot.hpp"
#include "core/engine/classes/Bones.hpp"
#include "core/engine/classes/Hitbox.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "gui/frontend/overlays/Overlays.hpp"


#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <vector>

namespace {
    struct EspStyle {
        bool box = false;
        bool corner_box = false;
        bool name = false;
        bool skeleton = false;
        bool healthbar = false;
        bool armorbar = false;
        bool weapon = false;
        bool weapon_icon = false;
        bool distance = false;
        bool flag_c4 = false;
        bool flag_scoped = false;
        bool flag_flashed = false;
        bool flag_defusing = false;
        bool flag_hk = false;
        bool flag_reload = false;
        bool flag_immune = false;
        bool flag_kit = false;
        float box_thickness = 1.5f;
        color_t box_visible{};
        color_t box_invisible{};
        color_t skeleton_visible{};
        color_t skeleton_invisible{};
    };

    EspStyle EnemyStyle()
    {
        return {
            cfg::esp::box, cfg::esp::corner_box, cfg::esp::name, cfg::esp::skeleton,
            cfg::esp::healthbar, cfg::esp::armorbar, cfg::esp::weapon, cfg::esp::weapon_icon, cfg::esp::distance,
            cfg::esp::flag_c4, cfg::esp::flag_scoped, cfg::esp::flag_flashed, cfg::esp::flag_defusing,
            cfg::esp::flag_hk, cfg::esp::flag_reload, cfg::esp::flag_immune, cfg::esp::flag_kit,
            cfg::esp::box_thickness, cfg::esp::box_visible, cfg::esp::box_invisible,
            cfg::esp::skeleton_visible, cfg::esp::skeleton_invisible
        };
    }

    EspStyle MateStyle()
    {
        return {
            cfg::esp::mate_box, cfg::esp::mate_corner_box, cfg::esp::mate_name, cfg::esp::mate_skeleton,
            cfg::esp::mate_healthbar, false, false, false, false,
            cfg::esp::mate_flag_c4, cfg::esp::mate_flag_scoped, cfg::esp::mate_flag_flashed, cfg::esp::mate_flag_defusing,
            false, false, false, false,
            cfg::esp::mate_box_thickness, cfg::esp::mate_box_visible, cfg::esp::mate_box_invisible,
            cfg::esp::mate_skeleton_visible, cfg::esp::mate_skeleton_invisible
        };
    }

    bool StyleActive(const EspStyle& s)
    {
        return s.box || s.name || s.skeleton || s.healthbar || s.armorbar || s.weapon || s.weapon_icon || s.distance ||
            s.flag_c4 || s.flag_scoped || s.flag_flashed || s.flag_defusing ||
            s.flag_hk || s.flag_reload || s.flag_immune || s.flag_kit;
    }

    ImFont* EspFont()
    {
        return Overlays::EspText();
    }

    float EspFontSize(ImFont* font)
    {
        return font ? font->LegacySize : 12.f;
    }

    ImVec2 EspTextSize(ImFont* font, const char* text)
    {
        if (!font)
            return ImGui::CalcTextSize(text);
        return font->CalcTextSizeA(EspFontSize(font), FLT_MAX, 0.f, text);
    }

    void EspAddText(ImDrawList* dl, ImFont* font, const ImVec2& pos, ImU32 col, const char* text)
    {
        if (font)
            dl->AddText(font, EspFontSize(font), pos, col, text);
        else
            dl->AddText(pos, col, text);
    }


    void DrawCornerBox(float x, float y, float w, float h, ImU32 color, float thickness)
    {
        const float line_w = w * 0.25f;
        const float line_h = h * 0.25f;
        auto* dl = ImGui::GetBackgroundDrawList();

        dl->AddLine(ImVec2(x, y), ImVec2(x + line_w, y), color, thickness);
        dl->AddLine(ImVec2(x, y), ImVec2(x, y + line_h), color, thickness);

        dl->AddLine(ImVec2(x + w, y), ImVec2(x + w - line_w, y), color, thickness);
        dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + line_h), color, thickness);

        dl->AddLine(ImVec2(x, y + h), ImVec2(x + line_w, y + h), color, thickness);
        dl->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - line_h), color, thickness);

        dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - line_w, y + h), color, thickness);
        dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - line_h), color, thickness);
    }

    const Vec3_t* BoneAt(const FramePlayer& player, int id)
    {
        if (id < 0 || id >= player.bone_count)
            return nullptr;
        const auto& p = player.bones[id].pos;
        if (p.zero())
            return nullptr;
        return &p;
    }



    void DrawSkeleton(const FramePlayer& player, const view_matrix_t& matrix, const Vec2_t& screen, ImU32 color)
    {
        if (player.bone_count <= 0)
            return;

        auto* dl = ImGui::GetBackgroundDrawList();
        for (const auto& link : kSkeleton)
        {
            const auto* wa = BoneAt(player, link.a);
            const auto* wb = BoneAt(player, link.b);
            if (!wa || !wb)
                continue;
            Vec2_t a{}, b{};
            if (!matrix.wts(*wa, screen, a, false) || !matrix.wts(*wb, screen, b, false))
                continue;
            dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color, 1.6f);
        }

        if (const auto* head = BoneAt(player, bone_index::head))
        {
            Vec2_t hs{};
            if (matrix.wts(*head, screen, hs, false))
                dl->AddCircleFilled(ImVec2(hs.x, hs.y), 2.5f, color);
        }
    }

    void DrawAimPoints(const FramePlayer& player, const view_matrix_t& matrix, const Vec2_t& screen)
    {
        if (!cfg::settings::debug_multipoint)
            return;

        std::vector<HitboxRaw> boxes;
        hitbox::FallbackPlayer(boxes);

        const float scale = std::clamp(cfg::aimbot::multipoint_scale, 0.35f, 1.f);
        const int count = hitbox::ClampCount(cfg::aimbot::multipoint_count);

        auto* dl = ImGui::GetBackgroundDrawList();
        for (const auto& raw : boxes)
        {
            if (raw.bone < 0 || raw.bone >= player.bone_count)
                continue;
            const Vec3_t origin = player.bones[raw.bone].pos;
            if (origin.zero())
                continue;

            const auto pts = hitbox::Generate(raw, origin, 0.f, 0.f, 0.f, 1.f, count, scale);
            for (const auto& world : pts)
            {
                Vec2_t p{};
                if (!matrix.wts(world, screen, p, false))
                    continue;
                dl->AddCircleFilled(ImVec2(p.x, p.y), 2.8f, IM_COL32(255, 220, 40, 230));
                dl->AddCircle(ImVec2(p.x, p.y), 2.8f, IM_COL32(0, 0, 0, 200), 0, 1.1f);
            }
        }
    }
    void DrawBoxESP(const FramePlayer& player, const view_matrix_t& matrix, const Vec2_t& screen, const EspStyle& style, bool mate)
    {
        const bool seen = player.visible;
        Vec3_t head = player.pos;
        head.z += 72.f;
        Vec3_t feet = player.pos;

        for (int i = 0; i < player.bone_count; ++i)
        {
            const auto& bone = player.bones[i].pos;
            if (bone.zero())
                continue;
            const float d = bone.dist_to(player.pos);
            if (d < 2.f || d > 85.f)
                continue;
            if (bone.z > head.z)
                head = bone;
            if (bone.z < feet.z)
                feet = bone;
        }

        float min_x = 1.0e9f, min_y = 1.0e9f;
        float max_x = -1.0e9f, max_y = -1.0e9f;
        int hits = 0;

        auto add = [&](const Vec3_t& world) {
            Vec2_t p{};
            if (!matrix.wts(world, screen, p, false))
                return;
            min_x = (std::min)(min_x, p.x);
            min_y = (std::min)(min_y, p.y);
            max_x = (std::max)(max_x, p.x);
            max_y = (std::max)(max_y, p.y);
            ++hits;
        };

        add(head);
        add(feet);
        add(player.pos);

        for (const auto& link : kSkeleton)
        {
            if (const auto* a = BoneAt(player, link.a))
                add(*a);
            if (const auto* b = BoneAt(player, link.b))
                add(*b);
        }

        if (style.skeleton)
        {
            const auto& sc = seen ? style.skeleton_visible : style.skeleton_invisible;
            DrawSkeleton(player, matrix, screen, IM_COL32(
                (int)(std::clamp(sc.r, 0.f, 1.f) * 255.f),
                (int)(std::clamp(sc.g, 0.f, 1.f) * 255.f),
                (int)(std::clamp(sc.b, 0.f, 1.f) * 255.f),
                (int)(std::clamp(sc.a, 0.f, 1.f) * 255.f)));
        }

        if (!mate)
            DrawAimPoints(player, matrix, screen);

        if (hits < 2)
        {
            Vec2_t origin{}, top{};
            const bool pt1 = matrix.wts(player.pos, screen, origin, false);
            Vec3_t pos_top = player.pos;
            pos_top.z += 65.f;
            if (const auto* hb = BoneAt(player, bone_index::head))
                pos_top = *hb;
            const bool pt2 = matrix.wts(pos_top, screen, top, false);
            if (!pt1 && !pt2)
                return;
            const float height = origin.y - top.y;
            const float width = height / 2.4f;
            top.x -= width / 2;
            origin.x += width / 2;
            top.y -= width / 4;
            min_x = top.x;
            min_y = top.y;
            max_x = origin.x;
            max_y = origin.y;
            hits = 2;
        }

        const float height = max_y - min_y;
        float width = max_x - min_x;
        if (height <= 1.f)
            return;

        if (width < height * 0.32f)
        {
            const float cx = (min_x + max_x) * 0.5f;
            width = height * 0.42f;
            min_x = cx - width * 0.5f;
            max_x = cx + width * 0.5f;
        }

        const float pad_x = height * 0.05f;
        const float left = min_x - pad_x;
        const float right = max_x + pad_x;
        const float top = min_y;
        const float bottom = max_y;
        const auto& c = seen ? style.box_visible : style.box_invisible;
        const ImU32 color = IM_COL32(
            (int)(std::clamp(c.r, 0.f, 1.f) * 255.f),
            (int)(std::clamp(c.g, 0.f, 1.f) * 255.f),
            (int)(std::clamp(c.b, 0.f, 1.f) * 255.f),
            (int)(std::clamp(c.a, 0.f, 1.f) * 255.f));

        if (style.box)
        {
            const float thickness = std::clamp(style.box_thickness, 0.5f, 5.f);
            if (style.corner_box)
                DrawCornerBox(left, top, right - left, bottom - top, color, thickness);
            else
                ImGui::GetBackgroundDrawList()->AddRect(
                    ImVec2(left, top),
                    ImVec2(right, bottom),
                    color,
                    0.f,
                    0,
                    thickness);
        }

        auto* dl = ImGui::GetBackgroundDrawList();
        ImFont* font = EspFont();
        const float box_x = left;
        const float box_y = top;
        const float box_w = right - left;
        const float box_h = bottom - top;

        struct LiveItem {
            const cfg::esp::flag_anchor* a;
            int kind;
            const char* text;
            ImU32 col;
            float tw, th;
            float x, y, w, h;
            char buf[48];
            ImFont* draw_font;
        };
        LiveItem items[16];
        int n = 0;
        auto add_text = [&](const cfg::esp::flag_anchor* a, const char* t, ImU32 c, ImFont* f = nullptr) {
            auto& it = items[n++];
            it.a = a;
            it.kind = 0;
            it.col = c;
            it.draw_font = f ? f : font;
            it.buf[0] = '\0';
            std::snprintf(it.buf, sizeof(it.buf), "%s", t);
            it.text = it.buf;
            const ImVec2 ts = EspTextSize(it.draw_font, it.text);
            it.tw = ts.x;
            it.th = ts.y;
            it.x = it.y = it.w = it.h = 0.f;
        };

        if (style.name && player.name[0] != '\0')
        {
            const auto& nc = cfg::esp::name_color;
            add_text(&cfg::esp::name_pos, player.name, IM_COL32(
                (int)(std::clamp(nc.r, 0.f, 1.f) * 255.f),
                (int)(std::clamp(nc.g, 0.f, 1.f) * 255.f),
                (int)(std::clamp(nc.b, 0.f, 1.f) * 255.f),
                (int)(std::clamp(nc.a, 0.f, 1.f) * 255.f)));
        }
        if (style.weapon && player.weapon_id > 0)
        {
            char wep[48];
            const char* wn = Weapon::NameFromIndex(player.weapon_id);
            if (player.ammo >= 0)
                std::snprintf(wep, sizeof(wep), "%s [%d]", wn ? wn : "?", player.ammo);
            else
                std::snprintf(wep, sizeof(wep), "%s", wn ? wn : "?");
            add_text(&cfg::esp::weapon_pos, wep, IM_COL32(230, 230, 230, 255));
        }
        if (style.weapon_icon && player.weapon_id > 0)
        {
            const char* ic = Weapon::IconFromIndex(player.weapon_id);
            if (ic && ic[0] && ic[0] != '?')
                add_text(&cfg::esp::weapon_icon_pos, ic, IM_COL32(230, 230, 230, 255), Overlays::EspIcons());
        }
        if (style.distance)
        {
            const float d = (player.pos - FrameStore::Get().Latest().local_eye).length() * 0.0254f;
            char dist[16];
            std::snprintf(dist, sizeof(dist), "%.0fm", d);
            add_text(&cfg::esp::distance_pos, dist, IM_COL32(200, 200, 200, 255));
        }
        if (style.flag_hk && player.armor > 0)
            add_text(&cfg::esp::flag_hk_pos, player.has_helmet ? "HK" : "K", IM_COL32(200, 200, 220, 255));
        if (style.flag_scoped && player.scoped)
            add_text(&cfg::esp::flag_scoped_pos, "SCOPED", IM_COL32(80, 200, 255, 255));
        if (style.flag_flashed && player.flashed)
            add_text(&cfg::esp::flag_flashed_pos, "FLASHED", IM_COL32(255, 230, 60, 255));
        if (style.flag_reload && player.reloading)
            add_text(&cfg::esp::flag_reload_pos, "RELOAD", IM_COL32(255, 160, 60, 255));
        if (style.flag_c4 && player.has_c4)
            add_text(&cfg::esp::flag_c4_pos, "C4", IM_COL32(255, 180, 40, 255));
        if (style.flag_kit && player.has_defuser)
            add_text(&cfg::esp::flag_kit_pos, "DEFUSE", IM_COL32(80, 220, 160, 255));
        if (style.flag_defusing && player.defusing)
            add_text(&cfg::esp::flag_defusing_pos, "DEFUSING", IM_COL32(80, 220, 120, 255));
        if (style.flag_immune && player.immune)
            add_text(&cfg::esp::flag_immune_pos, "IMMUNE", IM_COL32(180, 80, 255, 255));

        const float hp_th = std::clamp(cfg::esp::healthbar_thickness, 1.f, 8.f);
        if (style.healthbar)
        {
            auto& it = items[n++];
            it.a = &cfg::esp::flag_health_pos;
            it.kind = 1;
            it.text = nullptr;
            it.col = 0;
            it.draw_font = nullptr;
            it.tw = hp_th;
            it.th = box_h;
            it.x = it.y = it.w = it.h = 0.f;
            it.buf[0] = '\0';
        }
        if (style.armorbar && player.armor > 0)
        {
            auto& it = items[n++];
            it.a = &cfg::esp::armorbar_pos;
            it.kind = 2;
            it.text = nullptr;
            it.col = IM_COL32(80, 160, 255, 255);
            it.draw_font = nullptr;
            it.tw = 2.f;
            it.th = box_h;
            it.x = it.y = it.w = it.h = 0.f;
            it.buf[0] = '\0';
        }

        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (items[j].a->order < items[i].a->order)
                    std::swap(items[i], items[j]);
        for (int i = 0; i < n; ++i)
            if (items[i].kind == 0)
                items[i].text = items[i].buf;

        float used_bar[4] = {};
        float used_txt[12] = {};
        for (int i = 0; i < n; ++i)
        {
            auto& it = items[i];
            if (it.kind == 0)
                continue;
            const float thk = (it.kind == 1) ? hp_th : 2.f;
            cfg::esp::LayoutBar(*it.a, used_bar[it.a->side], box_x, box_y, box_w, box_h, thk, it.x, it.y, it.w, it.h);
            used_bar[it.a->side] += ((it.a->side == 1 || it.a->side == 3) ? it.w : it.h) + 3.f;
        }
        for (int i = 0; i < n; ++i)
        {
            auto& it = items[i];
            if (it.kind != 0)
                continue;
            const int slot = cfg::esp::SlotKey(*it.a);
            cfg::esp::FlagPos(*it.a, box_x, box_y, box_w, box_h, it.tw, it.th, it.x, it.y);
            cfg::esp::OffsetByBar(*it.a, used_bar[it.a->side], it.x, it.y);
            cfg::esp::StackFlag(*it.a, used_txt[slot], it.y);
            used_txt[slot] += it.th + 1.f;
            it.w = it.tw;
            it.h = it.th;
        }

        for (int i = 0; i < n; ++i)
        {
            const auto& it = items[i];
            if (it.kind == 1)
            {
                const float hp = std::clamp(player.health / 100.f, 0.f, 1.f);
                const float ha = std::clamp(cfg::esp::healthbar_alpha, 0.f, 1.f);
                if (cfg::esp::healthbar_outline)
                    dl->AddRectFilled(ImVec2(it.x - 1.f, it.y - 1.f), ImVec2(it.x + it.w + 1.f, it.y + it.h + 1.f), IM_COL32(0, 0, 0, (int)(200.f * ha)));
                const auto& hf = cfg::esp::healthbar_full;
                const auto& hl = cfg::esp::healthbar_low;
                const ImU32 hi = IM_COL32(
                    (int)(std::clamp(hf.r, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hf.g, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hf.b, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hf.a * ha, 0.f, 1.f) * 255.f));
                const ImU32 lo = IM_COL32(
                    (int)(std::clamp(hl.r, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hl.g, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hl.b, 0.f, 1.f) * 255.f),
                    (int)(std::clamp(hl.a * ha, 0.f, 1.f) * 255.f));
                if (it.h >= it.w)
                {
                    const float fill_h = it.h * hp;
                    dl->AddRectFilledMultiColor(ImVec2(it.x, it.y + it.h - fill_h), ImVec2(it.x + it.w, it.y + it.h), hi, hi, lo, lo);
                }
                else
                {
                    const float fill_w = it.w * hp;
                    dl->AddRectFilledMultiColor(ImVec2(it.x, it.y), ImVec2(it.x + fill_w, it.y + it.h), lo, hi, hi, lo);
                }
                char hp_txt[8];
                std::snprintf(hp_txt, sizeof(hp_txt), "%d", player.health);
                const ImVec2 ts = EspTextSize(font, hp_txt);
                ImVec2 tp(it.x - ts.x - 2.f, it.y);
                if (it.a->side == 1)
                    tp = ImVec2(it.x + it.w + 2.f, it.y);
                else if (it.a->side == 0)
                    tp = ImVec2(it.x + it.w + 2.f, it.y);
                else if (it.a->side == 2)
                    tp = ImVec2(it.x + it.w + 2.f, it.y);
                if (it.h >= it.w)
                {
                    const float fill_h = it.h * hp;
                    tp.y = it.y + it.h - fill_h - ts.y * 0.5f;
                    if (tp.y < it.y) tp.y = it.y;
                    if (tp.y + ts.y > it.y + it.h) tp.y = it.y + it.h - ts.y;
                }
                EspAddText(dl, font, ImVec2(tp.x + 1.f, tp.y + 1.f), IM_COL32(0, 0, 0, 220), hp_txt);
                EspAddText(dl, font, tp, IM_COL32(255, 255, 255, 255), hp_txt);
            }
            else if (it.kind == 2)
            {
                const float ar = std::clamp(player.armor / 100.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(it.x - 1.f, it.y - 1.f), ImVec2(it.x + it.w + 1.f, it.y + it.h + 1.f), IM_COL32(0, 0, 0, 200));
                if (it.h >= it.w)
                {
                    const float fill_h = it.h * ar;
                    dl->AddRectFilled(ImVec2(it.x, it.y + it.h - fill_h), ImVec2(it.x + it.w, it.y + it.h), it.col);
                }
                else
                {
                    const float fill_w = it.w * ar;
                    dl->AddRectFilled(ImVec2(it.x, it.y), ImVec2(it.x + fill_w, it.y + it.h), it.col);
                }
            }
            else
            {
                ImFont* tf = it.draw_font ? it.draw_font : font;
                const ImU32 outline = IM_COL32(0, 0, 0, 220);
                EspAddText(dl, tf, ImVec2(it.x + 1.f, it.y + 1.f), outline, it.text);
                EspAddText(dl, tf, ImVec2(it.x, it.y), it.col, it.text);
            }
        }
    }

}

void Esp::Render()
{
    Get().RenderImpl();
}

void Esp::RenderImpl()
{
    const auto enemy = EnemyStyle();
    const auto mate_style = MateStyle();
    const bool enemy_on = StyleActive(enemy) || cfg::settings::debug_multipoint;
    const bool mate_on = cfg::esp::mate_enabled && StyleActive(mate_style);
    if (!enemy_on && !mate_on)
        return;

    const auto& frame = FrameStore::Get().Latest();
    const Vec2_t screen = ImGui::GetIO().DisplaySize;
    const auto& matrix = frame.matrix;

    for (int i = 0; i < frame.player_count; ++i)
    {
        const auto& player = frame.players[i];
        if (!player.alive)
            continue;
        if (player.localplayer || player.index == frame.local_index)
            continue;

        const bool mate = frame.local_team != 0 &&
            player.team != 0 &&
            player.team == frame.local_team;
        if (mate)
        {
            if (!frame.local_alive || !cfg::esp::mate_enabled || !mate_on)
                continue;
            DrawBoxESP(player, matrix, screen, mate_style, true);
            continue;
        }

        if (!enemy_on)
            continue;
        DrawBoxESP(player, matrix, screen, enemy, false);
    }
}
