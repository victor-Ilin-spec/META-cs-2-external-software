#include "../../../includes.hh"

namespace framework
{
	c_button::c_button(std::string label, std::function<void()> callback) : m_callback(std::move(callback))
	{
		m_label = std::move(label);
		m_size = math::c_vector_2d(0, 25);

		m_type = element_type::button;
		m_focus_priority = focus_priority::interactive;

		// this element width (parent width named as it will be only accessed from parent)
		m_parent_width = m_child_size;
	}

	void c_button::draw()
	{
		animations::m_button_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_button_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_value", m_visible && this->m_callback_called && g_ctx->m_open, 0.25);
		animations::m_button_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		// animation handler
		if (animations::m_button_value.val() > 0.99f)
		{
			// we have to reset this after the animation has been finished
			// this is dogshit code i know
			this->m_callback_called = false;
		}

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_button_value.val() > 0.f)
			target_opacity = 0.2f + (0.6 * animations::m_button_value.val());
		else if (animations::m_button_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_button_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity;

		g_render->use_layer(m_layer, [&]()
			{
				g_render->rect_filled(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);
				g_render->rect(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_outline.modulate(animations::m_window_opacity.val()), 2.f);

				// label
				g_font->f_childs.text(m_pos.x + (m_child_size * 0.5) - (g_font->f_childs.measure(this->m_label).x * 0.5), m_pos.y + 3, m_label, g_style->m_text.modulate(final_opacity).lerp(g_style->m_accent.modulate(final_opacity), animations::m_button_value.val()));
			});
	}

	void c_button::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		// we do not do return if the focus is nullptr as we want to access this
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
		{
			// execute it
			this->m_callback();

			// note: this will track if the callback was called, ussualy used for animation
			m_callback_called = true;
		}
	}
}