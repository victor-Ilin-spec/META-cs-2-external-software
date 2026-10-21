#include "../../../includes.hh"

namespace framework
{
	namespace
	{
		constexpr float kSwatchW = 18.f;
		constexpr float kSwatchH = 10.f;
		constexpr float kPopupW = 180.f;
		constexpr float kPopupH = 220.f;

		math::c_vector_2d SwatchPos(const c_colorpicker* self)
		{
			if (self->m_hide_label)
				return self->m_pos;
			return math::c_vector_2d(self->m_pos.x + self->m_child_size - kSwatchW, self->m_pos.y + 3.f);
		}

		math::c_vector_2d PopupPos(const c_colorpicker* self)
		{
			const auto swatch = SwatchPos(self);
			math::c_vector_2d pos(swatch.x, swatch.y + kSwatchH + 6.f);
			const ImVec2 display = ImGui::GetIO().DisplaySize;
			if (pos.x + kPopupW > display.x - 8.f)
				pos.x = (std::max)(8.f, display.x - kPopupW - 8.f);
			if (pos.y + kPopupH > display.y - 8.f)
				pos.y = (std::max)(8.f, swatch.y - kPopupH - 6.f);
			if (pos.x < 8.f)
				pos.x = 8.f;
			if (pos.y < 8.f)
				pos.y = 8.f;
			return pos;
		}

		hue::c_color HueAt(float hue, int alpha)
		{
			return hue::c_color::from_hsv(hue, 1.f, 1.f, alpha);
		}
	}

