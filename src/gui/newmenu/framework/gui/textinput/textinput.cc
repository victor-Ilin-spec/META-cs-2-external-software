#include "../../../includes.hh"

namespace framework
{
    static constexpr float k_fade_dur = 0.15f;
    static constexpr float k_offset_px = 3.0f;

    static float ease_out_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return 1.f - std::pow(1.f - t, 3.f);
    }

    static std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return toupper(c); });
        return s;
    }

    static float ease_in_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * t;
    }

    c_text_input::c_text_input(std::string label, std::string* var, bool hide_label)
        : m_var(var)
    {
        m_label = std::move(label);
        m_hide_label = hide_label;
        m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 30 };
        m_type = element_type::text_input;
        m_focus_priority = focus_priority::interactive;
        m_parent_width = m_child_size;

        const float now = (float)ImGui::GetTime();
        for (char c : *m_var)
            m_chars.push_back({ c, now - k_fade_dur });
    }

    void c_text_input::rebuild_var()
    {
        m_var->clear();
        for (auto& e : m_chars)
            if (!e.dying) m_var->push_back(e.c);
    }

    void c_text_input::draw()
    {
        animations::m_textinput_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
        animations::m_textinput_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_value", m_visible && g_ctx->top_focus() == this && g_ctx->m_open, 0.5f);
        animations::m_textinput_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

        float target_opacity = 0.2f;
        if (animations::m_textinput_value.val() > 0.f)
            target_opacity = 0.2f + (0.6f * animations::m_textinput_value.val());
        else if (animations::m_textinput_hover.val() > 0.f)
            target_opacity = 0.2f + (0.2f * animations::m_textinput_hover.val());

        static std::unordered_map<std::string, float> smooth_opacity_cache;
        float& smooth_opacity = smooth_opacity_cache[m_parent + m_label + "#smooth_opacity"];
        smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;

        const float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity;
        auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);


        g_render->use_layer(m_layer, [&]()
            {
                if (!m_hide_label)
                    g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

                const auto box_pos = m_pos + position;

                g_render->rect_filled(box_pos.x, box_pos.y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);
                g_render->rect(box_pos.x, box_pos.y, m_child_size, 25.f, g_style->m_outline.modulate(animations::m_window_opacity.val()), 2.f);

                const float now = (float)ImGui::GetTime();
                const float focus_lerp = animations::m_textinput_value.val();

                float x = (box_pos + math::c_vector_2d(6, 5.5f)).x;
                const float base_y = (box_pos + math::c_vector_2d(5, 3.0f)).y;

                static constexpr float k_fade_in_dur = 0.15f;
                static constexpr float k_fade_out_dur = 0.09f;

                for (const auto& entry : m_chars) {
                    float ease, y_off;

                    if (entry.dying) {
                        const float age = now - entry.death;
                        const float t = ease_in_cubic(age / k_fade_out_dur);
                        ease = 1.f - t;
                        y_off = k_offset_px * t;
                    }
                    else {
                        const float age = now - entry.birth;
                        ease = ease_out_cubic(age / k_fade_in_dur);
                        y_off = k_offset_px * (1.f - ease);
                    }

                    const float alpha = final_opacity * ease;
                    const std::string ch(1, entry.c);
                    const auto color = g_style->m_text.modulate(alpha).lerp(g_style->m_accent.modulate(alpha), focus_lerp);

                    g_font->f_childs.text(x, base_y + y_off, ch, color);
                    x += g_font->f_childs.measure(ch).x;
                }

                m_chars.erase(
                    std::remove_if(m_chars.begin(), m_chars.end(), [&](const char_entry& e) {
                        return e.dying && (now - e.death) >= k_fade_out_dur;
                        }),
                    m_chars.end()
                );

                if (g_ctx->top_focus() == this) {
                    const uint64_t now_ms = GetTickCount64();
                    if (now_ms >= (uint64_t)blink)
                        blink = (float)(now_ms + 800);

                    if (now_ms > (uint64_t)(blink - 400)) {
                        const auto cursor_col = g_style->m_text.modulate(final_opacity).lerp(g_style->m_accent.modulate(final_opacity), focus_lerp);
                        g_font->f_childs.text(x, base_y, "|", cursor_col);
                    }
                }
            });

       
    }

    void c_text_input::input()
    {
        if (!m_var)
            return;

        auto position = m_hide_label
            ? math::c_vector_2d(0, 0)
            : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

        math::c_rect bounding = math::c_rect(
            m_pos + math::c_vector_2d(0, position.y),
            math::c_vector_2d(m_child_size, 20.f));

        if (!g_ctx->can_interact(this, m_focus_priority))
            return;

        const bool over_box = g_input->mouse_in_region(bounding.pos(), bounding.size());
        if (over_box)
            g_ctx->m_hovered = this;
        else if (g_ctx->m_hovered == this)
            g_ctx->m_hovered = nullptr;

        if (g_input->clicked(input::mouse_buttons::left))
        {
            if (over_box)
                g_ctx->push_focus(this, m_focus_priority);
            else if (g_ctx->is_focused(this))
                g_ctx->pop_focus(this);
        }

        if (g_ctx->top_focus() != this)
            return;

        const float now = (float)ImGui::GetTime();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::IsKeyPressed(ImGuiKey_Insert) || ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            g_ctx->pop_focus(this);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
        {
            for (int j = (int)m_chars.size() - 1; j >= 0; j--) {
                if (!m_chars[j].dying) {
                    m_chars[j].dying = true;
                    m_chars[j].death = now;
                    rebuild_var();
                    break;
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Space))
        {
            m_chars.push_back({ ' ', now });
            rebuild_var();
        }

        for (int n = 0; n < io.InputQueueCharacters.Size; n++)
        {
            const ImWchar c = io.InputQueueCharacters[n];
            if (c >= 32 && c < 127)
            {
                m_chars.push_back({ static_cast<char>(c), now });
                rebuild_var();
            }
        }
    }
}