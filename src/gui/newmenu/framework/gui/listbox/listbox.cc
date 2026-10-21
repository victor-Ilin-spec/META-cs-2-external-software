#include "../../../includes.hh"

namespace framework
{
	c_listbox::c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label) : m_var(var), m_items(std::move(items)), m_height(height)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = math::c_vector_2d(m_child_size, height);
		m_type = framework::element_type::listbox;
		m_focus_priority = focus_priority::interactive;

		// we wont inline so there is no need for m_parent_width
	}

	void c_listbox::draw()
	{
		animations::m_listbox_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		// animations::m_listbox_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_listbox_value", m_visible &&  && g_ctx->m_open, 0.5);
		animations::m_listbox_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_listbox_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		//if (animations::m_listbox_value.val() > 0.f)
		//	target_opacity = 0.2f + (0.6 * animations::m_listbox_value.val());
		if (animations::m_listbox_hover.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_listbox_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_listbox_opacity.val() * smooth_opacity;
		if (!enabled())
			final_opacity *= 0.35f;

		if (!m_hide_label)
			g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

		const float box_a = animations::m_window_opacity.val() * (enabled() ? 1.f : 0.4f);
		g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, m_size.y, g_style->m_element_base.modulate(box_a), 2.f);
		g_render->rect((m_pos + position).x, (m_pos + position).y, m_child_size, m_size.y, g_style->m_outline.modulate(box_a), 2.f);

		// scrolling logic
		float visible_height = m_size.y - 10;
		float total_height = (g_font->f_childs.measure(m_label).y + 5) * this->m_items.size();
		float max_scroll = std::max(0.f, total_height - visible_height);

		bool listbox_hovered = g_input->mouse_in_region(m_pos + position, math::c_vector_2d(m_child_size, m_size.y));
		float scroll_delta = g_input->get_wheel_value();

		//if (listbox_hovered)
		//{
		//	char buf[256];
		//	sprintf(buf, "hovered | delta: %.2f | target: %.2f | offset: %.2f | max: %.2f",
		//		scroll_delta, m_scroll_target, m_scroll_offset, max_scroll);
		//	g_font->f_childs.text(m_pos.x, m_pos.y - 20, buf, hue::c_color(255, 0, 0, 255));
		//}

		if (enabled() && listbox_hovered && m_visible && g_ctx->m_open && max_scroll > 0.f)
		{
			float scroll_speed = 40.f;
			float scroll_delta = g_input->get_wheel_value();
			m_scroll_target -= scroll_delta * scroll_speed;
			m_scroll_target = std::clamp(m_scroll_target, 0.f, max_scroll);
		}

		float lerp_speed_scroll = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed_scroll;
		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}

		animations::m_listbox_scroll = utils::builder::create_animation_ctx(
			m_parent + m_label + "#m_listbox_scroll", true, 0.25);

		float dt = utils::g_anim_base.delta_time(1.f);

		// lerp toward target
		animations::m_listbox_scroll.m_value += (m_scroll_offset - animations::m_listbox_scroll.m_value) * std::clamp(dt, 0.f, 1.f);
		animations::m_listbox_scroll.animate(animations::m_listbox_scroll.m_value, 0.f, max_scroll, true);

		// read back the clamped value
		float m_scroll_offset = utils::stack[animations::m_listbox_scroll.m_id];

		// draw items
		g_render->push_clip((m_pos + position).x, (m_pos + position).y, m_child_size, m_size.y);

		int start_y = (m_pos + position).y + 5;
		for (int i = 0; i < this->m_items.size(); i++)
		{
			auto item = this->m_items[i];

			// item y
			float item_y = start_y + ((g_font->f_childs.measure(m_label).y + 5) * i) - m_scroll_offset;
			if (item_y + (g_font->f_childs.measure(m_label).y + 5) < (m_pos + position).y ||
				item_y >(m_pos + position).y + m_size.y)
				continue;

			animations::m_listbox_value = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_value", m_visible && *this->m_var == i && g_ctx->m_open, 0.5);

			m_listbox_item_hovered = g_input->mouse_in_region(math::c_vector_2d((m_pos + position).x + 6, item_y), math::c_vector_2d(g_font->f_childs.measure(m_label).x, g_font->f_childs.measure(m_label).y));
			animations::m_listbox_item_hover = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_item_hover", m_visible && this->m_listbox_item_hovered && g_ctx->m_open, 0.5);

			if (enabled() && this->m_listbox_item_hovered && g_input->clicked(input::mouse_buttons::left))
			{
				*this->m_var = i;
			}

			if (*this->m_var == i)
			{
				g_render->rect_filled(
					(m_pos + position).x + 2, item_y - 1,
					m_child_size - 4, g_font->f_childs.measure(m_label).y + 3,
					g_style->m_accent.modulate(animations::m_window_opacity.limit(0.28f).val()), 2.f);
			}

			g_font->f_childs.text((m_pos + position).x + 6 + (6 * animations::m_listbox_item_hover.val()), item_y, item,
				g_style->m_text.modulate(animations::m_window_opacity.limit(enabled() ? 0.3f : 0.12f).val()).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(enabled() ? 1.f : 0.25f).val()), animations::m_listbox_value.val()));
		}

		g_render->restore_clip();
	}

	void c_listbox::input()
	{
		if (!enabled())
		{
			disable_interaction();
			return;
		}

		math::c_rect bounding = math::c_rect(m_pos, m_size);

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (g_ctx->m_hovered == this && g_input->clicked(input::mouse_buttons::left))
		{
			// *this->m_value = !*this->m_value;
		}
	}

	bool c_listbox::consumes_wheel() const
	{
		if (!enabled())
			return false;
		return g_input->mouse_in_region(m_pos, m_size);
	}

	void c_listbox::scroll_to(int index)
	{
		if (!m_var || index < 0 || index >= (int)m_items.size())
			return;
		const float row = g_font->f_childs.measure(m_label).y + 5.f;
		const float visible_height = m_size.y - 10.f;
		const float total_height = row * (float)m_items.size();
		const float max_scroll = (std::max)(0.f, total_height - visible_height);
		float target = row * (float)index - visible_height * 0.35f;
		m_scroll_target = std::clamp(target, 0.f, max_scroll);
	}
}

