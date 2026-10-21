#include "../../../includes.hh"
#include "gui/renderer/window/Window.hpp"
#include "../../../assets/arrow_right_icon.inl"


namespace framework
{
	namespace
	{
		constexpr float kArrowSize = 16.f;

		ImTextureID ArrowTexture()
		{
			static ID3D11ShaderResourceView* srv = nullptr;
			if (srv || !Window::device)
				return (ImTextureID)srv;

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = kArrowIconW;
			desc.Height = kArrowIconH;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA data{};
			data.pSysMem = kArrowIconRgba;
			data.SysMemPitch = kArrowIconW * 4;

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

		math::c_vector_2d TitleArrowPos(const math::c_vector_2d& pos, const math::c_vector_2d& size)
		{
			return math::c_vector_2d(pos.x + size.x - kArrowSize - 8.f, pos.y + 7.f);
		}
	}

	c_child::c_child(std::string name, child_width width, float y) : m_name(std::move(name)), m_width_type(width), m_size(math::c_vector_2d(0, y)), m_titlebar(true) {}
	void c_child::set_title_icon(const unsigned char* rgba, int w, int h)
	{
		m_title_icon_rgba = rgba;
		m_title_icon_w = w;
		m_title_icon_h = h;
		m_title_icon_srv = nullptr;
	}

	namespace
	{
		ImTextureID MakeRgbaTexture(const unsigned char* rgba, int w, int h, void** cache)
		{
			if (!rgba || w <= 0 || h <= 0 || !cache)
				return (ImTextureID)0;
			if (*cache)
				return (ImTextureID)*cache;
			if (!Window::device)
				return (ImTextureID)0;

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = static_cast<UINT>(w);
			desc.Height = static_cast<UINT>(h);
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA data{};
			data.pSysMem = rgba;
			data.SysMemPitch = static_cast<UINT>(w * 4);

			ID3D11Texture2D* tex = nullptr;
			if (FAILED(Window::device->CreateTexture2D(&desc, &data, &tex)) || !tex)
				return (ImTextureID)0;

			ID3D11ShaderResourceView* srv = nullptr;
			D3D11_SHADER_RESOURCE_VIEW_DESC view{};
			view.Format = desc.Format;
			view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			view.Texture2D.MipLevels = 1;
			if (FAILED(Window::device->CreateShaderResourceView(tex, &view, &srv)))
				srv = nullptr;
			tex->Release();
			*cache = srv;
			return (ImTextureID)srv;
		}
	}

	void c_child::enable_collapse(bool start_open)
	{
		m_collapsible = true;
		m_open = start_open;
		m_open_h = (std::max)(32.f, m_size.y);
		m_title_bar_click = true;
		m_title_action = [this] { m_open = !m_open; };
		m_title_action_open = [this] { return m_open; };
	}

	float c_child::layout_height()
	{
		if (!m_collapsible)
			return m_size.y;
		if (m_open_h < 32.f)
			m_open_h = (std::max)(32.f, m_size.y);
		auto anim = utils::builder::create_animation_ctx("child_collapse#" + m_name, m_open, 0.22f);
		const float t = utils::stack[anim.m_id];
		const float e = t * t * (3.f - 2.f * t);
		m_size.y = 32.f + (m_open_h - 32.f) * e;
		return m_size.y;
	}

	void c_child::draw()
	{
		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, hue::c_color(5, 5, 5).modulate(animations::m_window_opacity.val()), 30.f, 6.f);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_bars.modulate(animations::m_window_opacity.val()), 6);

