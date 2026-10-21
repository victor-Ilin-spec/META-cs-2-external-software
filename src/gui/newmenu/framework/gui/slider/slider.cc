#include "../../../includes.hh"

namespace framework
{
	namespace
	{
		bool CtrlDown()
		{
			return ImGui::GetIO().KeyCtrl;
		}

		math::c_rect ValueRect(const math::c_vector_2d& pos, float child_size, const std::string& info)
		{
			const auto ts = g_font->f_childs.measure(info.empty() ? "0.0" : info);
			const float w = (std::max)(ts.x, 28.f);
			return math::c_rect(
				math::c_vector_2d(pos.x + child_size - w - 2.f, pos.y - 1.f),
				math::c_vector_2d(w + 6.f, ts.y + 4.f));
		}

		bool ParseFloatBuf(const std::string& s, float& out)
		{
			if (s.empty() || s == "-" || s == "." || s == "-.")
				return false;
			try {
				size_t n = 0;
				out = std::stof(s, &n);
				return n > 0;
			} catch (...) {
				return false;
			}
		}

		bool ParseIntBuf(const std::string& s, int& out)
		{
			if (s.empty() || s == "-")
				return false;
			try {
				size_t n = 0;
				out = std::stoi(s, &n);
				return n > 0;
			} catch (...) {
				return false;
			}
		}

		void TypeChar(std::string& buf, char c, bool allow_dot)
		{
			if (c >= '0' && c <= '9')
			{
				buf.push_back(c);
				return;
			}
			if (c == '-' && buf.empty())
			{
				buf.push_back(c);
				return;
			}
			if (allow_dot && c == '.' && buf.find('.') == std::string::npos)
				buf.push_back(c);
		}
	}

