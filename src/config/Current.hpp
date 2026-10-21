#pragma once
#include <algorithm>
#include <cmath>
#include <unordered_map>


namespace cfg {
	namespace esp {
		inline bool box = true;
		inline bool corner_box = false;
		inline bool name = false;
		inline color_t name_color{ 1.f, 1.f, 1.f, 1.f };
		inline float box_thickness = 1.5f;
		inline bool skeleton = false;
		inline bool healthbar = false;
		inline float healthbar_thickness = 3.f;
		inline bool healthbar_outline = true;
		inline float healthbar_alpha = 1.f;
		inline color_t healthbar_full{ 40.f / 255.f, 220.f / 255.f, 70.f / 255.f, 1.f };
		inline color_t healthbar_low{ 220.f / 255.f, 40.f / 255.f, 40.f / 255.f, 1.f };
		inline bool armorbar = false;
		inline bool weapon = false;
		inline bool weapon_icon = false;
		inline bool distance = false;
		inline bool flag_c4 = false;
		inline bool flag_scoped = false;
		inline bool flag_flashed = false;
		inline bool flag_defusing = false;
		inline bool flag_health = false;
		inline bool flag_hk = false;
		inline bool flag_reload = false;
		inline bool flag_immune = false;
		inline bool flag_kit = false;

		struct flag_anchor {
			int side = 1;
			float along = 0.5f;
			int order = 0;
		};

		inline int flag_order_seq = 1;

		inline flag_anchor flag_c4_pos{ 1, 0.f, 1 };
		inline flag_anchor flag_scoped_pos{ 1, 0.f, 2 };
		inline flag_anchor flag_flashed_pos{ 1, 0.f, 3 };
		inline flag_anchor flag_defusing_pos{ 1, 0.f, 4 };
		inline flag_anchor flag_health_pos{ 3, 0.5f, 5 };
		inline flag_anchor flag_hk_pos{ 1, 0.f, 6 };
		inline flag_anchor flag_reload_pos{ 1, 0.f, 7 };
		inline flag_anchor flag_immune_pos{ 1, 0.f, 8 };
		inline flag_anchor flag_kit_pos{ 1, 0.f, 9 };
		inline flag_anchor name_pos{ 0, 0.5f, 10 };
		inline float name_off_x = 0.f;
		inline float name_off_y = 0.f;

		inline void NamePos(float bx, float by, float bw, float tw, float th, float& x, float& y)
		{
			x = bx + bw * 0.5f - tw * 0.5f + name_off_x;
			y = by - th - 3.f + name_off_y;
		}
		inline flag_anchor weapon_pos{ 2, 0.5f, 11 };
		inline flag_anchor weapon_icon_pos{ 2, 0.5f, 14 };
		inline flag_anchor distance_pos{ 2, 0.5f, 12 };
		inline flag_anchor armorbar_pos{ 3, 0.5f, 13 };

		inline float SnapAlong(float v)
		{
			if (v < 0.25f) return 0.f;
			if (v > 0.75f) return 1.f;
			return 0.5f;
		}

		inline float SnapAlongEnds(float v)
		{
			return v < 0.5f ? 0.f : 1.f;
		}

		inline void NormalizeAnchor(flag_anchor& a)
		{
			a.side = std::clamp(a.side, 0, 3);
			a.along = SnapAlong(a.along);
		}

		inline void NormalizeFlag(flag_anchor& a)
		{
			a.side = std::clamp(a.side, 0, 3);
			if (a.side == 0 || a.side == 2)
				a.along = 0.5f;
			else
				a.along = 0.f;
		}

