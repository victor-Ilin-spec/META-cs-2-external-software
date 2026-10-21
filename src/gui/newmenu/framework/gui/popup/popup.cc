#include "../../../includes.hh"
#include "gui/renderer/window/Window.hpp"
#include "../../../assets/gear_icon.inl"

namespace framework
{
	namespace
	{
		constexpr float kGearSize = 14.f;

		ImTextureID GearTexture()
		{
			static ID3D11ShaderResourceView* srv = nullptr;
			if (srv || !Window::device)
				return (ImTextureID)srv;

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = kGearIconW;
			desc.Height = kGearIconH;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA data{};
			data.pSysMem = kGearIconRgba;
			data.SysMemPitch = kGearIconW * 4;

			ID3D11Texture2D* tex = nullptr;
			if (FAILED(Window::device->CreateTexture2D(&desc, &data, &tex)) || !tex)
				return (ImTextureID)0;

			D3D11_SHADER_RESOURCE_VIEW_DESC view{};
			view.Format = desc.Format;
			view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			view.Texture2D.MipLevels = 1;
			if (FAILED(Window::device->CreateShaderResourceView(tex, &view, &srv)))
				srv = nullptr;
			tex->Release();
			return (ImTextureID)srv;
		}

		math::c_vector_2d GearPos(const c_popup* self)
		{
			if (self->m_hide_label)
				return self->m_pos;
			return math::c_vector_2d(self->m_pos.x + self->m_child_size - kGearSize, self->m_pos.y + 1.f);
		}

		math::c_vector_2d PopupPos(const c_popup* self, float width, float height)
		{
			const auto gear = GearPos(self);
			math::c_vector_2d pos(gear.x + kGearSize - width, gear.y + kGearSize + 6.f);
			const ImVec2 display = ImGui::GetIO().DisplaySize;
			if (pos.x < 8.f)
				pos.x = 8.f;
			if (pos.x + width > display.x - 8.f)
				pos.x = (std::max)(8.f, display.x - width - 8.f);
			if (pos.y + height > display.y - 8.f)
				pos.y = (std::max)(8.f, gear.y - height - 6.f);
			return pos;
		}
	}