	c_slider_float::c_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix) : m_val(val), m_min(min), m_max(max), m_prefix(prefix)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 15 };
		m_type = element_type::slider;
		m_focus_priority = focus_priority::persistent;
		m_parent_width = m_child_size;
	}

	bool c_slider_float::consumes_wheel() const
	{
		if (!enabled() || m_hide_label || !m_val)
			return false;
		const std::string shown = m_ctrl_edit ? m_edit : (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));
		auto value_rect = ValueRect(this->m_pos, this->m_child_size, shown);
		return g_input->mouse_in_region(value_rect.pos(), value_rect.size());
	}


	void c_slider_float::input()
	{
		if (!enabled())
		{
			disable_interaction();
			m_ctrl_edit = false;
			m_edit.clear();
			return;
		}

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_child_size, (std::max)(m_size.y, position.y + 10.f)));
		const std::string shown = m_ctrl_edit ? m_edit : (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));
		auto value_rect = ValueRect(this->m_pos, this->m_child_size, shown);
		const bool over_value = !m_hide_label && g_input->mouse_in_region(value_rect.pos(), value_rect.size());

		if (!g_ctx->can_interact(this, m_focus_priority) && !m_ctrl_edit)
			return;
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) || over_value)
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
			g_ctx->m_hovered = nullptr;

		if (m_ctrl_edit)
		{
			if (g_input->clicked(input::mouse_buttons::left) && !over_value)
			{
				float parsed = 0.f;
				if (ParseFloatBuf(m_edit, parsed))
					*this->m_val = std::clamp(parsed, this->m_min, this->m_max);
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
			{
				float parsed = 0.f;
				if (ParseFloatBuf(m_edit, parsed))
					*this->m_val = std::clamp(parsed, this->m_min, this->m_max);
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_edit.empty())
				m_edit.pop_back();

			auto& q = ImGui::GetIO().InputQueueCharacters;
			for (int n = 0; n < q.Size; n++)
			{
				const ImWchar c = q[n];
				if (c < 127)
					TypeChar(m_edit, static_cast<char>(c), true);
			}
			return;
		}

		if (CtrlDown() && over_value && g_input->clicked(input::mouse_buttons::left))
		{
			m_ctrl_edit = true;
			m_edit = utils::builder::precision(*this->m_val, 1);
			g_ctx->push_focus(this, focus_priority::interactive);
			g_ctx->m_click_consumed = true;
			return;
		}

		const float wheel = g_input->get_wheel_value();
		if (over_value && wheel != 0.f && !g_input->click_down(input::mouse_buttons::left))
			*this->m_val = std::clamp(*this->m_val + (wheel > 0.f ? 0.1f : -0.1f), this->m_min, this->m_max);

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
			g_ctx->push_focus(this, m_focus_priority);

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, 0, m_child_size);
				float target_value = utils::builder::modulate_float(offset, 0, m_child_size, this->m_min, this->m_max);
				*this->m_val = *this->m_val + (target_value - *this->m_val) * 0.2f;
			}
		}

		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);
	}




	void c_slider_float::draw()
	{
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_value", m_visible && (*this->m_val > this->m_min + 0.1) && g_ctx->m_open, 0.5);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;
		if (!enabled())
			final_opacity *= 0.35f;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		auto position2 = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 7);

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 10.f, g_style->m_element_base.modulate(animations::m_window_opacity.val() * (enabled() ? 1.f : 0.45f)), 2.f);
				g_render->rect((m_pos + position).x, (m_pos + position).y, m_child_size, 10.f, g_style->m_outline.modulate(animations::m_window_opacity.val() * (enabled() ? 1.f : 0.35f)), 2.f);

				float width = utils::builder::modulate_float(*this->m_val, this->m_min, this->m_max, 0, m_child_size);
				if (*this->m_val > this->m_min + 0.1)
				{
					auto schizo = m_pos + math::c_vector_2d(1, position2.y);
					const float fill_a = animations::m_window_opacity.val() * (enabled() ? 1.f : 0.25f);

					g_render->rect_filled(schizo.x, schizo.y, width, 8.f, g_style->m_accent.modulate(fill_a), 2.f);
					g_render->fade_rect_filled(schizo.x, schizo.y, width, 8.f, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, 50 * fill_a), engine::fade_direction::vertically, 2.f);
				}

				std::string info = m_ctrl_edit
					? (m_edit + "_")
					: (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));

				if (!m_hide_label)
				{
					const float value_a = m_ctrl_edit ? 1.f : (enabled() ? 0.3f : 0.12f);
					g_font->f_childs.text((m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).x, (m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).y,
						info, g_style->m_text.modulate(animations::m_window_opacity.limit(value_a).val()));
				}
				animations::m_window_opacity.restore();
			});
	}

	c_slider_int::c_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix) : m_val(val), m_min(min), m_max(max), m_prefix(prefix)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 15 };
		m_type = element_type::slider;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}

	bool c_slider_int::consumes_wheel() const
	{
		if (!enabled() || m_hide_label || !m_val)
			return false;
		const std::string shown = m_ctrl_edit ? m_edit : (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));
		auto value_rect = ValueRect(this->m_pos, this->m_child_size, shown);
		return g_input->mouse_in_region(value_rect.pos(), value_rect.size());
	}

	void c_slider_int::input()
	{
		if (!enabled())
		{
			disable_interaction();
			m_ctrl_edit = false;
			m_edit.clear();
			return;
		}

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_child_size, (std::max)(m_size.y, position.y + 10.f)));
		const std::string shown = m_ctrl_edit ? m_edit : (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));
		auto value_rect = ValueRect(this->m_pos, this->m_child_size, shown);
		const bool over_value = !m_hide_label && g_input->mouse_in_region(value_rect.pos(), value_rect.size());

		if (!g_ctx->can_interact(this, m_focus_priority) && !m_ctrl_edit)
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) || over_value)
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
			g_ctx->m_hovered = nullptr;

		if (m_ctrl_edit)
		{
			if (g_input->clicked(input::mouse_buttons::left) && !over_value)
			{
				int parsed = 0;
				if (ParseIntBuf(m_edit, parsed))
					*this->m_val = std::clamp(parsed, this->m_min, this->m_max);
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
			{
				int parsed = 0;
				if (ParseIntBuf(m_edit, parsed))
					*this->m_val = std::clamp(parsed, this->m_min, this->m_max);
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				m_ctrl_edit = false;
				m_edit.clear();
				g_ctx->pop_focus(this);
				return;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_edit.empty())
				m_edit.pop_back();

			auto& q = ImGui::GetIO().InputQueueCharacters;
			for (int n = 0; n < q.Size; n++)
			{
				const ImWchar c = q[n];
				if (c < 127)
					TypeChar(m_edit, static_cast<char>(c), false);
			}
			return;
		}

		if (CtrlDown() && over_value && g_input->clicked(input::mouse_buttons::left))
		{
			m_ctrl_edit = true;
			m_edit = std::to_string(*this->m_val);
			g_ctx->push_focus(this, focus_priority::interactive);
			g_ctx->m_click_consumed = true;
			return;
		}

		const float wheel = g_input->get_wheel_value();
		if (over_value && wheel != 0.f && !g_input->click_down(input::mouse_buttons::left))
		{
			const int span = this->m_max - this->m_min;
			const int step = (std::max)(1, span / 50);
			*this->m_val = std::clamp(*this->m_val + static_cast<int>(wheel > 0.f ? step : -step), this->m_min, this->m_max);
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
			g_ctx->push_focus(this, m_focus_priority);

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, 0, m_child_size);
				float target_value = utils::builder::modulate_float(offset, 0, m_child_size, (float)this->m_min, (float)this->m_max);
				static float current = (float)(*this->m_val);
				current += (target_value - current) * 0.2f;
				*this->m_val = (int)(current + 0.5f);
				if (std::abs(current - target_value) < 0.5f)
					*this->m_val = (int)(target_value + 0.5f);
				*this->m_val = *this->m_val + (target_value - *this->m_val) * 0.2f;
			}
		}

		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);
	}

	void c_slider_int::draw()
	{
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_value", m_visible && (*this->m_val > this->m_min) && g_ctx->m_open, 0.5);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;
		if (!enabled())
			final_opacity *= 0.35f;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		auto position2 = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 7);

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 10.f, g_style->m_element_base.modulate(animations::m_window_opacity.val() * (enabled() ? 1.f : 0.45f)), 2.f);
				g_render->rect((m_pos + position).x, (m_pos + position).y, m_child_size, 10.f, g_style->m_outline.modulate(animations::m_window_opacity.val() * (enabled() ? 1.f : 0.35f)), 2.f);

				float width = utils::builder::modulate_float(*this->m_val, this->m_min, this->m_max, 0, m_child_size);
				if (*this->m_val > this->m_min + 1)
				{
					auto schizo = m_pos + math::c_vector_2d(1, position2.y);
					const float fill_a = animations::m_window_opacity.val() * (enabled() ? 1.f : 0.25f);

					g_render->rect_filled(schizo.x, schizo.y, width, 8.f, g_style->m_accent.modulate(fill_a), 2.f);
					g_render->fade_rect_filled(schizo.x, schizo.y, width, 8.f, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, 50 * fill_a), engine::fade_direction::vertically, 2.f);
				}

				std::string info = m_ctrl_edit
					? (m_edit + "_")
					: (utils::builder::precision(*this->m_val, 1) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str()));

				if (!m_hide_label)
				{
					const float value_a = m_ctrl_edit ? 1.f : (enabled() ? 0.3f : 0.12f);
					g_font->f_childs.text((m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).x, (m_pos + math::c_vector_2d(m_child_size - g_font->f_childs.measure(info).x, 0.5)).y,
						info, g_style->m_text.modulate(animations::m_window_opacity.limit(value_a).val()));
				}
				animations::m_window_opacity.restore();
			});
	}
}