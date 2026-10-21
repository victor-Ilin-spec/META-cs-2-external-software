#pragma once

namespace framework
{
	class c_listbox : public c_base_element
	{
	public:
		c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label = false);

		void draw() override;
		void input() override;
		bool consumes_wheel() const override;
		void set_items(std::vector<std::string> items) { m_items = std::move(items); }
		void scroll_to(int index);
	private:
		int* m_var{};
		std::vector<std::string> m_items{};
		float m_height{};

		bool m_listbox_item_hovered{false};
		float m_scroll_offset{ 0.f };
		float m_scroll_target{ 0.f };
	};
}