	c_popup::c_popup(std::string label, bool hide_label)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, 16.f };
		m_type = element_type::popup;
		m_focus_priority = focus_priority::modal;
		m_parent_width = 20;
		m_height = 92.f;
	}

	void c_popup::draw()
	{
		animations::m_popup_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5);
		animations::m_popup_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_popup_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5);
		animations::m_popup_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_popup_hover", m_visible && g_ctx->m_hovered == this, 0.5);

		float target_opacity = 0.45f;
		if (animations::m_popup_value.val() > 0.f)
			target_opacity = 0.85f;
		else if (animations::m_popup_hover.val() > 0.f)
			target_opacity = 0.7f;

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];
		smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;
		const float final_opacity = (std::max)(0.4f, animations::m_popup_opacity.val() * smooth_opacity) * (enabled() ? 1.f : 0.3f);

		const auto gear = GearPos(this);
		if (const ImTextureID tex = GearTexture())
		{
			g_render->image((int)gear.x, (int)gear.y, (int)kGearSize, (int)kGearSize, tex, hue::c_color(255, 255, 255, (int)(210 * final_opacity)));
			g_render->rect((int)gear.x - 1, (int)gear.y - 1, (int)kGearSize + 2, (int)kGearSize + 2, hue::c_color(230, 230, 230, (int)(90 * final_opacity)), 3.f, 1.f);
		}
	}

	void c_popup::draw_overlay()
	{
		if (!g_ctx->is_focused(this) || !g_ctx->m_open)
			return;

		const float open = (std::max)(animations::m_popup_value.val(), 0.9f);
		const math::c_vector_2d pallete_size = { 168.f, (std::max)(this->m_height, 86.f) };
		const math::c_vector_2d pallete_pos = PopupPos(this, pallete_size.x, pallete_size.y);
		float padding = 0.f;
		c_base_element* parent_control = nullptr;

		g_render->use_layer(engine::render_layer::prioritized, [&]()
		{
			g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(open), 10.f, 8.f);
			g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_bars.modulate(open), 8.f);
			g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 24.f, g_style->m_child_top.modulate(open), 8.f, engine::draw_flags_::draw_flags_round_corners_top);
			g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(open * 0.75f));
			g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(open), 8.f);

			for (auto& control : this->m_controls)
			{
				control->m_pos = pallete_pos + math::c_vector_2d(10.f, 30.f + padding);
				control->set_parent(this->m_label + "#");
				control->set_visibility(true);
				control->m_child_size = pallete_size.x - 20.f;
				control->m_parent_control = parent_control;
				if (g_ctx->m_open)
					control->input();
				control->draw();
				if (!control->m_inlined)
					padding += control->m_size.y + 8.f;
				parent_control = control.get();
			}
			this->m_height = padding + 42.f;
		}, true);

		for (auto& control : this->m_controls)
		{
			if (control->m_type == element_type::colorpicker)
				static_cast<c_colorpicker*>(control.get())->draw_overlay();
			else if (control->m_type == element_type::dropdown)
				static_cast<c_dropdown*>(control.get())->draw_overlay();
		}

		math::c_rect bounding_data(pallete_pos, pallete_size);
		const auto gear = GearPos(this);
		math::c_rect gear_rect(gear, math::c_vector_2d(kGearSize, kGearSize));
		const bool on_popup = g_input->mouse_in_region(bounding_data.pos(), bounding_data.size());
		const bool on_gear = g_input->mouse_in_region(gear_rect.pos(), gear_rect.size());
		bool inner_focused = false;
		bool on_inner_overlay = false;
		for (auto& control : this->m_controls)
		{
			if (g_ctx->is_focused(control.get()))
			{
				inner_focused = true;
				if (control->m_type == element_type::dropdown)
				{
					const auto list_off = control->m_hide_label
						? math::c_vector_2d(0, 0)
						: math::c_vector_2d(0, g_font->f_childs.measure(control->m_label).y + 33);
					const auto list_h = (static_cast<c_dropdown*>(control.get())->item_count()
						* g_font->f_childs.measure(control->m_label).y + 1) + 6.f;
					math::c_rect list_rect(
						control->m_pos.x + list_off.x,
						control->m_pos.y + list_off.y,
						control->m_child_size,
						list_h);
					if (g_input->mouse_in_region(list_rect.pos(), list_rect.size()))
						on_inner_overlay = true;
				}
			}
		}
		if (!g_ctx->m_click_consumed && !on_popup && !on_gear && !inner_focused && !on_inner_overlay && g_input->clicked(input::mouse_buttons::left))
		{
			g_ctx->pop_focus(this);
			g_ctx->m_modal_owner = nullptr;
			g_ctx->m_click_consumed = true;
		}
	}

	void c_popup::input()
	{
		if (!enabled())
		{
			if (g_ctx->is_focused(this))
			{
				g_ctx->pop_focus(this);
				g_ctx->m_modal_owner = nullptr;
			}
			if (g_ctx->m_hovered == this)
				g_ctx->m_hovered = nullptr;
			return;
		}

		const auto gear = GearPos(this);
		math::c_rect bounding(gear, math::c_vector_2d(kGearSize, kGearSize));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
			g_ctx->m_hovered = nullptr;

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			if (g_ctx->is_focused(this))
			{
				g_ctx->pop_focus(this);
				g_ctx->m_modal_owner = nullptr;
			}
			else
			{
				g_ctx->push_focus(this, m_focus_priority);
				g_ctx->m_modal_owner = this;
			}
			g_ctx->m_click_consumed = true;
		}
	}

	std::shared_ptr<c_checkbox> c_popup::add_checkbox(std::string label, bool* val)
	{
		auto control = std::make_shared<framework::c_checkbox>(label, val);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_checkbox, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_float> c_popup::add_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_float>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_slider_float, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_int> c_popup::add_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_int>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_slider_int, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_dropdown> c_popup::add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label)
	{
		auto control = std::make_shared<c_dropdown>(label, val, items, hide_label);
		{
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_dropdown, label: {}]", label);
		}

		// allow chaining
		return control;
	}

	std::shared_ptr<c_multidropdown> c_popup::add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback)
	{
		auto control = std::make_shared<c_multidropdown>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_multidropdown, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_button> c_popup::add_button(std::string label, std::function<void()> callback)
	{
		auto control = std::make_shared<c_button>(label, callback);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_button, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_colorpicker> c_popup::add_colorpicker(std::string label, hue::c_color* val, bool hide_label)
	{
		auto control = std::make_shared<c_colorpicker>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_colorpicker, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_keybind> c_popup::add_keybind(std::string label, key_var_t* val, bool hide_label)
	{
		auto control = std::make_shared<c_keybind>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_keybind, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_text_input> c_popup::add_input_box(std::string label, std::string* val, bool hide_label)
	{
		auto control = std::make_shared<c_text_input>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_text_input, label: {}]", label);
		}
		return control;
	}
}