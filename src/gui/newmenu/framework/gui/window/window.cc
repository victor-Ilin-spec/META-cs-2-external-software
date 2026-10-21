#include "../../../includes.hh"

namespace framework
{
	c_window::c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size) : m_title(title), m_pos(pos), m_size(size)
	{ 
		slog::log::success("[+] c_window:: {} created at pos: {}, {}, with size: {}, {}", m_title, m_pos.x, m_pos.y, m_size.x, m_size.y);
	}

	void c_window::paint()
	{
		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_shadow, 30.f, 6.f);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_background, 6.f);

		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, 35, g_style->m_window_bars.modulate(animations::m_window_opacity.val()), 6.f, engine::draw_flags_::draw_flags_round_corners_top);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y + this->m_size.y - 35, this->m_size.x, 35, g_style->m_window_bars.modulate(animations::m_window_opacity.val()), 6.f, engine::draw_flags_::draw_flags_round_corners_bottom);

		g_font->f_default.text(this->m_pos.x + 10, this->m_pos.y + 8, this->m_title, g_style->m_text.modulate(animations::m_window_opacity.limit(0.6).val()));

		const std::string version = "v1.1";
		g_font->f_default.text(
			this->m_pos.x + this->m_size.x - 10 - g_font->f_default.measure(version).x,
			this->m_pos.y + this->m_size.y - (35 - 8),
			version,
			g_style->m_accent.modulate(animations::m_window_opacity.limit(0.8).val()));

		animations::m_window_opacity.restore();

		// paint tabs
		this->m_obj_tab->paint();

		math::c_vector_2d layout_system = {};
		const float content_top = 50.f;
		const float content_bot = 35.f;
		const float view_h = this->m_size.y - content_top - content_bot;
		const float col_w = (this->m_size.x - this->child_padding().x - 24.f) * 0.5f;
		const float gap = 15.f;
		float left_y = 0.f;
		float right_y = 0.f;

		g_render->push_clip(this->m_pos.x, this->m_pos.y + content_top, this->m_size.x, view_h);

		for (auto& child : this->m_childrens)
		{
			if (child->is_overlay())
				continue;

			if (child->get_type() == child_width::half)
				child->m_size.x = col_w;
			else
				child->m_size.x = this->m_size.x - this->child_padding().x;

			if (!child->visible())
			{
				child->m_relative_pos = {};
				child->m_pos = this->m_pos + this->subtab_padding();
				continue;
			}

			const float h = child->layout_height();
			if (child->get_type() == child_width::full)
			{
				const float y = (std::max)(left_y, right_y);
				child->m_relative_pos = math::c_vector_2d(0.f, y - m_scroll);
				left_y = y + h + gap;
				right_y = left_y;
			}
			else if (left_y <= right_y)
			{
				child->m_relative_pos = math::c_vector_2d(0.f, left_y - m_scroll);
				left_y += h + gap;
			}
			else
			{
				child->m_relative_pos = math::c_vector_2d(col_w + gap, right_y - m_scroll);
				right_y += h + gap;
			}

			child->m_pos = child->m_relative_pos + (this->m_pos + this->subtab_padding());
			child->input();
			child->draw();
		}

		g_render->restore_clip();
		m_content_h = (std::max)(left_y, right_y);
	}

	void c_window::paint_overlays()
	{
		for (auto& child : this->m_childrens)
		{
			if (child->is_overlay() && child->visible())
			{
				child->input();
				child->draw();
			}
			child->draw_overlays();
		}
	}

	void c_window::input()
	{
		if (GetAsyncKeyState(VK_INSERT) & 1)
		{
			g_ctx->m_open = !g_ctx->m_open;
		}

		animations::m_window_opacity = utils::builder::create_animation_ctx(this->m_title, g_ctx->m_open, 0.5f);

		if (!g_ctx->m_open)
			return;

		static math::c_vector_2d prev_mouse_pos{}, delta{};

		const bool picker_open = g_ctx->top_focus() &&
			(g_ctx->top_focus()->m_type == element_type::colorpicker
				|| g_ctx->top_focus()->m_type == element_type::popup);

		bool overlay_hover = false;
		for (auto& child : this->m_childrens)
		{
			if (child->is_overlay() && child->visible() &&
				g_input->mouse_in_region(child->pos(), child->size()))
			{
				overlay_hover = true;
				break;
			}
		}

		math::c_rect bounding = math::c_rect(this->m_pos.x, this->m_pos.y, this->m_size.x, 35);
		delta = prev_mouse_pos - g_input->get_mouse_position();

		if (picker_open || overlay_hover)
		{
			g_ctx->m_dragging = false;
		}
		else if (!g_ctx->m_dragging
			&& g_input->clicked(input::mouse_buttons::left)
			&& g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_dragging = true;
		}
		else if (g_ctx->m_dragging && g_input->click_down(input::mouse_buttons::left))
		{
			this->m_pos -= delta;
		}
		else
		{
			g_ctx->m_dragging = false;
		}

		const float content_top = 50.f;
		const float content_bot = 35.f;
		const float view_h = this->m_size.y - content_top - content_bot;
		const float max_scroll = (std::max)(0.f, m_content_h - view_h);
		bool child_wants_wheel = g_ctx->m_hovered && g_ctx->m_hovered->consumes_wheel();
		if (!child_wants_wheel)
		{
			for (auto& child : this->m_childrens)
			{
				if (!child->visible() || child->is_overlay() || !child->can_scroll())
					continue;
				if (g_input->mouse_in_region(child->pos(), child->size()))
				{
					child_wants_wheel = true;
					break;
				}
			}
		}
		const bool in_body = g_input->mouse_in_region(
			math::c_vector_2d(this->m_pos.x, this->m_pos.y + content_top),
			math::c_vector_2d(this->m_size.x, view_h));
		if (in_body && !child_wants_wheel && !g_ctx->m_dragging)
		{
			m_scroll_target -= g_input->get_wheel_value() * 40.f;
			m_scroll_target = std::clamp(m_scroll_target, 0.f, max_scroll);
		}
		else
		{
			m_scroll_target = std::clamp(m_scroll_target, 0.f, max_scroll);
		}
		m_scroll += (m_scroll_target - m_scroll) * 0.25f;
		if (std::abs(m_scroll_target - m_scroll) < 0.5f)
			m_scroll = m_scroll_target;

		prev_mouse_pos = g_input->get_mouse_position();
		this->m_obj_tab->update_input(this->m_pos, this->m_size);
	}

	std::shared_ptr<c_child> c_window::build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback)
	{
		auto child = std::make_shared<c_child>(name, width, y);
		{
			if (!child)
			{
				slog::log::error("failed to create child: {}", name);
				return nullptr;
			}

			// attach the pos to the window's, remains to get updated in window's input but we must have atleat framed to it the moment of creation
			child->m_pos = child->m_relative_pos + this->m_pos + subtab_padding();

			// we do not call draw and input here as this only gets called once, well we just do the child caching
			this->m_childrens.push_back(child);

			callback(child.get());

			slog::log::success("[framework::c_window] created child: {}", name);
		}
		return child;
	}

	std::shared_ptr<c_tab> c_window::prebuild_tabs(std::function<void(c_tab* ptr)> callback)
	{
		auto obj = std::make_shared<c_tab>(this->m_pos, this->m_size);
		{
			slog::log::success("[+] prebuilded tab pointer");

			// pass the pointer
			callback(obj.get());

			// pass the pointer
			this->m_obj_tab = obj;
		}
		return obj;
	}

	void c_window::finish_tab_prebuild()
	{
		// we surely attached it now we have to update it so we wont have problems with childs, assuming the menu just initialized we dont have the problem that user interacted with it yet
		// we can safely asume that the first tab and subtab are active as menu just initialized, this way the data we pass when we attach child to tab will be valid
		g_ctx->m_cur_tab = g_ctx->m_tabs[0].m_name;

		if (g_ctx->m_tabs[0].m_subtab.empty())
		{
			g_ctx->m_tabs[0].m_cur_subtab = "";
		}
	}

	math::c_vector_2d c_window::subtab_padding()
	{
		return math::c_vector_2d(15, 50);
	}

	math::c_vector_2d c_window::child_padding()
	{
		return math::c_vector_2d(20, 55);
	}
}