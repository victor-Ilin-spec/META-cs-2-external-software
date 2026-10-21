#include "../../../includes.hh"

namespace framework
{
	c_tab::c_tab(math::c_vector_2d pos, math::c_vector_2d size) : m_pos(pos), m_size(size) { }

	void c_tab::paint()
	{
		if (g_ctx->m_tabs.empty())
		{
			// we do not need to run the tabs if there are none
			return;
		}

		int header_height = 35;
		int tab_spacing = 15;

		int total_tab_width = 0;
		for (const auto& tabs : g_ctx->m_tabs)
		{
			int tab_width = g_font->f_default.measure(tabs.m_name).x;
			total_tab_width += tab_width + tab_spacing;
		}

		// remove last spacing
		total_tab_width -= tab_spacing;

		int tab_start_x = this->m_pos.x + (this->m_size.x - 10) - total_tab_width;

		for (int i = 0; i < g_ctx->m_tabs.size(); i++) {
			auto& tab = g_ctx->m_tabs[i];

			math::c_vector_2d text_size = g_font->f_default.measure(tab.m_name);
			math::c_rect bounding = math::c_rect(tab_start_x, this->m_pos.y + 8, text_size.x, text_size.y);

			if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->m_active_tab = i;
				g_ctx->m_cur_tab = tab.m_name;
				g_ctx->m_focus_stack.clear();
				g_ctx->m_hovered = nullptr;
				g_ctx->m_dragging = false;
				g_ctx->m_modal_owner = nullptr;
			}

			animations::m_tab_switching = utils::builder::create_animation_ctx(tab.m_name + utils::builder::get_id(i), (g_ctx->m_active_tab == i) && g_ctx->m_open, 0.5);

			g_font->f_default.text( (float)tab_start_x, this->m_pos.y + 8, tab.m_name, 
				g_style->m_text.modulate(animations::m_window_opacity.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(1.f).val()), animations::m_tab_switching.val()));

			// update tab pos
			tab_start_x += text_size.x + tab_spacing;
		}
	}

	void c_tab::update_input(math::c_vector_2d pos, math::c_vector_2d size)
	{
		this->m_pos = pos;
		this->m_size = size;
	}

	void c_tab::create_tab(std::string name, std::vector<std::string> subtabs)
	{
		g_ctx->m_tabs.push_back({ name, subtabs });
		slog::log::success("[+] a new tab has been created: {} with {} subtabs", name, subtabs.size());
	}
}