		if (this->m_titlebar)
		{
			g_render->rect_filled(this->m_pos.x + 1, this->m_pos.y + 1, this->m_size.x - 2, 30, g_style->m_child_top.modulate(animations::m_window_opacity.val()), 6, engine::draw_flags_::draw_flags_round_corners_top);
			float title_x = this->m_pos.x + 10.f;
			if (m_title_icon_rgba)
			{
				const ImTextureID tex = MakeRgbaTexture(m_title_icon_rgba, m_title_icon_w, m_title_icon_h, &m_title_icon_srv);
				if (tex)
				{
					const float ih = 16.f;
					const float iw = 16.f;
					const float iy = this->m_pos.y + (30.f - ih) * 0.5f;
					g_render->image((int)(this->m_pos.x + 8.f), (int)iy, (int)iw, (int)ih, tex,
						hue::c_color(255, 255, 255).modulate(animations::m_window_opacity.val()), 0.f);
					title_x = this->m_pos.x + 8.f + iw + 6.f;
				}
			}
			g_font->f_default.text(title_x, this->m_pos.y + 7, this->m_name, g_style->m_text.modulate(animations::m_window_opacity.limit(0.6).val()));
			if (m_title_action)
			{
				const bool open = m_title_action_open && m_title_action_open();
				const auto ap = TitleArrowPos(this->m_pos, this->m_size);
				if (const ImTextureID tex = ArrowTexture())
				{
					const auto col = g_style->m_text.modulate(animations::m_window_opacity.limit(0.85).val());
					if (auto* dl = g_render->draw_list())
					{
						const ImVec2 a(ap.x, ap.y);
						const ImVec2 b(ap.x + kArrowSize, ap.y + kArrowSize);
						ImVec2 uv0(0.f, 0.f);
						ImVec2 uv1(1.f, 1.f);
						if (m_collapsible)
						{
							uv0 = open ? ImVec2(0.f, 1.f) : ImVec2(0.f, 0.f);
							uv1 = open ? ImVec2(1.f, 0.f) : ImVec2(1.f, 1.f);
						}
						else if (open)
						{
							uv0 = ImVec2(1.f, 0.f);
							uv1 = ImVec2(0.f, 1.f);
						}
						dl->AddImage(tex, a, b, uv0, uv1, col.transform());
					}
				}
			}
		}

		// element handling
		float padding = 0.f;
		c_base_element* m_parent_control = nullptr;

		math::c_vector_2d content_start = this->m_pos + this->calculate_element_padding();
		float content_area_height = this->m_size.y - this->calculate_element_padding().y;

