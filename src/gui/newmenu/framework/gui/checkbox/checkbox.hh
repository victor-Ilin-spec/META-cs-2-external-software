#pragma once

namespace framework
{
	class c_checkbox :public c_base_element
	{
	private:
		bool* m_value{};
	public:
		c_checkbox(std::string label, bool* value);

		void draw() override;
		void input() override;
	};
}