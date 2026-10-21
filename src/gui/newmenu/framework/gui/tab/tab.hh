#pragma once

namespace framework
{
	class c_tab
	{
	public:
		c_tab(math::c_vector_2d pos, math::c_vector_2d size);

		void paint();
		void update_input(math::c_vector_2d pos, math::c_vector_2d size);

		void create_tab(std::string name, std::vector<std::string> subtabs);
	private:
		math::c_vector_2d m_pos{}, m_size{};

		std::string m_tab{};
		std::function<void()> m_callback{};
	};
}