	c_colorpicker::c_colorpicker(std::string label, hue::c_color* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, 16.f };
		m_type = element_type::colorpicker;
		m_focus_priority = focus_priority::persistent;
		m_parent_width = 22;
		if (this->m_val)
			this->rgb_to_hsv();
	}

	void c_colorpicker::draw()
	{
		if (!this->m_val)
			return;

		if (!g_ctx->is_focused(this))
			this->rgb_to_hsv();

		animations::m_colorpicker_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_colorpicker_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5);
		animations::m_colorpicker_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		float target_opacity = 0.7f;
		if (animations::m_colorpicker_value.val() > 0.f)
			target_opacity = 0.85f;
		else if (animations::m_colorpicker_hover.val() > 0.f)
			target_opacity = 0.8f;

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];
		smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;
		const float final_opacity = (std::max)(0.55f, animations::m_colorpicker_opacity.val() * smooth_opacity);

		const auto swatch = SwatchPos(this);
		g_render->use_layer(m_layer, [&]()
		{
			if (!m_hide_label)
				g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

			g_render->rect_filled(swatch.x, swatch.y, kSwatchW, kSwatchH, this->m_val->modulate(final_opacity), 2.f);
			g_render->rect(swatch.x, swatch.y, kSwatchW, kSwatchH, g_style->m_outline.modulate(final_opacity), 2.f);
		});
	}

	void c_colorpicker::draw_overlay()
	{
		if (!this->m_val)
			return;
		if (!g_ctx->is_focused(this))
			return;

		const math::c_vector_2d pallete_pos = PopupPos(this);
		const math::c_vector_2d pallete_size = { kPopupW, kPopupH };
		const float open = (std::max)(animations::m_colorpicker_value.val(), 0.85f);
		const int open_a = (int)(255 * open);

		g_render->use_layer(engine::render_layer::prioritized, [&]()
		{
			g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(open), 4.f);
			g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_bars.modulate(open), 4.f);
			g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 25.f, g_style->m_child_top.modulate(open), 4.f, engine::draw_flags_::draw_flags_round_corners_top);
			g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 5, m_label, g_style->m_text.modulate(open));
			animations::m_colorpicker_value.restore();

			const math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
			const math::c_vector_2d hsv_size = { pallete_size.x - 16, 139.f };
			g_render->rect_filled(hsv_pos.x, hsv_pos.y, hsv_size.x, hsv_size.y, HueAt(m_hue, open_a), 2.f);
			g_render->gradient(hsv_pos, hsv_size,
				hue::c_color(255, 255, 255, open_a),
				hue::c_color(255, 255, 255, 0),
				engine::fade_direction::horizontally, 0);
			g_render->gradient(hsv_pos, hsv_size,
				hue::c_color(0, 0, 0, 0),
				hue::c_color(0, 0, 0, open_a),
				engine::fade_direction::vertically, 0);

			const math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
			const math::c_vector_2d hue_size = { hsv_size.x, 10 };
			for (int i = 0; i < 6; ++i)
			{
				const float segment_width = hue_size.x / 6.f;
				g_render->gradient(
					math::c_vector_2d(hue_pos.x + segment_width * i, hue_pos.y),
					math::c_vector_2d(segment_width, hue_size.y),
					HueAt(60.f * i, open_a),
					HueAt(60.f * (i + 1), open_a),
					engine::fade_direction::horizontally, 0);
			}

			const math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
			const math::c_vector_2d alpha_size = { hsv_size.x, 10 };
			for (int i = 0; i < (int)((alpha_size.x + 4) / 5); ++i)
			{
				for (int j = 0; j < 2; ++j)
				{
					const bool is_light = ((i + j) % 2) == 0;
					g_render->rect_filled(alpha_bar.x + i * 5, alpha_bar.y + j * 5, 5, 5,
						hue::c_color(is_light ? 200 : 150, is_light ? 200 : 150, is_light ? 200 : 150).modulate(open));
				}
			}
			g_render->gradient(alpha_bar, alpha_size,
				hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, 0),
				hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, (int)(255 * open)),
				engine::fade_direction::horizontally, 0);

			const int cursor_x = (int)(hsv_pos.x + m_saturation * hsv_size.x);
			const int cursor_y = (int)(hsv_pos.y + (1.f - m_value) * hsv_size.y);
			g_render->rect(cursor_x - 4, cursor_y - 4, 8, 8, hue::c_color(255, 255, 255).modulate(open), 4);
			g_render->rect(cursor_x - 3, cursor_y - 3, 6, 6, hue::c_color(0, 0, 0).modulate(open), 4);

			const int hue_cursor_x = (int)(hue_pos.x + (m_hue / 360.f) * hue_size.x);
			g_render->rect_filled(hue_cursor_x - 2, hue_pos.y - 2, 4, hue_size.y + 4, hue::c_color(255, 255, 255).modulate(open), 2.f);
			g_render->rect(hue_cursor_x - 2, hue_pos.y - 2, 4, hue_size.y + 4, hue::c_color(0, 0, 0).modulate(open), 2.f);

			const int alpha_cursor_x = (int)(alpha_bar.x + (this->m_val->a / 255.f) * alpha_size.x);
			g_render->rect_filled(alpha_cursor_x - 2, alpha_bar.y - 2, 4, alpha_size.y + 4, hue::c_color(255, 255, 255).modulate(open), 2.f);
			g_render->rect(alpha_cursor_x - 2, alpha_bar.y - 2, 4, alpha_size.y + 4, hue::c_color(0, 0, 0).modulate(open), 2.f);

			g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(open), 4.f);
		}, true);
		if (open <= 0.01f)
			return;

		const auto swatch = SwatchPos(this);
		math::c_rect swatch_rect(swatch, math::c_vector_2d(kSwatchW, kSwatchH));
		math::c_rect bounding_data(pallete_pos, pallete_size);
		const bool on_swatch = g_input->mouse_in_region(swatch_rect.pos(), swatch_rect.size());
		const bool on_popup = g_input->mouse_in_region(bounding_data.pos(), bounding_data.size());
		if (!on_swatch && !on_popup && g_input->clicked(input::mouse_buttons::left))
		{
			g_ctx->pop_focus(this);
			g_ctx->m_click_consumed = true;
		}
	}

	void c_colorpicker::input()
	{
		if (!this->m_val)
			return;

		const auto swatch = SwatchPos(this);
		math::c_rect bounding(swatch, math::c_vector_2d(kSwatchW, kSwatchH));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
			g_ctx->m_hovered = nullptr;

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			this->rgb_to_hsv();
			g_ctx->push_focus(this, m_focus_priority);
		}

		if (!g_ctx->is_focused(this))
			return;

		const math::c_vector_2d pallete_pos = PopupPos(this);
		const math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
		const math::c_vector_2d hsv_size = { kPopupW - 16, 139.f };
		const math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
		const math::c_vector_2d hue_size = { hsv_size.x, 10 };
		const math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
		const math::c_vector_2d alpha_size = { hsv_size.x, 10 };

		if (g_input->click_down(input::mouse_buttons::left) && g_input->mouse_in_region(hsv_pos, hsv_size))
		{
			const auto mouse_pos = g_input->get_mouse_position();
			m_saturation = std::clamp((mouse_pos.x - hsv_pos.x) / hsv_size.x, 0.f, 1.f);
			m_value = std::clamp(1.f - (mouse_pos.y - hsv_pos.y) / hsv_size.y, 0.f, 1.f);
			hsv_to_rgb();
		}

		if (g_input->click_down(input::mouse_buttons::left) && g_input->mouse_in_region(hue_pos, hue_size))
		{
			const auto mouse_pos = g_input->get_mouse_position();
			m_hue = std::clamp((mouse_pos.x - hue_pos.x) / hue_size.x, 0.f, 1.f) * 360.f;
			hsv_to_rgb();
		}

		if (g_input->click_down(input::mouse_buttons::left) && g_input->mouse_in_region(alpha_bar, alpha_size))
		{
			const auto mouse_pos = g_input->get_mouse_position();
			this->m_val->a = (int)(std::clamp((mouse_pos.x - alpha_bar.x) / alpha_size.x, 0.f, 1.f) * 255.f);
		}
	}

	void c_colorpicker::rgb_to_hsv()
	{
		if (!this->m_val)
			return;
		const float r = this->m_val->r / 255.f;
		const float g = this->m_val->g / 255.f;
		const float b = this->m_val->b / 255.f;
		const float maxc = (std::max)({ r, g, b });
		const float minc = (std::min)({ r, g, b });
		const float delta = maxc - minc;
		m_value = maxc;
		m_saturation = (maxc > 0.f) ? (delta / maxc) : 0.f;
		if (delta <= 0.f)
			m_hue = 0.f;
		else if (maxc == r)
			m_hue = 60.f * fmodf(((g - b) / delta), 6.f);
		else if (maxc == g)
			m_hue = 60.f * (((b - r) / delta) + 2.f);
		else
			m_hue = 60.f * (((r - g) / delta) + 4.f);
		if (m_hue < 0.f)
			m_hue += 360.f;
	}

	void c_colorpicker::hsv_to_rgb()
	{
		if (!this->m_val)
			return;
		const float c = m_value * m_saturation;
		const float x = c * (1.f - std::abs(fmodf(m_hue / 60.f, 2.f) - 1.f));
		const float m = m_value - c;
		float r, g, b;
		if (m_hue < 60.f) { r = c; g = x; b = 0; }
		else if (m_hue < 120.f) { r = x; g = c; b = 0; }
		else if (m_hue < 180.f) { r = 0; g = c; b = x; }
		else if (m_hue < 240.f) { r = 0; g = x; b = c; }
		else if (m_hue < 300.f) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }
		this->m_val->r = (int)((r + m) * 255.f);
		this->m_val->g = (int)((g + m) * 255.f);
		this->m_val->b = (int)((b + m) * 255.f);
	}
}