		inline void SnapFlag(flag_anchor& a, float px, float py, float bx, float by, float bw, float bh, bool flag_slots = false)
		{
			if (bw < 1.f) bw = 1.f;
			if (bh < 1.f) bh = 1.f;
			if (flag_slots)
			{
				struct Spot { int side; float along; float x; float y; };
				const Spot spots[4] = {
					{ 0, 0.5f, bx + bw * 0.5f, by },
					{ 2, 0.5f, bx + bw * 0.5f, by + bh },
					{ 3, 0.f,  bx,             by },
					{ 1, 0.f,  bx + bw,        by },
				};
				int best = 0;
				float best_d = 1.0e9f;
				for (int i = 0; i < 4; ++i)
				{
					const float dx = px - spots[i].x;
					const float dy = py - spots[i].y;
					const float d = dx * dx + dy * dy;
					if (d < best_d)
					{
						best_d = d;
						best = i;
					}
				}
				a.side = spots[best].side;
				a.along = spots[best].along;
				return;
			}
			const float left = bx;
			const float right = bx + bw;
			const float top = by;
			const float bot = by + bh;
			int side;
			if (px >= right)
				side = 1;
			else if (px <= left)
				side = 3;
			else if (py <= top)
				side = 0;
			else if (py >= bot)
				side = 2;
			else
			{
				const float dl = px - left;
				const float dr = right - px;
				const float dt = py - top;
				const float db = bot - py;
				if (dr <= dl && dr <= dt && dr <= db)
					side = 1;
				else if (dl <= dt && dl <= db)
					side = 3;
				else if (dt <= db)
					side = 0;
				else
					side = 2;
			}
			const float along = (side == 1 || side == 3) ? ((py - top) / bh) : ((px - left) / bw);
			a.side = side;
			a.along = SnapAlong(along);
		}

		inline int SlotKey(const flag_anchor& a)
		{
			const int along = a.along < 0.25f ? 0 : (a.along > 0.75f ? 2 : 1);
			switch (a.side)
			{
			case 0: return along;
			case 1: return 3 + along;
			case 2: return 6 + along;
			default: return 9 + along;
			}
		}

		inline void FlagPos(const flag_anchor& a, float bx, float by, float bw, float bh, float tw, float th, float& x, float& y)
		{
			const float pad = 3.f;
			const float along_y = (a.along < 0.25f) ? by : ((a.along > 0.75f) ? (by + bh - th) : (by + bh * 0.5f - th * 0.5f));
			switch (a.side)
			{
			case 0: x = bx + a.along * bw - tw * 0.5f; y = by - th - pad; break;
			case 1: x = bx + bw + pad; y = along_y; break;
			case 2: x = bx + a.along * bw - tw * 0.5f; y = by + bh + pad; break;
			default: x = bx - tw - pad; y = along_y; break;
			}
		}

		inline void StackFlag(const flag_anchor& a, float used, float& y)
		{
			if (a.side == 0)
				y -= used;
			else
				y += used;
		}

		inline void LayoutBar(const flag_anchor& a, float used, float bx, float by, float bw, float bh, float thickness, float& x, float& y, float& w, float& h)
		{
			const float pad = 4.f;
			if (a.side == 1 || a.side == 3)
			{
				w = thickness;
				h = bh;
				x = (a.side == 1) ? (bx + bw + pad + used) : (bx - thickness - pad - used);
				y = by;
			}
			else
			{
				w = bw;
				h = thickness;
				x = bx;
				y = (a.side == 0) ? (by - thickness - pad - used) : (by + bh + pad + used);
			}
		}

		inline void OffsetByBar(const flag_anchor& a, float bar_used, float& x, float& y)
		{
			switch (a.side)
			{
			case 0: y -= bar_used; break;
			case 1: x += bar_used; break;
			case 2: y += bar_used; break;
			default: x -= bar_used; break;
			}
		}
		inline color_t box_visible{ 1.f, 50.f / 255.f, 50.f / 255.f, 1.f };
		inline color_t box_invisible{ 1.f, 1.f, 1.f, 0.55f };
		inline color_t skeleton_visible{ 1.f, 50.f / 255.f, 50.f / 255.f, 1.f };
		inline color_t skeleton_invisible{ 1.f, 1.f, 1.f, 0.55f };

		inline bool mate_enabled = false;
		inline bool mate_box = true;
		inline bool mate_corner_box = false;
		inline bool mate_name = false;
		inline float mate_box_thickness = 1.5f;
		inline bool mate_skeleton = false;
		inline bool mate_healthbar = false;
		inline bool mate_flag_c4 = false;
		inline bool mate_flag_scoped = false;
		inline bool mate_flag_flashed = false;
		inline bool mate_flag_defusing = false;
		inline color_t mate_box_visible{ 80.f / 255.f, 200.f / 255.f, 1.f, 1.f };
		inline color_t mate_box_invisible{ 80.f / 255.f, 200.f / 255.f, 1.f, 0.55f };
		inline color_t mate_skeleton_visible{ 80.f / 255.f, 200.f / 255.f, 1.f, 1.f };
		inline color_t mate_skeleton_invisible{ 80.f / 255.f, 200.f / 255.f, 1.f, 0.55f };
	}

