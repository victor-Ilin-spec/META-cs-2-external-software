#pragma once

namespace framework
{
	class c_slider_float : public c_base_element {
	public:
		c_slider_float(std::string label, float* val, float min, float max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
		bool consumes_wheel() const override;
	private:
		float* m_val{};
		float m_min{}, m_max{};
		bool m_ctrl_edit = false;
		std::string m_edit;

		std::wstring m_prefix{};
	};

	class c_slider_int : public c_base_element {
	public:
		c_slider_int(std::string label, int* val, int min, int max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
		bool consumes_wheel() const override;
	private:
		int* m_val{};
		int m_min{}, m_max{};
		bool m_ctrl_edit = false;
		std::string m_edit;

		std::wstring m_prefix{};
	};
}