#include "../../../includes.hh"

namespace framework
{
	c_keybind::c_keybind(std::string label, key_var_t* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0.f : 16.f) };

		m_type = element_type::keybind;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = 20;
	}
	
	static std::vector<std::string> modes = { "Always", "Hold", "Toggle" };
	void c_keybind::draw()
	{
		animations::m_keybind_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_keybind_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5);
		animations::m_keybind_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_keybind_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_keybind_value.val());
		else if (animations::m_keybind_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_keybind_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_keybind_opacity.val() * smooth_opacity;

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
				{
					// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5, m_label, g_style->m_text.modulate(final_opacity));
					g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 1, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
				else
				{
					// imagine if we have multiinline
					g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
			});

		if (animations::m_keybind_value.val() > 0.f)
		{
			math::c_vector_2d pallete_size = { 180, 105 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			g_render->use_layer(engine::render_layer::prioritized, [&]() {
				// draw the background of the pallete
				g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_keybind_value.val()), 4.f);
				g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_bars.modulate(animations::m_keybind_value.val()), 4.f);
				g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 4.f, engine::draw_flags_::draw_flags_round_corners_top);

				// pallete name
				g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_keybind_value.limit(0.6).val()));
				animations::m_keybind_value.restore();

				// mode selection
				g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				g_render->rect(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				// mode drawing
				{ // store this in {} so we dont affect next vars
					int start_x = pallete_pos.x + 15;
					for (int i = 0; i < modes.size(); i++)
					{
						auto mode = modes[i];

						math::rect_t bounding = math::rect_t(start_x, pallete_pos.y + 35, g_font->f_childs.measure(mode).x, 25.f);
						if (g_input->mouse_in_region(bounding.position(), bounding.size()) && g_input->clicked(input::mouse_buttons::left))
						{
							this->m_val->mode = i;
						}

						animations::m_keybind_selected_mode = utils::builder::create_animation_ctx(m_parent + mode + std::to_string(i) + "#m_keybind_selected_mode", this->m_val->mode == i && g_ctx->is_focused(this), 0.5);
						g_font->f_childs.text(start_x, pallete_pos.y + 38, mode, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_selected_mode.val()));

						start_x += g_font->f_childs.measure(mode).x + 10.f;
					}
				}
				
				// key selection
				// area for binding
				g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				g_render->rect(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + 70, pallete_size.x - 20, 25.f);

				animations::m_keybind_binding = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_binding", this->m_key_callback && g_ctx->is_focused(this), 0.5);

				g_font->f_icons.text(binding_area.x, binding_area.y + 5.f, ICON_FA_KEYBOARD, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				std::string key_name = g_ctx->get_key_name(this->m_val->key);
				g_font->f_childs.text(binding_area.x + g_font->f_icons.measure(ICON_FA_KEYBOARD).x + 8.f, binding_area.y + 4.f, key_name, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_keybind_value.val()), 4.f);
			}, true);


			// check if we click oob
			math::c_rect bounding_data = math::c_rect(pallete_pos, pallete_size);
			if (!g_input->mouse_in_region(bounding_data.pos(), bounding_data.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->pop_focus(this);
				this->m_key_callback = false; // we need to reset this
			}
		}
	}

	void c_keybind::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? g_font->f_icons.measure(ICON_FA_PALLET).x : m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
		{
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
		}

		if (g_ctx->is_focused(this))
		{
			math::c_vector_2d pallete_size = { 180, 105 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + 70, pallete_size.x - 20, 25.f);

			static bool skip_frame = false;
			if (g_input->mouse_in_region(binding_area.pos(), binding_area.size()) && g_input->clicked(input::mouse_buttons::left)) {
				this->m_key_callback = true;
				skip_frame = false;
			}

			if (this->m_key_callback) {
				if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Insert, false)) {
					(this->m_val)->key = 0;
					this->m_key_callback = false;
				}
				else {
					for (auto i = 0; i < 5; i++) {
						if (ImGui::GetIO().MouseClicked[i] && !skip_frame) {
							this->m_val->key = -1;
						}
					}

					// yeah this is bugged
					for (int i = 0; i < 255; i++) {
						if (ImGui::GetIO().MouseClicked[i]) {
							switch (i) {
							case 0:
								this->m_val->key = VK_LBUTTON;
								break;
							case 1:
								this->m_val->key = VK_RBUTTON;
								break;
							case 2:
								this->m_val->key = VK_MBUTTON;
								break;
							case 3:
								this->m_val->key = VK_XBUTTON1;
								break;
							case 4:
								this->m_val->key = VK_XBUTTON2;
								break;
							}
						}


						if (ImGui::IsKeyPressed(ImGuiKey(i), false)) {
							this->m_val->key = i;

							this->m_key_callback = false;
						}
					}
				}
			}

		}
	}
}