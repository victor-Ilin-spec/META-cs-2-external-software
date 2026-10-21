#pragma once

namespace framework
{
	class c_button : public c_base_element
	{
	public:
		c_button(std::string label, std::function<void()> callback);

		void draw() override;
		void input() override;
	private:
		std::function<void()> m_callback;

		bool m_callback_called{ false };
	};
}