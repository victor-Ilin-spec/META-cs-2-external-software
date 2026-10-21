#include "../includes.hh"

#include "config/Config.hpp"
#include "core/engine/cache/FrameStore.hpp"
#include "gui/frontend/overlays/Overlays.hpp"
#include "gui/renderer/window/Window.hpp"
#include "gui/renderer/Renderer.hpp"
#include "gui/newmenu/assets/child_title_icons.inl"
#include "assets/fonts/WeaponIcons.h"

#define half_height 181
#define full_height 377

namespace {
	hue::c_color FromCfg(const color_t& c)
	{
		return hue::c_color(
			static_cast<int>(c.r * 255.f),
			static_cast<int>(c.g * 255.f),
			static_cast<int>(c.b * 255.f),
			static_cast<int>(c.a * 255.f));
	}

	color_t ToCfg(const hue::c_color& c)
	{
		return color_t(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
	}

	hue::c_color g_menu_color = FromCfg(cfg::settings::menu_color);
	hue::c_color g_radar_enemy = FromCfg(cfg::radar::enemy);
	hue::c_color g_radar_teammate = FromCfg(cfg::radar::teammate);
	hue::c_color g_box_visible = FromCfg(cfg::esp::box_visible);
	hue::c_color g_box_invisible = FromCfg(cfg::esp::box_invisible);
	hue::c_color g_skel_visible = FromCfg(cfg::esp::skeleton_visible);
	hue::c_color g_skel_invisible = FromCfg(cfg::esp::skeleton_invisible);
	hue::c_color g_mate_box_visible = FromCfg(cfg::esp::mate_box_visible);
	hue::c_color g_mate_box_invisible = FromCfg(cfg::esp::mate_box_invisible);
	hue::c_color g_mate_skel_visible = FromCfg(cfg::esp::mate_skeleton_visible);
	hue::c_color g_mate_skel_invisible = FromCfg(cfg::esp::mate_skeleton_invisible);
	hue::c_color g_hp_full = FromCfg(cfg::esp::healthbar_full);
	hue::c_color g_hp_low = FromCfg(cfg::esp::healthbar_low);
	hue::c_color g_name_color = FromCfg(cfg::esp::name_color);
	bool g_mate_visuals_open = false;
	std::shared_ptr<framework::c_child> g_visuals_child;
	std::shared_ptr<framework::c_child> g_mate_visuals_child;
	bool g_humanize_open = false;
	std::shared_ptr<framework::c_child> g_humanize_header;
	std::shared_ptr<framework::c_child> g_humanize_panel;

	int g_cfg_selected = 0;
	std::string g_cfg_name = "default";
	std::string g_cfg_status;
	std::shared_ptr<framework::c_listbox> g_cfg_list;

	int g_weapon_selected = 0;
	bool g_weapon_override = false;
	float g_weapon_spacer = 0.f;
	short g_last_held_weapon = -1;
	bool g_scope_weapon = false;
	std::shared_ptr<framework::c_listbox> g_weapon_list;

	std::vector<std::string> BuildWeaponLabels(short held)
	{
		std::vector<std::string> labels;
		labels.reserve(cfg::aimbot::kWeaponCount);
		for (const auto& entry : cfg::aimbot::kWeapons)
		{
			std::string line = entry.name;
			if (entry.id == held)
				line += "  *";
			labels.push_back(std::move(line));
		}
		return labels;
	}

	void SyncWeaponWidgets()
	{
		if (g_weapon_selected < 0 || g_weapon_selected >= cfg::aimbot::kWeaponCount)
			g_weapon_selected = 0;
		const short tune_id = cfg::aimbot::kWeapons[g_weapon_selected].id;
		const auto& tune = cfg::aimbot::Tune(tune_id);
		if (cfg::aimbot::weapon_override && tune.override)
		{
			cfg::aimbot::fov = tune.fov;
			cfg::aimbot::smooth = tune.smooth;
			cfg::aimbot::hb_nearest = tune.hb_nearest;
			cfg::aimbot::hb_head = tune.hb_head;
			cfg::aimbot::hb_chest = tune.hb_chest;
			cfg::aimbot::hb_stomach = tune.hb_stomach;
			cfg::aimbot::hb_arms = tune.hb_arms;
			cfg::aimbot::hb_legs = tune.hb_legs;

			cfg::triggerbot::hit_chance = tune.trg_hit_chance;
			cfg::triggerbot::reaction_time = tune.trg_reaction_time;
			cfg::triggerbot::before_shot_delay = tune.trg_before_shot;
			cfg::triggerbot::between_shots = tune.trg_between_shots;
			cfg::triggerbot::block_noscope = tune.trg_block_noscope;
			cfg::triggerbot::hb_head = tune.trg_hb_head;
			cfg::triggerbot::hb_chest = tune.trg_hb_chest;
			cfg::triggerbot::hb_stomach = tune.trg_hb_stomach;
			cfg::triggerbot::hb_arms = tune.trg_hb_arms;
			cfg::triggerbot::hb_legs = tune.trg_hb_legs;

			cfg::autofire::hit_chance = tune.af_hit_chance;
			cfg::autofire::reaction_time = tune.af_reaction_time;
			cfg::autofire::before_shot_delay = tune.af_before_shot;
			cfg::autofire::between_shots = tune.af_between_shots;
			cfg::autofire::block_noscope = tune.af_block_noscope;
		}
		cfg::autofire::autoscope = tune.af_autoscope && cfg::aimbot::IsScopeWeapon(tune_id);
		if (!cfg::aimbot::IsScopeWeapon(tune_id))
			cfg::autofire::block_noscope = false;
	}

	void PullWeaponWidgets()
	{
		short id = -1;
		const short held = FrameStore::Get().Latest().local_weapon;
		if (cfg::aimbot::IndexOf(held) >= 0)
			id = held;
		else if (g_weapon_selected >= 0 && g_weapon_selected < cfg::aimbot::kWeaponCount)
			id = cfg::aimbot::kWeapons[g_weapon_selected].id;
		if (id < 0)
			return;

		auto& tune = cfg::aimbot::Tune(id);
		if (cfg::aimbot::IsScopeWeapon(id))
		{
			tune.af_autoscope = cfg::autofire::autoscope;
			tune.af_block_noscope = cfg::autofire::block_noscope;
		}

		if (!cfg::aimbot::weapon_override)
			return;

		tune.override = true;
		tune.fov = cfg::aimbot::fov;
		tune.smooth = cfg::aimbot::smooth;
		tune.hb_nearest = cfg::aimbot::hb_nearest;
		tune.hb_head = cfg::aimbot::hb_head;
		tune.hb_chest = cfg::aimbot::hb_chest;
		tune.hb_stomach = cfg::aimbot::hb_stomach;
		tune.hb_arms = cfg::aimbot::hb_arms;
		tune.hb_legs = cfg::aimbot::hb_legs;

		tune.trg_hit_chance = cfg::triggerbot::hit_chance;
		tune.trg_reaction_time = cfg::triggerbot::reaction_time;
		tune.trg_before_shot = cfg::triggerbot::before_shot_delay;
		tune.trg_between_shots = cfg::triggerbot::between_shots;
		tune.trg_block_noscope = cfg::triggerbot::block_noscope;
		tune.trg_hb_head = cfg::triggerbot::hb_head;
		tune.trg_hb_chest = cfg::triggerbot::hb_chest;
		tune.trg_hb_stomach = cfg::triggerbot::hb_stomach;
		tune.trg_hb_arms = cfg::triggerbot::hb_arms;
		tune.trg_hb_legs = cfg::triggerbot::hb_legs;

		tune.af_hit_chance = cfg::autofire::hit_chance;
		tune.af_reaction_time = cfg::autofire::reaction_time;
		tune.af_before_shot = cfg::autofire::before_shot_delay;
		tune.af_between_shots = cfg::autofire::between_shots;
		if (cfg::aimbot::IsScopeWeapon(id))
			tune.af_block_noscope = cfg::autofire::block_noscope;
	}

	void RefreshCfgList()
	{
		try {
			auto names = Config::List();
			if (g_cfg_list)
				g_cfg_list->set_items(names);
			if (names.empty() || g_cfg_selected < 0 || g_cfg_selected >= static_cast<int>(names.size()))
				g_cfg_selected = 0;
		}
		catch (const std::exception& e) {
			LOGF(FATAL, "RefreshCfgList: {}", e.what());
		}
	}

	void PushColorsToWidgets()
	{
		g_menu_color = FromCfg(cfg::settings::menu_color);
		g_box_visible = FromCfg(cfg::esp::box_visible);
		g_box_invisible = FromCfg(cfg::esp::box_invisible);
		g_skel_visible = FromCfg(cfg::esp::skeleton_visible);
		g_skel_invisible = FromCfg(cfg::esp::skeleton_invisible);
		g_mate_box_visible = FromCfg(cfg::esp::mate_box_visible);
		g_mate_box_invisible = FromCfg(cfg::esp::mate_box_invisible);
		g_mate_skel_visible = FromCfg(cfg::esp::mate_skeleton_visible);
		g_mate_skel_invisible = FromCfg(cfg::esp::mate_skeleton_invisible);
		g_hp_full = FromCfg(cfg::esp::healthbar_full);
		g_hp_low = FromCfg(cfg::esp::healthbar_low);
		g_name_color = FromCfg(cfg::esp::name_color);
		g_radar_enemy = FromCfg(cfg::radar::enemy);
		g_radar_teammate = FromCfg(cfg::radar::teammate);
	}

	void PullColorsFromWidgets()
	{
		cfg::settings::menu_color = ToCfg(g_menu_color);
		cfg::esp::box_visible = ToCfg(g_box_visible);
		cfg::esp::box_invisible = ToCfg(g_box_invisible);
		cfg::esp::skeleton_visible = ToCfg(g_skel_visible);
		cfg::esp::skeleton_invisible = ToCfg(g_skel_invisible);
		cfg::esp::mate_box_visible = ToCfg(g_mate_box_visible);
		cfg::esp::mate_box_invisible = ToCfg(g_mate_box_invisible);
		cfg::esp::mate_skeleton_visible = ToCfg(g_mate_skel_visible);
		cfg::esp::mate_skeleton_invisible = ToCfg(g_mate_skel_invisible);
		cfg::esp::healthbar_full = ToCfg(g_hp_full);
		cfg::esp::healthbar_low = ToCfg(g_hp_low);
		cfg::esp::name_color = ToCfg(g_name_color);
		cfg::radar::enemy = ToCfg(g_radar_enemy);
		cfg::radar::teammate = ToCfg(g_radar_teammate);
	}

	void ApplyMenuStyle()
	{
		const auto& c = cfg::settings::menu_color;
		const float op = std::clamp(cfg::settings::menu_bg_opacity, 0.f, 1.f);
		const int r = std::clamp((int)(c.r * 255.f), 0, 255);
		const int g = std::clamp((int)(c.g * 255.f), 0, 255);
		const int b = std::clamp((int)(c.b * 255.f), 0, 255);
		const int a = std::clamp((int)(op * 255.f), 0, 255);

		framework::g_style->m_accent = hue::c_color(r, g, b, 255);
		framework::g_style->m_window_background = hue::c_color(15, 15, 15, a);
		framework::g_style->m_window_shadow = hue::c_color(15, 15, 15, a);
		framework::g_style->m_window_bars = hue::c_color(20, 20, 20, 255);
		framework::g_style->m_child_top = hue::c_color(25, 25, 25, 255);
		framework::g_style->m_element_base = hue::c_color(15, 15, 15, 255);
		framework::g_style->m_outline = hue::c_color(36, 36, 36, 255);
		framework::g_style->m_text = hue::c_color(255, 255, 255, 255);
	}
}

namespace framework
{
	void c_menu::initialize()
	{
		slog::log::info("[>] started menu initialization");

		auto window = std::make_shared<c_window>("META", math::c_vector_2d(200, 200), math::c_vector_2d(650, 480));
		if (!window)
		{
			slog::log::error("[-] failed to create window");
			return;
		}

		window->prebuild_tabs([](framework::c_tab* controller) {
			controller->create_tab("AimBot", {});
			controller->create_tab("Visual", {});
			controller->create_tab("Movement", {});
			controller->create_tab("Radar", {});
			controller->create_tab("Misc", {});
			controller->create_tab("Config", {});
		});
		window->finish_tab_prebuild();

		window->build_child("Aimbot", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("AimBot", "");
			controller->enable_collapse(false);
			controller->set_title_icon(kChildIcon_aimbot, kChildTitleIconSize, kChildTitleIconSize);
			controller->add_checkbox("Enable", &cfg::aimbot::enabled);
			controller->add_popup("Hitboxes", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Nearest", &cfg::aimbot::hb_nearest);
				popup->add_checkbox("Head", &cfg::aimbot::hb_head);
				popup->add_checkbox("Chest", &cfg::aimbot::hb_chest);
				popup->add_checkbox("Stomach", &cfg::aimbot::hb_stomach);
				popup->add_checkbox("Arms", &cfg::aimbot::hb_arms);
				popup->add_checkbox("Legs", &cfg::aimbot::hb_legs);
			})->set_inlined().set_enabled(&cfg::aimbot::enabled);
			controller->add_slider_float("FOV", &cfg::aimbot::fov, 0.5f, 20.f)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_slider_float("Smooth (ticks)", &cfg::aimbot::smooth, 1.f, 12.f)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_checkbox("Multipoints", &cfg::aimbot::multipoints)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_popup("Multipoints", true, [](framework::c_popup* popup) {
				popup->add_slider_float("Scale", &cfg::aimbot::multipoint_scale, 0.f, 1.f);
				popup->add_slider_int("Count", &cfg::aimbot::multipoint_count, 0, 12);
			})->set_inlined().set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::aimbot::multipoints);
			controller->add_checkbox("Draw FOV", &cfg::aimbot::draw_fov)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_checkbox("Kill Delay", &cfg::aimbot::kill_delay)
				->set_enabled(&cfg::aimbot::enabled);
			{
				auto kill_ms = controller->add_slider_int("Kill Delay MS", &cfg::aimbot::kill_delay_ms, 0, 1000);
				kill_ms->set_enabled(&cfg::aimbot::enabled);
				kill_ms->set_enabled(&cfg::aimbot::kill_delay);
			}
			g_weapon_list = controller->add_listbox("Weapons", &g_weapon_selected, BuildWeaponLabels(-1), 150.f, false);
			g_weapon_list->set_enabled(&cfg::aimbot::enabled);
			controller->add_checkbox("Automatic selection", &cfg::aimbot::weapon_override)
				->set_inlined().set_enabled(&cfg::aimbot::enabled);
		});


		window->build_child("Triggerbot", framework::child_width::half, half_height, [](framework::c_child* controller) {
			controller->attach_child("AimBot", "");
			controller->enable_collapse(false);
			controller->set_title_icon(kChildIcon_trigger, kChildTitleIconSize, kChildTitleIconSize);
			controller->add_checkbox("Enable", &cfg::triggerbot::enabled);
			controller->add_popup("Hitboxes", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Head", &cfg::triggerbot::hb_head);
				popup->add_checkbox("Chest", &cfg::triggerbot::hb_chest);
				popup->add_checkbox("Stomach", &cfg::triggerbot::hb_stomach);
				popup->add_checkbox("Arms", &cfg::triggerbot::hb_arms);
				popup->add_checkbox("Legs", &cfg::triggerbot::hb_legs);
			})->set_inlined().set_enabled(&cfg::triggerbot::enabled);
			controller->add_slider_int("Hit chance", &cfg::triggerbot::hit_chance, 0, 100)
				->set_enabled(&cfg::triggerbot::enabled);
			controller->add_slider_int("Reaction time", &cfg::triggerbot::reaction_time, 0, 1000)
				->set_enabled(&cfg::triggerbot::enabled);
			controller->add_slider_int("Before shot", &cfg::triggerbot::before_shot_delay, 0, 100)
				->set_enabled(&cfg::triggerbot::enabled);
			controller->add_slider_int("Between shots", &cfg::triggerbot::between_shots, 0, 500)
				->set_enabled(&cfg::triggerbot::enabled);
			controller->add_checkbox("Block noscope", &cfg::triggerbot::block_noscope)
				->set_enabled(&cfg::triggerbot::enabled);
		});





		window->build_child("Semi Rage", framework::child_width::half, half_height, [](framework::c_child* controller) {
			controller->attach_child("AimBot", "");
			controller->enable_collapse(false);
			controller->set_title_icon(kChildIcon_semirage, kChildTitleIconSize, kChildTitleIconSize);
			controller->add_checkbox("AutoFire", &cfg::autofire::enabled)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_checkbox("Early Stop", &cfg::autofire::early_stop)
				->set_inlined().set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled);
			controller->add_checkbox("Autoscope", &cfg::autofire::autoscope)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled)
				.set_enabled(&g_scope_weapon);
			controller->add_slider_int("Reaction Time", &cfg::autofire::reaction_time, 0, 300)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled);
			controller->add_slider_int("Before shot", &cfg::autofire::before_shot_delay, 0, 500)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled);
			controller->add_slider_int("Between Shots", &cfg::autofire::between_shots, 0, 1000)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled);
			controller->add_slider_int("Hit Chance", &cfg::autofire::hit_chance, 0, 100)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled);
			controller->add_checkbox("Block Noscope", &cfg::autofire::block_noscope)
				->set_enabled(&cfg::aimbot::enabled)
				.set_enabled(&cfg::autofire::enabled)
				.set_enabled(&g_scope_weapon);
		});

		window->build_child("RCS Standalone", framework::child_width::half, half_height, [](framework::c_child* controller) {
			controller->attach_child("AimBot", "");
			controller->enable_collapse(false);
			controller->add_checkbox("Enable", &cfg::rcs::enabled);
			controller->add_slider_float("Smooth", &cfg::rcs::smooth, 1.f, 12.f)
				->set_enabled(&cfg::rcs::enabled);
			controller->add_slider_float("Strength Y", &cfg::rcs::strength_y, 0.30f, 1.f)
				->set_enabled(&cfg::rcs::enabled);
			controller->add_slider_float("Strength X", &cfg::rcs::strength_x, 0.20f, 1.f)
				->set_enabled(&cfg::rcs::enabled);
			controller->add_slider_int("Reaction", &cfg::rcs::reaction_ms, 0, 250)
				->set_enabled(&cfg::rcs::enabled);
		});

		window->build_child("Humanize", framework::child_width::half, 360.f, [](framework::c_child* controller) {
			controller->attach_child("AimBot", "");
			controller->enable_collapse(false);
			controller->set_title_icon(kChildIcon_humanize, kChildTitleIconSize, kChildTitleIconSize);
			controller->add_checkbox("Enable Humanize", &cfg::humanize::enabled)
				->set_enabled(&cfg::aimbot::enabled);
			controller->add_slider_float("Fitts A", &cfg::humanize::fitts_a, 20.f, 120.f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Fitts B", &cfg::humanize::fitts_b, 50.f, 300.f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Target Width", &cfg::humanize::target_width, 4.f, 40.f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Overshoot Prob", &cfg::humanize::overshoot_prob, 0.f, 0.40f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Curvature", &cfg::humanize::curvature_scale, 0.f, 0.08f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("OU Sigma", &cfg::humanize::ou_sigma, 0.f, 3.f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Tremor Amp", &cfg::humanize::tremor_amp, 0.f, 1.2f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("SDN K", &cfg::humanize::sdn_k, 0.f, 0.12f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_slider_float("Speed Scale", &cfg::humanize::speed_scale, 0.50f, 2.0f)
				->set_enabled(&cfg::humanize::enabled);
			controller->add_checkbox("Rebuild On Target Switch", &cfg::humanize::rebuild_on_target_switch)
				->set_enabled(&cfg::humanize::enabled);
		});



		window->build_child("ESP Preview", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Visual", "");
			controller->add_popup("Flags", true, [](framework::c_popup* popup) {
				popup->add_checkbox("C4", &cfg::esp::flag_c4);
				popup->add_checkbox("Scoped", &cfg::esp::flag_scoped);
				popup->add_checkbox("Flashed", &cfg::esp::flag_flashed);
				popup->add_checkbox("Defusing", &cfg::esp::flag_defusing);
				popup->add_checkbox("HK / K", &cfg::esp::flag_hk);
				popup->add_checkbox("Reload", &cfg::esp::flag_reload);
				popup->add_checkbox("Immune", &cfg::esp::flag_immune);
				popup->add_checkbox("Defuse kit", &cfg::esp::flag_kit);
			});
			controller->set_paint([](framework::c_child* child) {
				const auto p = child->pos();
				const auto s = child->size();
				const float cx = p.x + s.x * 0.5f;
				const float top = p.y + 52.f;
				const float bot = p.y + s.y - 22.f;
				const float h = bot - top;
				if (h < 40.f)
					return;

				ImFont* ef = Overlays::EspText();
				ImFont* eif = Overlays::EspIcons();
				auto emeasure = [&](const char* t, ImFont* f = nullptr) -> ImVec2 {
					ImFont* use = f ? f : ef;
					if (use)
						return use->CalcTextSizeA(use->LegacySize, 1.0e30f, 0.f, t);
					const auto m = g_font->f_childs.measure(t);
					return ImVec2(m.x, m.y);
				};
				auto etext = [&](float x, float y, const char* t, hue::c_color c, ImFont* f = nullptr) {
					ImFont* use = f ? f : ef;
					if (use)
					{
						if (auto* pdl = g_render->draw_list())
							pdl->AddText(use, use->LegacySize, ImVec2(x, y), IM_COL32(c.r, c.g, c.b, c.a), t);
					}
					else
					{
						g_font->f_childs.text((int)x, (int)y, t, c);
					}
				};

				const float bw = s.x * 0.34f;
				const float bh = h * 0.72f;
				const float bx = cx - bw * 0.5f;
				const float by = top + (h - bh) * 0.42f;
				const float a = animations::m_window_opacity.val();
				const auto& bc = cfg::esp::box_visible;
				const auto boxc = hue::c_color(
					(int)(std::clamp(bc.r, 0.f, 1.f) * 255.f),
					(int)(std::clamp(bc.g, 0.f, 1.f) * 255.f),
					(int)(std::clamp(bc.b, 0.f, 1.f) * 255.f),
					(int)(std::clamp(bc.a, 0.f, 1.f) * 255.f * a));


				if (cfg::esp::box)
				{
					const int t = (std::max)(1, (int)std::lround(std::clamp(cfg::esp::box_thickness, 0.5f, 5.f)));
					if (cfg::esp::corner_box)
					{
						const float lw = bw * 0.25f;
						const float lh = bh * 0.25f;
						g_render->rect_filled((int)bx, (int)by, (int)lw, t, boxc);
						g_render->rect_filled((int)bx, (int)by, t, (int)lh, boxc);
						g_render->rect_filled((int)(bx + bw - lw), (int)by, (int)lw, t, boxc);
						g_render->rect_filled((int)(bx + bw - t), (int)by, t, (int)lh, boxc);
						g_render->rect_filled((int)bx, (int)(by + bh - t), (int)lw, t, boxc);
						g_render->rect_filled((int)bx, (int)(by + bh - lh), t, (int)lh, boxc);
						g_render->rect_filled((int)(bx + bw - lw), (int)(by + bh - t), (int)lw, t, boxc);
						g_render->rect_filled((int)(bx + bw - t), (int)(by + bh - lh), t, (int)lh, boxc);
					}
					else
					{
						g_render->rect((int)bx, (int)by, (int)bw, (int)bh, boxc, 0.f, (float)t);
					}
				}

				cfg::esp::flag_health = cfg::esp::healthbar;

				const int fa = (int)(255.f * a);
				struct PvItem {
					cfg::esp::flag_anchor* a;
					int kind;
					const char* text;
					hue::c_color col;
					float tw, th;
					float x, y, w, h;
					ImFont* font;
				};
				PvItem items[16];
				int n = 0;
				auto add_text = [&](cfg::esp::flag_anchor* a, const char* t, hue::c_color c, ImFont* f = nullptr) {
					const auto ts = emeasure(t, f);
					items[n++] = { a, 0, t, c, ts.x, ts.y, 0.f, 0.f, ts.x, ts.y, f };
				};
				if (cfg::esp::name)
				{
					const auto& nc = cfg::esp::name_color;
					const auto namec = hue::c_color(
						(int)(std::clamp(nc.r, 0.f, 1.f) * 255.f),
						(int)(std::clamp(nc.g, 0.f, 1.f) * 255.f),
						(int)(std::clamp(nc.b, 0.f, 1.f) * 255.f),
						(int)(std::clamp(nc.a, 0.f, 1.f) * 255.f * a));
					add_text(&cfg::esp::name_pos, "player", namec);
				}
				if (cfg::esp::weapon)
					add_text(&cfg::esp::weapon_pos, "AK-47 [30]", hue::c_color(230, 230, 230, fa));
				if (cfg::esp::weapon_icon)
					add_text(&cfg::esp::weapon_icon_pos, WeaponIcons::AK47, hue::c_color(230, 230, 230, fa), eif);
				if (cfg::esp::distance)
					add_text(&cfg::esp::distance_pos, "18m", hue::c_color(200, 200, 200, fa));
				if (cfg::esp::flag_hk)
					add_text(&cfg::esp::flag_hk_pos, "HK", hue::c_color(200, 200, 220, fa));
				if (cfg::esp::flag_scoped)
					add_text(&cfg::esp::flag_scoped_pos, "SCOPED", hue::c_color(80, 200, 255, fa));
				if (cfg::esp::flag_flashed)
					add_text(&cfg::esp::flag_flashed_pos, "FLASHED", hue::c_color(255, 230, 60, fa));
				if (cfg::esp::flag_reload)
					add_text(&cfg::esp::flag_reload_pos, "RELOAD", hue::c_color(255, 160, 60, fa));
				if (cfg::esp::flag_c4)
					add_text(&cfg::esp::flag_c4_pos, "C4", hue::c_color(255, 180, 40, fa));
				if (cfg::esp::flag_kit)
					add_text(&cfg::esp::flag_kit_pos, "DEFUSE", hue::c_color(80, 220, 160, fa));
				if (cfg::esp::flag_defusing)
					add_text(&cfg::esp::flag_defusing_pos, "DEFUSING", hue::c_color(80, 220, 120, fa));
				if (cfg::esp::flag_immune)
					add_text(&cfg::esp::flag_immune_pos, "IMMUNE", hue::c_color(180, 80, 255, fa));
				const float hp_th = std::clamp(cfg::esp::healthbar_thickness, 1.f, 8.f);
				if (cfg::esp::healthbar)
					items[n++] = { &cfg::esp::flag_health_pos, 1, "100", hue::c_color(255, 255, 255, fa), hp_th, bh, 0.f, 0.f, hp_th, bh };
				if (cfg::esp::armorbar)
					items[n++] = { &cfg::esp::armorbar_pos, 2, nullptr, hue::c_color(80, 160, 255, fa), 2.f, bh, 0.f, 0.f, 2.f, bh };

				for (int i = 0; i < n; ++i)
					for (int j = i + 1; j < n; ++j)
						if (items[j].a->order < items[i].a->order)
							std::swap(items[i], items[j]);

				float used_bar[4] = {};
				float used_txt[12] = {};
				for (int i = 0; i < n; ++i)
				{
					auto& it = items[i];
					if (it.kind == 0)
						continue;
					const float thk = (it.kind == 1) ? hp_th : 2.f;
					cfg::esp::LayoutBar(*it.a, used_bar[it.a->side], bx, by, bw, bh, thk, it.x, it.y, it.w, it.h);
					used_bar[it.a->side] += ((it.a->side == 1 || it.a->side == 3) ? it.w : it.h) + 3.f;
				}
				for (int i = 0; i < n; ++i)
				{
					auto& it = items[i];
					if (it.kind != 0)
						continue;
					const int slot = cfg::esp::SlotKey(*it.a);
					cfg::esp::FlagPos(*it.a, bx, by, bw, bh, it.tw, it.th, it.x, it.y);
					cfg::esp::OffsetByBar(*it.a, used_bar[it.a->side], it.x, it.y);
					cfg::esp::StackFlag(*it.a, used_txt[slot], it.y);
					used_txt[slot] += it.th + 1.f;
					it.w = it.tw;
					it.h = it.th;
				}

				static cfg::esp::flag_anchor* drag_a = nullptr;
				static int drag_kind = 0;
				static float drag_grab_x = 0.f;
				static float drag_grab_y = 0.f;
				const auto mp = g_input->get_mouse_position();
				const bool in_title = mp.y < p.y + 32.f;
				const bool blocked = g_ctx->m_modal_owner != nullptr || in_title || g_ctx->m_click_consumed;
				if (!g_input->click_down(input::mouse_buttons::left))
				{
					if (drag_a)
					{
						const bool flag_item = drag_kind == 0
							&& drag_a != &cfg::esp::weapon_pos && drag_a != &cfg::esp::weapon_icon_pos && drag_a != &cfg::esp::distance_pos;
						cfg::esp::SnapFlag(*drag_a, mp.x, mp.y, bx, by, bw, bh, flag_item);
						drag_a->order = ++cfg::esp::flag_order_seq;
						drag_a = nullptr;
					}
				}
				else if (!drag_a && !blocked && g_input->clicked(input::mouse_buttons::left))
				{
					for (int i = n - 1; i >= 0; --i)
					{
						const auto& it = items[i];
						if (mp.x >= it.x - 3.f && mp.y >= it.y - 3.f && mp.x <= it.x + it.w + 3.f && mp.y <= it.y + it.h + 3.f)
						{
							drag_a = it.a;
							drag_kind = it.kind;
							drag_grab_x = mp.x - it.x;
							drag_grab_y = mp.y - it.y;
							g_ctx->m_click_consumed = true;
							break;
						}
					}
				}

				auto draw_health = [&](float x, float y, float w, float h) {
					const float ha = std::clamp(cfg::esp::healthbar_alpha, 0.f, 1.f);
					if (cfg::esp::healthbar_outline)
						g_render->rect_filled((int)x - 1, (int)y - 1, (int)w + 2, (int)h + 2, hue::c_color(0, 0, 0, (int)(200.f * a * ha)));
					if (auto* dl = g_render->draw_list())
					{
						const auto& hf = cfg::esp::healthbar_full;
						const auto& hl = cfg::esp::healthbar_low;
						const int aa = (int)(255.f * a * ha);
						const ImU32 hi = IM_COL32((int)(hf.r * 255.f), (int)(hf.g * 255.f), (int)(hf.b * 255.f), aa);
						const ImU32 lo = IM_COL32((int)(hl.r * 255.f), (int)(hl.g * 255.f), (int)(hl.b * 255.f), aa);
						if (h >= w)
							dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), hi, hi, lo, lo);
						else
							dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), lo, hi, hi, lo);
					}
					const auto ts = emeasure("100");
					float tx = x - ts.x - 3.f, ty = y - ts.y * 0.5f;
					if (w >= h)
					{
						tx = x + w + 3.f;
						ty = y + (h - ts.y) * 0.5f;
					}
					else
						ty = y;
					etext(tx, ty, "100", hue::c_color(255, 255, 255, (int)(255.f * a)));
				};

				for (int i = 0; i < n; ++i)
				{
					const auto& it = items[i];
					if (drag_a == it.a)
						continue;
					if (it.kind == 1)
						draw_health(it.x, it.y, it.w, it.h);
					else if (it.kind == 2)
					{
						g_render->rect_filled((int)it.x - 1, (int)it.y - 1, (int)it.w + 2, (int)it.h + 2, hue::c_color(0, 0, 0, (int)(200.f * a)));
						g_render->rect_filled((int)it.x, (int)it.y, (int)it.w, (int)it.h, hue::c_color(80, 160, 255, fa));
					}
					else
						etext(it.x, it.y, it.text, it.col, it.font);
				}

				if (drag_a)
				{
						cfg::esp::flag_anchor preview = *drag_a;
						const bool flag_item = drag_kind == 0
							&& drag_a != &cfg::esp::weapon_pos && drag_a != &cfg::esp::weapon_icon_pos && drag_a != &cfg::esp::distance_pos;
						cfg::esp::SnapFlag(preview, mp.x, mp.y, bx, by, bw, bh, flag_item);
						preview.order = drag_a->order;
						if (auto* dl = g_render->draw_list())
						{
							const ImU32 sc = IM_COL32(255, 255, 255, (int)(90.f * a));
							const ImU32 hi = IM_COL32(80, 200, 255, (int)(220.f * a));
							if (flag_item)
							{
								const ImVec2 spots[4] = {
									{ bx + bw * 0.5f, by },
									{ bx + bw * 0.5f, by + bh },
									{ bx, by },
									{ bx + bw, by },
								};
								const int active = (preview.side == 0) ? 0
									: (preview.side == 2) ? 1
									: (preview.side == 3) ? 2
									: 3;
								for (int i = 0; i < 4; ++i)
									dl->AddCircleFilled(spots[i], i == active ? 4.5f : 3.f, i == active ? hi : sc);
							}
							else
							{
								const ImVec2 spots[12] = {
									{ bx, by }, { bx + bw * 0.5f, by }, { bx + bw, by },
									{ bx, by + bh }, { bx + bw * 0.5f, by + bh }, { bx + bw, by + bh },
									{ bx, by }, { bx, by + bh * 0.5f }, { bx, by + bh },
									{ bx + bw, by }, { bx + bw, by + bh * 0.5f }, { bx + bw, by + bh },
								};
								const int along_i = preview.along < 0.25f ? 0 : (preview.along > 0.75f ? 2 : 1);
								const int active = (preview.side == 0) ? along_i
									: (preview.side == 2) ? (3 + along_i)
									: (preview.side == 3) ? (6 + along_i)
									: (9 + along_i);
								for (int i = 0; i < 12; ++i)
									dl->AddCircleFilled(spots[i], i == active ? 4.5f : 3.f, i == active ? hi : sc);
							}
						}
						float sx = mp.x, sy = mp.y, sw = 8.f, sh = 8.f;
						if (drag_kind == 1)
							cfg::esp::LayoutBar(preview, 0.f, bx, by, bw, bh, hp_th, sx, sy, sw, sh);
						else if (drag_kind == 2)
							cfg::esp::LayoutBar(preview, 0.f, bx, by, bw, bh, 2.f, sx, sy, sw, sh);
						else
						{
							for (int i = 0; i < n; ++i)
							{
								if (items[i].a != drag_a)
									continue;
								cfg::esp::FlagPos(preview, bx, by, bw, bh, items[i].tw, items[i].th, sx, sy);
								etext(sx, sy, items[i].text, items[i].col, items[i].font);
								break;
							}
						}
						if (drag_kind == 1)
							draw_health(sx, sy, sw, sh);
						else if (drag_kind == 2)
						{
							g_render->rect_filled((int)sx - 1, (int)sy - 1, (int)sw + 2, (int)sh + 2, hue::c_color(0, 0, 0, (int)(200.f * a)));
							g_render->rect_filled((int)sx, (int)sy, (int)sw, (int)sh, hue::c_color(80, 160, 255, fa));
						}
				}
			});
		});
		g_visuals_child = window->build_child("Visuals", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Visual", "");
			controller->set_title_action([] { g_mate_visuals_open = !g_mate_visuals_open; }, [] { return g_mate_visuals_open; });
			controller->add_checkbox("Box", &cfg::esp::box);
			controller->add_popup("Box", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Corner box", &cfg::esp::corner_box);
				popup->add_slider_float("Thickness", &cfg::esp::box_thickness, 0.5f, 5.f);
			})->set_inlined().set_enabled(&cfg::esp::box);
			controller->add_colorpicker("Invisible", &g_box_invisible, true)
				->set_inlined().set_enabled(&cfg::esp::box);
			controller->add_colorpicker("Visible", &g_box_visible, true)
				->set_inlined().set_enabled(&cfg::esp::box);
			controller->add_checkbox("Name", &cfg::esp::name);
			controller->add_colorpicker("Name", &g_name_color, true)
				->set_inlined().set_enabled(&cfg::esp::name);
			controller->add_checkbox("Healthbar", &cfg::esp::healthbar);
			controller->add_popup("Healthbar", true, [](framework::c_popup* popup) {
				popup->add_slider_float("Thickness", &cfg::esp::healthbar_thickness, 1.f, 8.f);
				popup->add_slider_float("Opacity", &cfg::esp::healthbar_alpha, 0.f, 1.f);
				popup->add_checkbox("Outline", &cfg::esp::healthbar_outline);
				popup->add_colorpicker("Full", &g_hp_full, true);
				popup->add_colorpicker("Low", &g_hp_low, true);
			})->set_inlined().set_enabled(&cfg::esp::healthbar);
			controller->add_checkbox("Armor bar", &cfg::esp::armorbar);
			controller->add_checkbox("Weapon", &cfg::esp::weapon);
			controller->add_checkbox("Weapon Icon", &cfg::esp::weapon_icon);
			controller->add_checkbox("Distance", &cfg::esp::distance);
			controller->add_checkbox("Skeleton", &cfg::esp::skeleton);
			controller->add_colorpicker("Invisible\xE2\x80\x8B", &g_skel_invisible, true)
				->set_inlined().set_enabled(&cfg::esp::skeleton);
			controller->add_colorpicker("Visible\xE2\x80\x8B", &g_skel_visible, true)
				->set_inlined().set_enabled(&cfg::esp::skeleton);
			controller->add_checkbox("World items", &cfg::world::items);
			controller->add_popup("World items", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Name", &cfg::world::item_name);
				popup->add_checkbox("Icon", &cfg::world::item_icon);
				popup->add_checkbox("Ammo", &cfg::world::item_ammo);
				popup->add_slider_float("Distance", &cfg::world::item_distance, 5.f, 80.f);
				popup->add_checkbox("Rifles", &cfg::world::item_rifles);
				popup->add_checkbox("SMGs", &cfg::world::item_smgs);
				popup->add_checkbox("Shotguns", &cfg::world::item_shotguns);
				popup->add_checkbox("Snipers", &cfg::world::item_snipers);
				popup->add_checkbox("Pistols", &cfg::world::item_pistols);
				popup->add_checkbox("Heavy", &cfg::world::item_heavy);
				popup->add_checkbox("Grenades", &cfg::world::item_grenades);
				popup->add_checkbox("Utility", &cfg::world::item_utility);
			})->set_inlined().set_enabled(&cfg::world::items);
			controller->add_checkbox("Projectiles", &cfg::world::projectiles);
			controller->add_popup("Projectiles", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Name", &cfg::world::proj_name);
				popup->add_checkbox("Icon", &cfg::world::proj_icon);
				popup->add_checkbox("Timer", &cfg::world::proj_timer);
				popup->add_checkbox("Inferno", &cfg::world::inferno_bounds);
			})->set_inlined().set_enabled(&cfg::world::projectiles);
			controller->add_checkbox("Grenade predict", &cfg::world::grenade_pred);
			controller->add_popup("Grenade predict", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Local only", &cfg::world::grenade_local_only);
			})->set_inlined().set_enabled(&cfg::world::grenade_pred);

		});

		g_mate_visuals_child = window->build_child("Teammates", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Visual", "");
			controller->set_overlay(true);
			controller->set_extra_visible(&g_mate_visuals_open);
			controller->add_checkbox("Enable", &cfg::esp::mate_enabled);
			controller->add_checkbox("Box", &cfg::esp::mate_box)
				->set_enabled(&cfg::esp::mate_enabled);
			controller->add_popup("Box", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Corner box", &cfg::esp::mate_corner_box);
				popup->add_slider_float("Thickness", &cfg::esp::mate_box_thickness, 0.5f, 5.f);
			})->set_inlined().set_enabled(&cfg::esp::mate_box);
			controller->add_colorpicker("Invisible", &g_mate_box_invisible, true)
				->set_inlined().set_enabled(&cfg::esp::mate_box);
			controller->add_colorpicker("Visible", &g_mate_box_visible, true)
				->set_inlined().set_enabled(&cfg::esp::mate_box);
			controller->add_checkbox("Name", &cfg::esp::mate_name)
				->set_enabled(&cfg::esp::mate_enabled);
			controller->add_checkbox("Healthbar", &cfg::esp::mate_healthbar)
				->set_enabled(&cfg::esp::mate_enabled);
			controller->add_checkbox("Skeleton", &cfg::esp::mate_skeleton)
				->set_enabled(&cfg::esp::mate_enabled);
			controller->add_colorpicker("Invisible\xE2\x80\x8B", &g_mate_skel_invisible, true)
				->set_inlined().set_enabled(&cfg::esp::mate_skeleton);
			controller->add_colorpicker("Visible\xE2\x80\x8B", &g_mate_skel_visible, true)
				->set_inlined().set_enabled(&cfg::esp::mate_skeleton);
			controller->add_popup("Flags", true, [](framework::c_popup* popup) {
				popup->add_checkbox("C4", &cfg::esp::mate_flag_c4);
				popup->add_checkbox("Scoped", &cfg::esp::mate_flag_scoped);
				popup->add_checkbox("Flashed", &cfg::esp::mate_flag_flashed);
				popup->add_checkbox("Defusing", &cfg::esp::mate_flag_defusing);
			})->set_enabled(&cfg::esp::mate_enabled);
		});

		window->build_child("Radar", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Radar", "");
			controller->add_checkbox("Enable", &cfg::radar::enabled);
			controller->add_checkbox("Teammates", &cfg::radar::show_teammates)
				->set_enabled(&cfg::radar::enabled);
			controller->add_dropdown("Point", &cfg::radar::type, { "Circle", "Arrow", "Circle + Arrow" })
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Size", &cfg::radar::size, 80.f, 480.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Range", &cfg::radar::range, 40.f, 400.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Proportion", &cfg::radar::proportion, 400.f, 8000.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Opacity", &cfg::radar::opacity, 0.05f, 1.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Pos X", &cfg::radar::pos_x, 0.f, 1600.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_slider_float("Pos Y", &cfg::radar::pos_y, 0.f, 1000.f)
				->set_enabled(&cfg::radar::enabled);
			controller->add_colorpicker("Enemy", &g_radar_enemy)
				->set_enabled(&cfg::radar::enabled);
			controller->add_colorpicker("Teammate", &g_radar_teammate)
				->set_enabled(&cfg::radar::enabled);
		});


		window->build_child("Settings", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Misc", "");
			controller->add_checkbox("Watermark", &cfg::settings::watermark);
			controller->add_checkbox("Streamproof", &cfg::settings::streamproof);
			controller->add_checkbox("VSync", &cfg::settings::vsync);
			controller->add_checkbox("Free CPU", &cfg::settings::free_cpu);
			controller->add_checkbox("Auto Accept", &cfg::settings::auto_accept);
			controller->add_checkbox("Debug Multipoint", &cfg::settings::debug_multipoint);
			controller->add_slider_int("Cache interval", &cfg::settings::cache_ms, 0, 16);
			controller->add_button("UnLoad", [] {
				Renderer::Unload();
			});
		});

		window->build_child("Menu Customization", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Misc", "");
			controller->add_colorpicker("Menu Color", &g_menu_color);
			controller->add_slider_float("Background Opacity", &cfg::settings::menu_bg_opacity, 0.f, 1.f);
		});

		window->build_child("Movement", framework::child_width::half, full_height, [](framework::c_child* controller) {
			controller->attach_child("Movement", "");
			controller->add_checkbox("Bhop", &cfg::movement::bhop);
			controller->add_checkbox("Strafe Helper", &cfg::movement::strafe_helper);
		});

		window->build_child("Configs", framework::child_width::full, full_height, [](framework::c_child* controller) {
			controller->attach_child("Config", "");
			controller->add_input_box("Name", &g_cfg_name);
			g_cfg_list = controller->add_listbox("Saved", &g_cfg_selected, Config::List(), 160.f, false);
			controller->add_button("Create", [] {
				try {
					LOGF(INFO, "Config Create '{}'", g_cfg_name);
					if (g_cfg_name.empty()) { g_cfg_status = "Empty name"; return; }
					g_cfg_status = Config::Create(g_cfg_name) ? "Created " + g_cfg_name : "Create failed";
					RefreshCfgList();
				} catch (const std::exception& e) { g_cfg_status = e.what(); }
			});
			controller->add_button("Save", [] {
				try {
					auto names = Config::List();
					const std::string target = (g_cfg_selected >= 0 && g_cfg_selected < static_cast<int>(names.size()))
						? names[g_cfg_selected] : g_cfg_name;
					LOGF(INFO, "Config Save '{}'", target);
					if (target.empty()) { g_cfg_status = "Empty name"; return; }
					if (Config::Save(target)) { g_cfg_name = target; g_cfg_status = "Saved " + target; }
					else g_cfg_status = "Save failed";
					RefreshCfgList();
				} catch (const std::exception& e) { g_cfg_status = e.what(); }
			})->set_inlined();
			controller->add_button("Load", [] {
				try {
					auto names = Config::List();
					if (g_cfg_selected < 0 || g_cfg_selected >= static_cast<int>(names.size())) {
						g_cfg_status = "Nothing selected";
						return;
					}
					LOGF(INFO, "Config Load '{}'", names[g_cfg_selected]);
					if (Config::Load(names[g_cfg_selected])) {
						g_cfg_name = names[g_cfg_selected];
						g_cfg_status = "Loaded " + names[g_cfg_selected];
						PushColorsToWidgets();
					} else g_cfg_status = "Load failed";
				} catch (const std::exception& e) { g_cfg_status = e.what(); }
			});
			controller->add_button("Delete", [] {
				try {
					auto names = Config::List();
					if (g_cfg_selected < 0 || g_cfg_selected >= static_cast<int>(names.size())) {
						g_cfg_status = "Nothing selected";
						return;
					}
					LOGF(INFO, "Config Delete '{}'", names[g_cfg_selected]);
					g_cfg_status = Config::Remove(names[g_cfg_selected]) ? "Deleted" : "Delete failed";
					RefreshCfgList();
				} catch (const std::exception& e) { g_cfg_status = e.what(); }
			})->set_inlined();
		});

		this->m_windows.push_back(window);
		RefreshCfgList();
	}

	void c_menu::runtime()
	{
		g_ctx->m_click_consumed = false;

		if (g_ctx->m_cur_tab != "Visual")
			g_mate_visuals_open = false;

		if (g_mate_visuals_child && g_visuals_child)
		{
			const auto p = g_visuals_child->pos();
			const auto s = g_visuals_child->size();
			g_mate_visuals_child->place(math::c_vector_2d(p.x + s.x + 12.f, p.y), s);
		}

		const bool picker_open = g_ctx->top_focus() &&
			g_ctx->top_focus()->m_type == element_type::colorpicker;
		if (!picker_open)
			PushColorsToWidgets();

		static int last_weapon_sel = -1;
		const short held = FrameStore::Get().Latest().local_weapon;
		const int held_idx = cfg::aimbot::IndexOf(held);
		if (held_idx >= 0 && held != g_last_held_weapon)
		{
			g_weapon_selected = held_idx;
			g_last_held_weapon = held;
			SyncWeaponWidgets();
			last_weapon_sel = g_weapon_selected;
			if (g_weapon_list)
				g_weapon_list->scroll_to(held_idx);
		}

		if (g_weapon_list && g_ctx->m_cur_tab == "AimBot")
			g_weapon_list->set_items(BuildWeaponLabels(held));

		if (g_weapon_selected != last_weapon_sel)
		{
			SyncWeaponWidgets();
			last_weapon_sel = g_weapon_selected;
			if (g_weapon_list)
				g_weapon_list->scroll_to(g_weapon_selected);
		}

		short scope_id = held;
		if (cfg::aimbot::IndexOf(scope_id) < 0 &&
			g_weapon_selected >= 0 && g_weapon_selected < cfg::aimbot::kWeaponCount)
			scope_id = cfg::aimbot::kWeapons[g_weapon_selected].id;
		g_scope_weapon = cfg::aimbot::IsScopeWeapon(scope_id);

		for (auto& windows : this->m_windows)
		{
			windows->input();
			windows->paint();
		}

		for (auto& windows : this->m_windows)
			windows->paint_overlays();

		if (g_mate_visuals_open && g_input->clicked(input::mouse_buttons::left))
		{
			const bool on_panel = g_mate_visuals_child &&
				g_input->mouse_in_region(g_mate_visuals_child->pos(), g_mate_visuals_child->size());
			const bool on_arrow = g_visuals_child && g_visuals_child->title_action_hovered();
			const bool on_own_modal = g_mate_visuals_child && g_mate_visuals_child->owns_modal();
			if (!on_panel && !on_arrow && !on_own_modal)
				g_mate_visuals_open = false;
		}
		PullWeaponWidgets();
		if (!g_scope_weapon)
		{
			cfg::autofire::autoscope = false;
			cfg::autofire::block_noscope = false;
		}
		if (!cfg::autofire::enabled)
			cfg::autofire::early_stop = false;
		PullColorsFromWidgets();
		ApplyMenuStyle();
	}
}
