#pragma once

namespace framework
{
	class c_menu
	{
	public:
		void initialize();
		void runtime();

	private:
		std::vector<std::shared_ptr<c_window>> m_windows{};
	};
	inline auto g_menu = std::make_unique<c_menu>();
}