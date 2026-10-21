#pragma once

namespace framework
{
	class c_dropdown : public c_base_element {
	public:
		c_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label = false);

		void draw() override;
		void draw_overlay();
		void input() override;
		int item_count() const { return (int)m_items.size(); }
	private:
		int* m_val{};
		std::vector<std::string> m_items{};
	};
}