		for (auto& control : m_controls)
		{
			// if control is inlined we use parent's control data to init this one
			if (control->m_inlined && m_parent_control != nullptr) {
				// we have to check if the element is a slider since it needs abit more presuring on it
				if (control->m_type == element_type::slider)
				{
					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					// check if parent is slider
					if (m_parent_control->m_type == element_type::slider)
					{
						m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					}

					control->m_child_size = (base_scalling * 0.5) - 7.5;

					// hide label if the parent control is a checkbox
					if (m_parent_control->m_type == element_type::checkbox && !control->m_hide_label)
					{
						// force label hide
						control->hide_label();
					}

					// check if parent control is checkbox so we align it when we have no label
					if (m_parent_control->m_type == element_type::checkbox && control->m_hide_label)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 4);
					}
					else {
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
					}
				}
				else if (control->m_type == element_type::button)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::button)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					control->m_child_size = (base_scalling * 0.5) - 7.5;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::text_input)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::text_input)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					int base_scalling = (this->m_size.x - 20.f);

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5) - 7.5;
					control->m_child_size = (base_scalling * 0.5) - 7.5;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::colorpicker)
				{
					control->hide_label();
					control->m_size.y = 16.f;
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_child_size - 20.f, 3.f);
					}
					else if (m_parent_control->m_type == element_type::popup)
					{
						control->m_pos = math::c_vector_2d(this->m_pos.x + this->m_size.x - 26.f, m_parent_control->m_pos.y + 1.f);
					}
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(23.f, 0);
					}
				}
				else if (control->m_type == element_type::keybind)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 8.f, 0);
					}
				}
				else if (control->m_type == element_type::popup)
				{
					control->hide_label();
					control->m_size.y = 16.f;
					if (m_parent_control->m_type == element_type::checkbox)
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(25.f + g_font->f_childs.measure(m_parent_control->m_label).x + 6.f, 1.f);
					else if (m_parent_control->m_type == element_type::colorpicker)
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(20.f, 0);
				}
				else if (control->m_type == element_type::checkbox && m_parent_control->m_type == element_type::listbox)
				{
					const float title_w = g_font->f_childs.measure(m_parent_control->m_label).x;
					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(title_w + 18.f, 0.f);
				}
				else {
					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
				}
			}
			else if (control->m_type == element_type::popup && control->m_hide_label)
			{
				control->m_pos = math::c_vector_2d(this->m_pos.x + this->m_size.x - 26.f, this->m_pos.y + 8.f);
			}
			else {
				control->m_pos = this->m_pos + math::c_vector_2d(0, padding - m_scroll_offset) + this->calculate_element_padding();
			}

			if (!this->visible())
			{
				if (control->m_type == element_type::colorpicker)
					g_ctx->pop_focus(control.get());
				continue;
			}

			if (control->m_callback_visibility && control->m_visible_by_callback && !(*control->m_visible_by_callback)) {
				if (control->m_type == element_type::colorpicker)
					g_ctx->pop_focus(control.get());
				continue;
			}

			float control_top = control->m_pos.y;
			float control_bottom = control->m_pos.y + control->m_size.y;
			float visible_top = content_start.y;
			float visible_bottom = this->m_pos.y + this->m_size.y - 12.f;

			bool is_visible = !(control_bottom < visible_top || control_top > visible_bottom);
			if (control->m_type == element_type::popup && control->m_hide_label && !control->m_inlined)
				is_visible = this->visible();

			// c_base_control::base
			// set the child parent, we are going to use this in the checkbox data ( if there are problems, make sure to include, tab subtab )
			control->set_parent(this->m_name + "#" + this->m_child_attach_data.m_subtab_name);

			// set base control visibility based on where we are
			control->set_visibility(this->visible() && is_visible);

			// c_base_control->element
			// no point in inputting if we have no menu opened
			if (g_ctx->m_open && is_visible)
			{
				// if a modal is open, only allow the popup that owns it to run input
				// everything else behind it gets blocked
				if (g_ctx->m_modal_owner != nullptr && control.get() != g_ctx->m_modal_owner)
				{
					// skip input for background elements
				}
				else if (g_ctx->can_interact(control.get(), control->m_focus_priority))
				{
					control->input();
				}
			}

			// engine::c_layout_engine(this->m_pos + this->calculate_element_padding(), this->calculate_safe_area())
			const bool skip_clip = control->m_type == element_type::popup || control->m_type == element_type::colorpicker || control->m_type == element_type::dropdown;
			if (!skip_clip)
				g_render->push_clip((this->m_pos + this->calculate_element_padding()).x, (this->m_pos + this->calculate_element_padding()).y, this->calculate_safe_area().x, this->calculate_safe_area().y);

			if (is_visible)
			{
				control->draw();
			}

			if (!skip_clip)
				g_render->restore_clip();

			// push y only if the control is not inlined
			if (!control->m_inlined && !(control->m_type == element_type::popup && control->m_hide_label))
				padding += control->m_size.y + 8.f;

			// we only set this if we do inlining
			control->m_parent_control = m_parent_control;
			m_parent_control = control.get();

			// set child data to control base
			control->m_child_size = this->m_size.x - 25.f;

			// g_ctx->m_focus_took = control.get();

			// check which control is focused
			if (g_ctx->m_focus_took != nullptr)
				slog::log::debug("[framework::c_child] control focused: {}", g_ctx->m_focus_took->m_label);

			animations::m_window_opacity.restore();
		}

		m_content_height = padding;
		m_max_scroll = std::max(0.f, m_content_height - content_area_height);

		// reset it
		animations::m_window_opacity.restore();
		if (m_paint)
			m_paint(this);
	}

	void c_child::draw_overlays()
	{
		if (!this->visible() || !g_ctx->m_open)
			return;

		for (auto& control : m_controls)
		{
			if (!control)
				continue;
			if (control->m_type == element_type::popup)
				static_cast<c_popup*>(control.get())->draw_overlay();
			else if (control->m_type == element_type::colorpicker)
				static_cast<c_colorpicker*>(control.get())->draw_overlay();
			else if (control->m_type == element_type::dropdown)
				static_cast<c_dropdown*>(control.get())->draw_overlay();
		}
	}

	void c_child::input()
	{
		if (m_title_action && this->visible() && g_ctx->m_open)
		{
			const auto ap = TitleArrowPos(this->m_pos, this->m_size);
			const math::c_vector_2d hit(kArrowSize + 4.f, kArrowSize + 4.f);
			const bool on_arrow = g_input->mouse_in_region(ap - math::c_vector_2d(2.f, 2.f), hit);
			const bool on_bar = m_title_bar_click &&
				g_input->mouse_in_region(this->m_pos, math::c_vector_2d(this->m_size.x, 30.f));
			if ((on_arrow || on_bar) &&
				g_input->clicked(input::mouse_buttons::left) &&
				!g_ctx->m_click_consumed)
			{
				m_title_action();
				g_ctx->m_click_consumed = true;
			}
		}

		bool is_hovered = g_input->mouse_in_region(this->m_pos, this->m_size);
		const bool wheel_taken = g_ctx->m_hovered && g_ctx->m_hovered->consumes_wheel();
		if (is_hovered && this->visible() && m_max_scroll > 0.f && !collapsed() && !wheel_taken)
		{
			float scroll_speed = 40.f;
			float scroll_delta = g_input->get_wheel_value();

			m_scroll_target -= scroll_delta * scroll_speed;
			m_scroll_target = std::clamp(m_scroll_target, 0.f, m_max_scroll);
		}

		float lerp_speed = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed;

		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}
	}

	bool c_child::title_action_hovered() const
	{
		if (!m_title_action)
			return false;
		if (m_title_bar_click &&
			g_input->mouse_in_region(this->m_pos, math::c_vector_2d(this->m_size.x, 30.f)))
			return true;
		const auto ap = TitleArrowPos(this->m_pos, this->m_size);
		return g_input->mouse_in_region(ap - math::c_vector_2d(2.f, 2.f), math::c_vector_2d(kArrowSize + 4.f, kArrowSize + 4.f));
	}

	bool c_child::owns_modal() const
	{
		auto* modal = g_ctx->m_modal_owner;
		if (!modal)
			return false;
		for (const auto& control : m_controls)
		{
			if (!control)
				continue;
			if (control.get() == modal || modal->m_parent_control == control.get())
				return true;
		}
		return false;
	}


	void c_child::attach_child(std::string tab_name, std::string subtab_name)
	{
		this->m_child_attach_data.m_tab_name = tab_name;
		this->m_child_attach_data.m_subtab_name = subtab_name;
	}

	void c_child::set_visible(bool data)
	{
		this->m_visible = data;
	}

	bool c_child::visible()
	{
		if (g_ctx->m_cur_tab != this->m_child_attach_data.m_tab_name)
		{
			return false;
		}

		if (m_overlay && m_extra_visible && !*m_extra_visible)
			return false;

		bool same_tab = g_ctx->m_cur_tab == this->m_child_attach_data.m_tab_name;
		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_subtab.empty())
		{
			return true;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab != this->m_child_attach_data.m_subtab_name)
		{
			return false;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab == this->m_child_attach_data.m_subtab_name)
		{
			return true;
		}

		// if none of the conditions matched return false
		return false;
	}

	child_width c_child::get_type()
	{
		return this->m_width_type;
	}

	math::c_vector_2d c_child::calculate_element_padding()
	{
		if (this->m_titlebar)
		{
			return math::c_vector_2d(12.f, 42.f);
		}
		else {
			return math::c_vector_2d(10.f, 12.f);
		}
	}

	math::c_vector_2d c_child::calculate_safe_area()
	{
		if (this->m_titlebar)
		{
			return this->m_size - math::c_vector_2d(18.f, 50.f);
		}
		else {
			return this->m_size - math::c_vector_2d(20.f, 24.f);
		}
	}

	std::shared_ptr<framework::c_checkbox> c_child::add_checkbox(std::string label, bool* val)
	{
		auto control = std::make_shared<framework::c_checkbox>(label, val);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_child] created a new control: [type: c_checkbox, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_float> c_child::add_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_float>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_child] created a new control: [type: c_slider_float, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_int> c_child::add_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_int>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_child] created a new control: [type: c_slider_int, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_dropdown> c_child::add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label)
	{
		auto control = std::make_shared<c_dropdown>(label, val, items, hide_label);
		{
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_child] created a new control: [type: c_dropdown, label: {}]", label);
		}

		// allow chaining
		return control;
	}

	std::shared_ptr<c_multidropdown> c_child::add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback)
	{
		auto control = std::make_shared<c_multidropdown>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			slog::log::success("[framework::c_child] created a new control: [type: c_multidropdown, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_button> c_child::add_button(std::string label, std::function<void()> callback)
	{
		auto control = std::make_shared<c_button>(label, callback);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_button, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_colorpicker> c_child::add_colorpicker(std::string label, hue::c_color* val, bool hide_label)
	{
		auto control = std::make_shared<c_colorpicker>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_colorpicker, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_keybind> c_child::add_keybind(std::string label, key_var_t* val, bool hide_label)
	{
		auto control = std::make_shared<c_keybind>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_keybind, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_text_input> c_child::add_input_box(std::string label, std::string* val, bool hide_label)
	{
		auto control = std::make_shared<c_text_input>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_text_input, label: {}]", label);
		}
		return control;
	}

	std::shared_ptr<c_popup> c_child::add_popup(std::string label, bool hide_label, std::function<void(c_popup* ptr)> callback)
	{
		auto control = std::make_shared<c_popup>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_popup, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_listbox> c_child::add_listbox(std::string label, int* val, std::vector<std::string> items, float height, bool hide_label)
	{
		auto control = std::make_shared<c_listbox>(label, val, items, height, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_listbox, label: {}]", label);
		}

		return control;
	}
}