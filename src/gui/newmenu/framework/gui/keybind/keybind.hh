#pragma once

namespace framework
{
	enum key_mode_t : int {
		// this is a bit ghetto but we can use this to define the keybind mode
		always = 0,
		hold = 1,
		toggle = 2,
	};

	// TOTAL DOSHIT
	struct key_var_t {
		int key{}, mode{ 1 };

		// yeah i guess we can make this our main input system for keybind
		bool active(bool bound_to_box = false) {
			if ((this->key <= 0 || this->key > 255) && this->mode != key_mode_t::always) {
				return bound_to_box; // invalid key
			}

			if (this->mode == key_mode_t::always) {
				// this is always on
				return true;
			}
			else if (this->mode == key_mode_t::toggle) {
				return GetKeyState(key); // this should return true or false
			}
			else if (this->mode == key_mode_t::hold) {
				return GetAsyncKeyState(key); // this should return true or false
			}
			else {
				return false;
			}
		}
	};

	class c_keybind : public c_base_element
	{
	public:
		c_keybind(std::string label, key_var_t* val, bool hide_label = false);

		void draw() override;
		void input() override;
	private:
		key_var_t* m_val{};

		// this var exists only for keybind objects, no fucking way we make it a global, its just useless
		bool m_key_callback{false}; // set it to false by default
	};
}