	namespace movement {
		inline bool bhop = false;
		inline bool strafe_helper = false;
	}

	namespace aimbot {
		inline bool enabled = false;
		inline float fov = 4.f;
		inline float smooth = 4.f;
		inline int bone = 0;
		inline bool draw_fov = false;
		inline bool hb_head = true;
		inline bool hb_chest = false;
		inline bool hb_stomach = false;
		inline bool hb_arms = false;
		inline bool hb_legs = false;
		inline bool hb_nearest = false;
		inline bool multipoints = false;
		inline float multipoint_scale = 0.70f;
		inline int multipoint_count = 5;
		inline bool weapon_override = false;
		inline bool kill_delay = true;
		inline int kill_delay_ms = 200;
	}

	namespace humanize {
		inline bool enabled = false;
		inline float fitts_a = 85.f;
		inline float fitts_b = 190.f;
		inline float target_width = 7.f;
		inline float overshoot_prob = 0.f;
		inline float curvature_scale = 0.006f;
		inline float ou_sigma = 0.25f;
		inline float tremor_amp = 0.08f;
		inline float sdn_k = 0.012f;
		inline float speed_scale = 0.70f;
		inline bool rebuild_on_target_switch = false;
	}

	namespace triggerbot {
		inline bool enabled = false;
		inline int hit_chance = 0;
		inline int reaction_time = 0;
		inline int before_shot_delay = 0;
		inline int between_shots = 20;
		inline bool block_noscope = true;
		inline bool hb_head = true;
		inline bool hb_chest = true;
		inline bool hb_stomach = false;
		inline bool hb_arms = false;
		inline bool hb_legs = false;
	}

	namespace autofire {
		inline bool enabled = false;
		inline bool early_stop = false;
		inline bool autoscope = false;
		inline bool block_noscope = true;
		inline int reaction_time = 100;
		inline int before_shot_delay = 0;
		inline int between_shots = 45;
		inline int hit_chance = 70;
	}

	namespace rcs {
		inline bool enabled = false;
		inline float smooth = 3.2f;
		inline float strength_y = 0.82f;
		inline float strength_x = 0.55f;
		inline int reaction_ms = 110;
	}

	namespace settings {
		inline bool watermark = true;
		inline bool streamproof = false;
		inline bool vsync = false;
		inline bool free_cpu = true;
		inline bool debug_multipoint = false;
		inline bool auto_accept = false;
		inline int cache_ms = 2;
		inline color_t menu_color{ 15.f / 255.f, 15.f / 255.f, 15.f / 255.f, 1.f };
		inline float menu_bg_opacity = 1.f;
	}

	namespace radar {
		inline bool enabled = false;
		inline bool show_teammates = false;
		inline int type = 0;
		inline float size = 200.f;
		inline float proportion = 2680.f;
		inline float range = 250.f;
		inline float opacity = 0.67f;
		inline float pos_x = 20.f;
		inline float pos_y = 80.f;
		inline color_t enemy{ 237.f / 255.f, 85.f / 255.f, 106.f / 255.f, 200.f / 255.f };
		inline color_t teammate{ 80.f / 255.f, 200.f / 255.f, 1.f, 200.f / 255.f };
	}

	namespace world {
		inline bool items = false;
		inline bool item_name = true;
		inline bool item_icon = true;
		inline bool item_ammo = true;
		inline float item_distance = 40.f;
		inline bool item_rifles = true;
		inline bool item_smgs = true;
		inline bool item_shotguns = true;
		inline bool item_snipers = true;
		inline bool item_pistols = true;
		inline bool item_heavy = true;
		inline bool item_grenades = true;
		inline bool item_utility = true;

		inline bool projectiles = false;
		inline bool proj_name = true;
		inline bool proj_icon = true;
		inline bool proj_timer = true;
		inline bool inferno_bounds = true;

		inline bool grenade_pred = false;
		inline bool grenade_local_only = true;
		inline color_t grenade_color{ 170.f / 255.f, 175.f / 255.f, 220.f / 255.f, 200.f / 255.f };
	}


	// Not stored, just for testing
	namespace dev {
		inline bool console = true;
		inline int open_menu_key = 0;
		inline int cache_refresh_rate = 5;
	}
}
#include "WeaponCatalog.hpp"
