#pragma once

// schizo code
namespace framework
{
	enum class element_type :int
	{
		checkbox,
		slider,
		dropdown,
		button,
		colorpicker,
		keybind,
		text_input,
		listbox,
		popup
	};

	// all elements inherit in this shit
	class c_base_element
	{
	public:
		math::c_vector_2d m_pos{}, m_size{};
		std::string m_label{}, m_parent{};

		// focused element
		// pointer as it needs to be but should it be a shared_pointer?
		c_base_element* m_parent_control{};

		// control visibility, handled by parent(child)
		bool m_visible{}, m_inlined{ false }, m_callback_visibility{};
		bool* m_visible_by_callback{ nullptr };
		std::vector<bool*> m_enabled{};

		// focus priority
		focus_priority m_focus_priority{ focus_priority::interactive };

		// parent control width
		float m_parent_width{}; // this should only be accessed from parent and not from current (depends on the need)
		float m_child_size{};

		// control label hiding
		bool m_hide_label{ false };

		// the drawing layer shoud be normal by default but when we are doing popup this needs to be edited
		engine::render_layer m_layer{ engine::render_layer::normal };

		// virtual calls
		virtual void draw() = 0;
		virtual void input() = 0;
		virtual bool consumes_wheel() const { return false; }

		// this will track out the element type, element type that we need to define the element width / etc when its inlined
		element_type m_type{};

		// deconstructor that its kinda useless tbh
		virtual ~c_base_element() = default;
	public: // direct declarations
		void set_parent(std::string parent) {
			this->m_parent = parent;
		}

		// virtual focus_priority m_priority() { return focus_priority::interactive; }

		// element visiblity
		void set_visibility(bool visible)
		{
			this->m_visible = visible;
		}

		// override layering
		void set_layer(engine::render_layer layer)
		{
			this->m_layer = layer;
		}

		void hide_label()
		{
			this->m_hide_label = true;
		}

		void set_callback_visiblity(bool* val)
		{
			// first we enable callback visibility and after we force the callback visibility
			this->m_callback_visibility = true;
			this->m_visible_by_callback = val;
		}
		c_base_element& set_inlined()
		{
			this->m_inlined = true;
			return *this;
		}

		c_base_element& set_enabled(bool* enabled)
		{
			if (enabled)
				m_enabled.push_back(enabled);
			return *this;
		}

		bool enabled() const
		{
			for (bool* flag : m_enabled)
			{
				if (flag && !*flag)
					return false;
			}
			return true;
		}

		void disable_interaction()
		{
			if (g_ctx->is_focused(this))
				g_ctx->pop_focus(this);
			if (g_ctx->m_hovered == this)
				g_ctx->m_hovered = nullptr;
		}
	};
}