#include "Config.hpp"
#include "core/engine/TickTime.hpp"
#include <shlobj.h>
#include <cctype>
#include <algorithm>


static std::string SanitizeName(const std::string& name)
{
	std::string out;
	out.reserve(name.size());
	for (unsigned char c : name)
	{
		if (std::isalnum(c) || c == '_' || c == '-' || c == ' ')
			out.push_back(static_cast<char>(c));
	}
	while (!out.empty() && out.front() == ' ')
		out.erase(out.begin());
	while (!out.empty() && out.back() == ' ')
		out.pop_back();
	if (out.empty())
		out = "default";
	return out;
}

std::filesystem::path Config::Dir()
{
	wchar_t docs[MAX_PATH]{};
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)))
		return std::filesystem::current_path() / L"чита";
	return std::filesystem::path(docs) / L"чита";
}

void Config::EnsureDir()
{
	std::error_code ec;
	std::filesystem::create_directories(Dir(), ec);
}

std::filesystem::path Config::PathFor(const std::string& name)
{
	return Dir() / (SanitizeName(name) + ".json");
}

const std::string& Config::CurrentName()
{
	return GetInstance().current_name;
}

std::vector<std::string> Config::List()
{
	std::vector<std::string> names;
	try {
		EnsureDir();
		std::error_code ec;
		const auto dir = Dir();
		if (!std::filesystem::exists(dir, ec) || ec)
			return names;

		for (auto& entry : std::filesystem::directory_iterator(dir, ec))
		{
			if (ec)
				break;
			if (!entry.is_regular_file(ec) || ec)
				continue;
			if (entry.path().extension() != ".json")
				continue;
			try {
				names.push_back(entry.path().stem().string());
			}
			catch (...) {
				continue;
			}
		}
		std::sort(names.begin(), names.end());
	}
	catch (const std::exception& e) {
		LOGF(FATAL, "Config::List failed: {}", e.what());
	}
	return names;
}

bool Config::Create(const std::string& name)
{
	const auto clean = SanitizeName(name);
	GetInstance().current_name = clean;
	return GetInstance().WriteTo(PathFor(clean));
}

bool Config::Save(const std::string& name)
{
	const auto clean = SanitizeName(name);
	GetInstance().current_name = clean;
	return GetInstance().WriteTo(PathFor(clean));
}

bool Config::Load(const std::string& name)
{
	const auto clean = SanitizeName(name);
	GetInstance().current_name = clean;
	return GetInstance().ReadFrom(PathFor(clean));
}

bool Config::Remove(const std::string& name)
{
	const auto clean = SanitizeName(name);
	std::error_code ec;
	const bool ok = std::filesystem::remove(PathFor(clean), ec);
	if (GetInstance().current_name == clean)
		GetInstance().current_name = "default";
	return ok && !ec;
}


bool Config::Read() {
	return GetInstance().ReadImpl();
}

bool Config::Write() {
	return GetInstance().WriteImpl();
}

bool Config::ReadImpl() {
	EnsureDir();
	const auto path = PathFor(current_name);
	if (!std::filesystem::exists(path))
	{
		const auto legacy = std::filesystem::path("config.json");
		if (std::filesystem::exists(legacy))
		{
			if (!ReadFrom(legacy))
				return false;
			WriteTo(path);
			return true;
		}
		LOGF(FATAL, "Configuration file does not exist, creating a new one");
		WriteImpl();
		return false;
	}
	return ReadFrom(path);
}

bool Config::ReadFrom(const std::filesystem::path& path) {
	std::ifstream f(path);

	if (!f.good()) {
		LOGF(FATAL, "Configuration file does not exist, creating a new one");
		WriteImpl();
		return false;
	}

	json data;
	try {
		data = json::parse(f);
	}
	catch (const std::exception&) {
		LOGF(FATAL, "Failed to parse configuration file");
		return false;
	}

	if (data.empty())
		return false;

	try {
		const json empty = json::object();
		const json& movement = (data.contains("movement") && data["movement"].is_object()) ? data["movement"] : empty;
		const json& aimbot = (data.contains("aimbot") && data["aimbot"].is_object()) ? data["aimbot"] : empty;
		const json& humanize = (data.contains("humanize") && data["humanize"].is_object()) ? data["humanize"] : empty;
		const json& triggerbot = (data.contains("triggerbot") && data["triggerbot"].is_object()) ? data["triggerbot"] : empty;
		const json& autofire = (data.contains("autofire") && data["autofire"].is_object()) ? data["autofire"] : empty;
		const json& rcs = (data.contains("rcs") && data["rcs"].is_object()) ? data["rcs"] : empty;
		const json& utils = (data.contains("utils") && data["utils"].is_object()) ? data["utils"] : empty;
		const json& esp = (data.contains("esp") && data["esp"].is_object()) ? data["esp"] : empty;

		cfg::movement::bhop = movement.value("bhop", false);
		cfg::movement::strafe_helper = movement.value("strafe_helper", false);

		cfg::aimbot::enabled = aimbot.value("enabled", false);
		cfg::aimbot::fov = aimbot.value("fov", 4.f);
		cfg::aimbot::smooth = tick::FromLegacySlider(aimbot.value("smooth", 4.f));
		cfg::aimbot::bone = aimbot.value("bone", 0);
		cfg::aimbot::draw_fov = aimbot.value("draw_fov", false);
		cfg::aimbot::hb_head = aimbot.value("hb_head", true);
		cfg::aimbot::hb_chest = aimbot.value("hb_chest", false);
		cfg::aimbot::hb_stomach = aimbot.value("hb_stomach", false);
		cfg::aimbot::hb_arms = aimbot.value("hb_arms", false);
		cfg::aimbot::hb_legs = aimbot.value("hb_legs", false);
		cfg::aimbot::hb_nearest = aimbot.value("hb_nearest", false);
		cfg::aimbot::multipoints = aimbot.value("multipoints", false);
		cfg::aimbot::multipoint_scale = std::clamp(aimbot.value("multipoint_scale", 0.70f), 0.f, 1.f);
		cfg::aimbot::multipoint_count = std::clamp(aimbot.value("multipoint_count", 5), 0, 12);
		cfg::aimbot::weapon_override = aimbot.value("weapon_override", false);
		cfg::aimbot::kill_delay = aimbot.value("kill_delay", true);
		cfg::aimbot::kill_delay_ms = std::clamp(aimbot.value("kill_delay_ms", 200), 0, 1000);
		cfg::humanize::enabled = humanize.value("enabled", false);
		cfg::humanize::fitts_a = std::clamp(humanize.value("fitts_a", 85.f), 20.f, 120.f);
		cfg::humanize::fitts_b = std::clamp(humanize.value("fitts_b", 190.f), 50.f, 300.f);
		cfg::humanize::target_width = std::clamp(humanize.value("target_width", 7.f), 4.f, 40.f);
		cfg::humanize::overshoot_prob = std::clamp(humanize.value("overshoot_prob", 0.f), 0.f, 0.40f);
		cfg::humanize::curvature_scale = std::clamp(humanize.value("curvature_scale", 0.006f), 0.f, 0.08f);
		cfg::humanize::ou_sigma = std::clamp(humanize.value("ou_sigma", 0.25f), 0.f, 3.f);
		cfg::humanize::tremor_amp = std::clamp(humanize.value("tremor_amp", 0.08f), 0.f, 1.2f);
		cfg::humanize::sdn_k = std::clamp(humanize.value("sdn_k", 0.012f), 0.f, 0.12f);
		cfg::humanize::speed_scale = std::clamp(humanize.value("speed_scale", 0.70f), 0.50f, 2.0f);
		cfg::humanize::rebuild_on_target_switch = humanize.value("rebuild_on_target_switch", false);
		if (aimbot.contains("weapons") && aimbot["weapons"].is_array())
		{
			for (const auto& w : aimbot["weapons"])
			{
				if (!w.is_object() || !w.contains("id"))
					continue;
				const short id = (short)w.value("id", 0);
				auto& tune = cfg::aimbot::Tune(id);
				tune.override = w.value("override", false);
				tune.fov = w.value("fov", cfg::aimbot::fov);
				tune.smooth = tick::FromLegacySlider(w.value("smooth", cfg::aimbot::smooth));
				tune.hb_nearest = w.value("hb_nearest", cfg::aimbot::hb_nearest);
				tune.hb_head = w.value("hb_head", cfg::aimbot::hb_head);
				tune.hb_chest = w.value("hb_chest", cfg::aimbot::hb_chest);
				tune.hb_stomach = w.value("hb_stomach", cfg::aimbot::hb_stomach);
				tune.hb_arms = w.value("hb_arms", cfg::aimbot::hb_arms);
				tune.hb_legs = w.value("hb_legs", cfg::aimbot::hb_legs);
				tune.trg_hit_chance = w.value("trg_hit_chance", cfg::triggerbot::hit_chance);
				tune.trg_reaction_time = w.value("trg_reaction_time", cfg::triggerbot::reaction_time);
				tune.trg_before_shot = w.value("trg_before_shot", cfg::triggerbot::before_shot_delay);
				tune.trg_between_shots = w.value("trg_between_shots", cfg::triggerbot::between_shots);
				tune.trg_block_noscope = w.value("trg_block_noscope", cfg::triggerbot::block_noscope);
				tune.trg_hb_head = w.value("trg_hb_head", cfg::triggerbot::hb_head);
				tune.trg_hb_chest = w.value("trg_hb_chest", cfg::triggerbot::hb_chest);
				tune.trg_hb_stomach = w.value("trg_hb_stomach", cfg::triggerbot::hb_stomach);
				tune.trg_hb_arms = w.value("trg_hb_arms", cfg::triggerbot::hb_arms);
				tune.trg_hb_legs = w.value("trg_hb_legs", cfg::triggerbot::hb_legs);
				tune.af_hit_chance = w.value("af_hit_chance", cfg::autofire::hit_chance);
				tune.af_reaction_time = w.value("af_reaction_time", cfg::autofire::reaction_time);
				tune.af_before_shot = w.value("af_before_shot", cfg::autofire::before_shot_delay);
				tune.af_between_shots = w.value("af_between_shots", cfg::autofire::between_shots);
				tune.af_block_noscope = w.value("af_block_noscope", cfg::autofire::block_noscope);
				tune.af_autoscope = w.value("af_autoscope", cfg::autofire::autoscope);
			}
		}

		cfg::triggerbot::enabled = triggerbot.value("enabled", false);
		cfg::triggerbot::hit_chance = triggerbot.value("hit_chance", 0);
		cfg::triggerbot::reaction_time = triggerbot.value("reaction_time", 0);
		cfg::triggerbot::before_shot_delay = triggerbot.value("before_shot_delay", 0);
		cfg::triggerbot::between_shots = triggerbot.value("between_shots", 20);
		cfg::triggerbot::block_noscope = triggerbot.value("block_noscope", true);
		cfg::triggerbot::hb_head = triggerbot.value("hb_head", true);
		cfg::triggerbot::hb_chest = triggerbot.value("hb_chest", true);
		cfg::triggerbot::hb_stomach = triggerbot.value("hb_stomach", false);
		cfg::triggerbot::hb_arms = triggerbot.value("hb_arms", false);
		cfg::triggerbot::hb_legs = triggerbot.value("hb_legs", false);

		cfg::autofire::enabled = autofire.value("enabled", false);
		cfg::autofire::early_stop = autofire.value("early_stop", false);
		cfg::autofire::autoscope = autofire.value("autoscope", false);
		cfg::autofire::block_noscope = autofire.value("block_noscope", true);
		cfg::autofire::reaction_time = autofire.value("reaction_time", 100);
		cfg::autofire::before_shot_delay = autofire.value("before_shot_delay", 0);
		cfg::autofire::between_shots = autofire.value("between_shots", 45);
		cfg::autofire::hit_chance = autofire.value("hit_chance", 70);
		cfg::rcs::enabled = rcs.value("enabled", false);
		cfg::rcs::smooth = std::clamp(rcs.value("smooth", 3.2f), 1.f, 12.f);
		cfg::rcs::strength_y = std::clamp(rcs.value("strength_y", 0.82f), 0.30f, 1.f);
		cfg::rcs::strength_x = std::clamp(rcs.value("strength_x", 0.55f), 0.20f, 1.f);
		cfg::rcs::reaction_ms = std::clamp(rcs.value("reaction_ms", 110), 0, 250);
		cfg::esp::box = esp.value("box", true);
		cfg::esp::corner_box = esp.value("corner_box", false);
		cfg::esp::name = esp.value("name", false);
		cfg::esp::box_thickness = std::clamp(esp.value("box_thickness", 1.5f), 0.5f, 5.f);
		cfg::esp::skeleton = esp.value("skeleton", false);
		cfg::esp::healthbar = esp.value("healthbar", false);
		cfg::esp::healthbar_thickness = std::clamp(esp.value("healthbar_thickness", 3.f), 1.f, 8.f);
		cfg::esp::healthbar_outline = esp.value("healthbar_outline", true);
		cfg::esp::healthbar_alpha = std::clamp(esp.value("healthbar_alpha", 1.f), 0.f, 1.f);
		cfg::esp::armorbar = esp.value("armorbar", false);
		cfg::esp::weapon = esp.value("weapon", false);
		cfg::esp::weapon_icon = esp.value("weapon_icon", false);
		cfg::esp::distance = esp.value("distance", false);
		cfg::esp::flag_c4 = esp.value("flag_c4", false);
		cfg::esp::flag_scoped = esp.value("flag_scoped", false);
		cfg::esp::flag_flashed = esp.value("flag_flashed", false);
		cfg::esp::flag_defusing = esp.value("flag_defusing", false);
		cfg::esp::flag_hk = esp.value("flag_hk", false);
		cfg::esp::flag_reload = esp.value("flag_reload", false);
		cfg::esp::flag_immune = esp.value("flag_immune", false);
		cfg::esp::flag_kit = esp.value("flag_kit", false);
		cfg::esp::flag_health = cfg::esp::healthbar || esp.value("flag_health", false);
		if (cfg::esp::flag_health)
			cfg::esp::healthbar = true;
		auto load_anchor = [&](const char* key, cfg::esp::flag_anchor& dest, const cfg::esp::flag_anchor& def) {
			if (!esp.contains(key) || !esp[key].is_object()) {
				dest = def;
				return;
			}
			dest.side = std::clamp(esp[key].value("side", def.side), 0, 3);
			dest.along = esp[key].value("along", def.along);
			dest.order = esp[key].value("order", def.order);
			cfg::esp::NormalizeAnchor(dest);
			if (dest.order >= cfg::esp::flag_order_seq)
				cfg::esp::flag_order_seq = dest.order + 1;
		};
		load_anchor("flag_c4_pos", cfg::esp::flag_c4_pos, { 1, 0.f, 1 });
		load_anchor("flag_scoped_pos", cfg::esp::flag_scoped_pos, { 1, 0.f, 2 });
		load_anchor("flag_flashed_pos", cfg::esp::flag_flashed_pos, { 1, 0.f, 3 });
		load_anchor("flag_defusing_pos", cfg::esp::flag_defusing_pos, { 1, 0.f, 4 });
		load_anchor("flag_health_pos", cfg::esp::flag_health_pos, { 3, 0.5f, 5 });
		load_anchor("flag_hk_pos", cfg::esp::flag_hk_pos, { 1, 0.f, 6 });
		load_anchor("flag_reload_pos", cfg::esp::flag_reload_pos, { 1, 0.f, 7 });
		load_anchor("flag_immune_pos", cfg::esp::flag_immune_pos, { 1, 0.f, 8 });
		load_anchor("flag_kit_pos", cfg::esp::flag_kit_pos, { 1, 0.f, 9 });
		load_anchor("name_pos", cfg::esp::name_pos, { 0, 0.5f, 10 });
		cfg::esp::name_off_x = esp.value("name_off_x", 0.f);
		cfg::esp::name_off_y = esp.value("name_off_y", 0.f);
		load_anchor("weapon_pos", cfg::esp::weapon_pos, { 2, 0.5f, 11 });
		load_anchor("weapon_icon_pos", cfg::esp::weapon_icon_pos, { 2, 0.5f, 14 });
		load_anchor("distance_pos", cfg::esp::distance_pos, { 2, 0.5f, 12 });
		load_anchor("armorbar_pos", cfg::esp::armorbar_pos, { 3, 0.5f, 13 });
		cfg::esp::NormalizeFlag(cfg::esp::flag_c4_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_scoped_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_flashed_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_defusing_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_hk_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_reload_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_immune_pos);
		cfg::esp::NormalizeFlag(cfg::esp::flag_kit_pos);
		cfg::esp::NormalizeFlag(cfg::esp::name_pos);
		if (cfg::esp::flag_order_seq < 10)
			cfg::esp::flag_order_seq = 10;
		cfg::esp::box_visible = JsonToColor(esp, "box_visible",
			JsonToColor(esp, "box_color", { 1.f, 50.f / 255.f, 50.f / 255.f, 1.f }));
		cfg::esp::box_invisible = JsonToColor(esp, "box_invisible", { 1.f, 1.f, 1.f, 0.55f });
		cfg::esp::skeleton_visible = JsonToColor(esp, "skeleton_visible", { 1.f, 50.f / 255.f, 50.f / 255.f, 1.f });
		cfg::esp::skeleton_invisible = JsonToColor(esp, "skeleton_invisible", { 1.f, 1.f, 1.f, 0.55f });
		cfg::esp::healthbar_full = JsonToColor(esp, "healthbar_full", { 40.f / 255.f, 220.f / 255.f, 70.f / 255.f, 1.f });
		cfg::esp::healthbar_low = JsonToColor(esp, "healthbar_low", { 220.f / 255.f, 40.f / 255.f, 40.f / 255.f, 1.f });
		cfg::esp::name_color = JsonToColor(esp, "name_color", { 1.f, 1.f, 1.f, 1.f });

		const json& mate = (esp.contains("teammates") && esp["teammates"].is_object()) ? esp["teammates"] : empty;
		cfg::esp::mate_enabled = mate.value("enabled", false);
		cfg::esp::mate_box = mate.value("box", true);
		cfg::esp::mate_corner_box = mate.value("corner_box", false);
		cfg::esp::mate_name = mate.value("name", false);
		cfg::esp::mate_box_thickness = std::clamp(mate.value("box_thickness", 1.5f), 0.5f, 5.f);
		cfg::esp::mate_skeleton = mate.value("skeleton", false);
		cfg::esp::mate_healthbar = mate.value("healthbar", false);
		cfg::esp::mate_flag_c4 = mate.value("flag_c4", false);
		cfg::esp::mate_flag_scoped = mate.value("flag_scoped", false);
		cfg::esp::mate_flag_flashed = mate.value("flag_flashed", false);
		cfg::esp::mate_flag_defusing = mate.value("flag_defusing", false);
		cfg::esp::mate_box_visible = JsonToColor(mate, "box_visible", { 80.f / 255.f, 200.f / 255.f, 1.f, 1.f });
		cfg::esp::mate_box_invisible = JsonToColor(mate, "box_invisible", { 80.f / 255.f, 200.f / 255.f, 1.f, 0.55f });
		cfg::esp::mate_skeleton_visible = JsonToColor(mate, "skeleton_visible", { 80.f / 255.f, 200.f / 255.f, 1.f, 1.f });
		cfg::esp::mate_skeleton_invisible = JsonToColor(mate, "skeleton_invisible", { 80.f / 255.f, 200.f / 255.f, 1.f, 0.55f });

		const json& radar = (data.contains("radar") && data["radar"].is_object()) ? data["radar"] : empty;
		cfg::radar::enabled = radar.value("enabled", false);
		cfg::radar::show_teammates = radar.value("show_teammates", false);
		cfg::radar::type = std::clamp(radar.value("type", 0), 0, 2);
		cfg::radar::size = std::clamp(radar.value("size", 200.f), 80.f, 480.f);
		cfg::radar::proportion = std::clamp(radar.value("proportion", 2680.f), 400.f, 8000.f);
		cfg::radar::range = std::clamp(radar.value("range", 250.f), 40.f, 400.f);
		cfg::radar::opacity = std::clamp(radar.value("opacity", 0.67f), 0.05f, 1.f);
		cfg::radar::pos_x = radar.value("pos_x", 20.f);
		cfg::radar::pos_y = radar.value("pos_y", 80.f);
		cfg::radar::enemy = JsonToColor(radar, "enemy", { 237.f / 255.f, 85.f / 255.f, 106.f / 255.f, 200.f / 255.f });
		cfg::radar::teammate = JsonToColor(radar, "teammate", { 80.f / 255.f, 200.f / 255.f, 1.f, 200.f / 255.f });

		const json& world = (data.contains("world") && data["world"].is_object()) ? data["world"] : empty;
		cfg::world::items = world.value("items", false);
		cfg::world::item_name = world.value("item_name", true);
		cfg::world::item_icon = world.value("item_icon", true);
		cfg::world::item_ammo = world.value("item_ammo", true);
		cfg::world::item_distance = std::clamp(world.value("item_distance", 40.f), 5.f, 80.f);
		cfg::world::item_rifles = world.value("item_rifles", true);
		cfg::world::item_smgs = world.value("item_smgs", true);
		cfg::world::item_shotguns = world.value("item_shotguns", true);
		cfg::world::item_snipers = world.value("item_snipers", true);
		cfg::world::item_pistols = world.value("item_pistols", true);
		cfg::world::item_heavy = world.value("item_heavy", true);
		cfg::world::item_grenades = world.value("item_grenades", true);
		cfg::world::item_utility = world.value("item_utility", true);
		cfg::world::projectiles = world.value("projectiles", false);
		cfg::world::proj_name = world.value("proj_name", true);
		cfg::world::proj_icon = world.value("proj_icon", true);
		cfg::world::proj_timer = world.value("proj_timer", true);
		cfg::world::inferno_bounds = world.value("inferno_bounds", true);
		cfg::world::grenade_pred = world.value("grenade_pred", false);
		cfg::world::grenade_local_only = world.value("grenade_local_only", true);
		cfg::world::grenade_color = JsonToColor(world, "grenade_color", { 170.f / 255.f, 175.f / 255.f, 220.f / 255.f, 200.f / 255.f });



		cfg::settings::watermark = utils.value("watermark", true);
		cfg::settings::streamproof = utils.value("streamproof", false);
		cfg::settings::vsync = utils.value("vsync", true);
		cfg::settings::free_cpu = utils.value("free_cpu", true);
		cfg::settings::debug_multipoint = utils.value("debug_multipoint", false);
		cfg::settings::auto_accept = utils.value("auto_accept", false);
		cfg::settings::cache_ms = utils.value("cache_ms", 2);
		cfg::settings::menu_color = JsonToColor(utils, "menu_color", { 15.f / 255.f, 15.f / 255.f, 15.f / 255.f, 1.f });
		cfg::settings::menu_bg_opacity = std::clamp(utils.value("menu_bg_opacity", 1.f), 0.f, 1.f);

		//cfg::settings::open_menu_key = data["utils"].value("open_menu_key", 0);
	}
	catch (const std::exception&) {
		LOGF(FATAL, "Failed to parse configuration");
		return false;
	}

	LOGF(INFO, "Successfully parsed configuration");
	return true;
}

bool Config::WriteImpl() {
	EnsureDir();
	return WriteTo(PathFor(current_name));
}

bool Config::WriteTo(const std::filesystem::path& path) {
	std::ofstream f(path);
	if (!f)
		return false;

	json data;
	data["esp"]["box"] = cfg::esp::box;
	data["esp"]["corner_box"] = cfg::esp::corner_box;
	data["esp"]["name"] = cfg::esp::name;
	data["esp"]["box_thickness"] = cfg::esp::box_thickness;
	data["esp"]["skeleton"] = cfg::esp::skeleton;
	data["esp"]["healthbar"] = cfg::esp::healthbar;
	data["esp"]["healthbar_thickness"] = cfg::esp::healthbar_thickness;
	data["esp"]["healthbar_outline"] = cfg::esp::healthbar_outline;
	data["esp"]["healthbar_alpha"] = cfg::esp::healthbar_alpha;
	data["esp"]["armorbar"] = cfg::esp::armorbar;
	data["esp"]["weapon"] = cfg::esp::weapon;
	data["esp"]["weapon_icon"] = cfg::esp::weapon_icon;
	data["esp"]["distance"] = cfg::esp::distance;
	data["esp"]["flag_c4"] = cfg::esp::flag_c4;
	data["esp"]["flag_scoped"] = cfg::esp::flag_scoped;
	data["esp"]["flag_flashed"] = cfg::esp::flag_flashed;
	data["esp"]["flag_defusing"] = cfg::esp::flag_defusing;
	data["esp"]["flag_health"] = cfg::esp::healthbar;
	data["esp"]["flag_hk"] = cfg::esp::flag_hk;
	data["esp"]["flag_reload"] = cfg::esp::flag_reload;
	data["esp"]["flag_immune"] = cfg::esp::flag_immune;
	data["esp"]["flag_kit"] = cfg::esp::flag_kit;
	auto save_anchor = [&](const char* key, const cfg::esp::flag_anchor& a) {
		data["esp"][key]["side"] = a.side;
		data["esp"][key]["along"] = a.along;
		data["esp"][key]["order"] = a.order;
	};
	save_anchor("flag_c4_pos", cfg::esp::flag_c4_pos);
	save_anchor("flag_scoped_pos", cfg::esp::flag_scoped_pos);
	save_anchor("flag_flashed_pos", cfg::esp::flag_flashed_pos);
	save_anchor("flag_defusing_pos", cfg::esp::flag_defusing_pos);
	save_anchor("flag_health_pos", cfg::esp::flag_health_pos);
	save_anchor("flag_hk_pos", cfg::esp::flag_hk_pos);
	save_anchor("flag_reload_pos", cfg::esp::flag_reload_pos);
	save_anchor("flag_immune_pos", cfg::esp::flag_immune_pos);
	save_anchor("flag_kit_pos", cfg::esp::flag_kit_pos);
	save_anchor("name_pos", cfg::esp::name_pos);
	data["esp"]["name_off_x"] = cfg::esp::name_off_x;
	data["esp"]["name_off_y"] = cfg::esp::name_off_y;
	save_anchor("weapon_pos", cfg::esp::weapon_pos);
	save_anchor("weapon_icon_pos", cfg::esp::weapon_icon_pos);
	save_anchor("distance_pos", cfg::esp::distance_pos);
	save_anchor("armorbar_pos", cfg::esp::armorbar_pos);
	ColorToJson(data["esp"], "box_visible", cfg::esp::box_visible);
	ColorToJson(data["esp"], "box_invisible", cfg::esp::box_invisible);
	ColorToJson(data["esp"], "healthbar_full", cfg::esp::healthbar_full);
	ColorToJson(data["esp"], "healthbar_low", cfg::esp::healthbar_low);
	ColorToJson(data["esp"], "skeleton_visible", cfg::esp::skeleton_visible);
	ColorToJson(data["esp"], "skeleton_invisible", cfg::esp::skeleton_invisible);
	ColorToJson(data["esp"], "name_color", cfg::esp::name_color);
	data["esp"]["teammates"]["enabled"] = cfg::esp::mate_enabled;
	data["esp"]["teammates"]["box"] = cfg::esp::mate_box;
	data["esp"]["teammates"]["corner_box"] = cfg::esp::mate_corner_box;
	data["esp"]["teammates"]["name"] = cfg::esp::mate_name;
	data["esp"]["teammates"]["box_thickness"] = cfg::esp::mate_box_thickness;
	data["esp"]["teammates"]["skeleton"] = cfg::esp::mate_skeleton;
	data["esp"]["teammates"]["healthbar"] = cfg::esp::mate_healthbar;
	data["esp"]["teammates"]["flag_c4"] = cfg::esp::mate_flag_c4;
	data["esp"]["teammates"]["flag_scoped"] = cfg::esp::mate_flag_scoped;
	data["esp"]["teammates"]["flag_flashed"] = cfg::esp::mate_flag_flashed;
	data["esp"]["teammates"]["flag_defusing"] = cfg::esp::mate_flag_defusing;
	ColorToJson(data["esp"]["teammates"], "box_visible", cfg::esp::mate_box_visible);
	ColorToJson(data["esp"]["teammates"], "box_invisible", cfg::esp::mate_box_invisible);
	ColorToJson(data["esp"]["teammates"], "skeleton_visible", cfg::esp::mate_skeleton_visible);
	ColorToJson(data["esp"]["teammates"], "skeleton_invisible", cfg::esp::mate_skeleton_invisible);

	data["radar"]["enabled"] = cfg::radar::enabled;
	data["radar"]["show_teammates"] = cfg::radar::show_teammates;
	data["radar"]["type"] = cfg::radar::type;
	data["radar"]["size"] = cfg::radar::size;
	data["radar"]["proportion"] = cfg::radar::proportion;
	data["radar"]["range"] = cfg::radar::range;
	data["radar"]["opacity"] = cfg::radar::opacity;
	data["radar"]["pos_x"] = cfg::radar::pos_x;
	data["radar"]["pos_y"] = cfg::radar::pos_y;
	ColorToJson(data["radar"], "enemy", cfg::radar::enemy);
	ColorToJson(data["radar"], "teammate", cfg::radar::teammate);

	data["world"]["items"] = cfg::world::items;
	data["world"]["item_name"] = cfg::world::item_name;
	data["world"]["item_icon"] = cfg::world::item_icon;
	data["world"]["item_ammo"] = cfg::world::item_ammo;
	data["world"]["item_distance"] = cfg::world::item_distance;
	data["world"]["item_rifles"] = cfg::world::item_rifles;
	data["world"]["item_smgs"] = cfg::world::item_smgs;
	data["world"]["item_shotguns"] = cfg::world::item_shotguns;
	data["world"]["item_snipers"] = cfg::world::item_snipers;
	data["world"]["item_pistols"] = cfg::world::item_pistols;
	data["world"]["item_heavy"] = cfg::world::item_heavy;
	data["world"]["item_grenades"] = cfg::world::item_grenades;
	data["world"]["item_utility"] = cfg::world::item_utility;
	data["world"]["projectiles"] = cfg::world::projectiles;
	data["world"]["proj_name"] = cfg::world::proj_name;
	data["world"]["proj_icon"] = cfg::world::proj_icon;
	data["world"]["proj_timer"] = cfg::world::proj_timer;
	data["world"]["inferno_bounds"] = cfg::world::inferno_bounds;
	data["world"]["grenade_pred"] = cfg::world::grenade_pred;
	data["world"]["grenade_local_only"] = cfg::world::grenade_local_only;
	ColorToJson(data["world"], "grenade_color", cfg::world::grenade_color);



	// utils

	// movement
	data["movement"]["bhop"] = cfg::movement::bhop;
	data["movement"]["strafe_helper"] = cfg::movement::strafe_helper;


	data["aimbot"]["enabled"] = cfg::aimbot::enabled;
	data["aimbot"]["fov"] = cfg::aimbot::fov;
	data["aimbot"]["smooth"] = cfg::aimbot::smooth;
	data["aimbot"]["bone"] = cfg::aimbot::bone;
	data["aimbot"]["draw_fov"] = cfg::aimbot::draw_fov;
	data["aimbot"]["hb_head"] = cfg::aimbot::hb_head;
	data["aimbot"]["hb_chest"] = cfg::aimbot::hb_chest;
	data["aimbot"]["hb_stomach"] = cfg::aimbot::hb_stomach;
	data["aimbot"]["hb_arms"] = cfg::aimbot::hb_arms;
	data["aimbot"]["hb_legs"] = cfg::aimbot::hb_legs;
	data["aimbot"]["hb_nearest"] = cfg::aimbot::hb_nearest;
	data["aimbot"]["multipoints"] = cfg::aimbot::multipoints;
	data["aimbot"]["multipoint_scale"] = cfg::aimbot::multipoint_scale;
	data["aimbot"]["multipoint_count"] = cfg::aimbot::multipoint_count;
	data["aimbot"]["weapon_override"] = cfg::aimbot::weapon_override;
	data["aimbot"]["kill_delay"] = cfg::aimbot::kill_delay;
	data["aimbot"]["kill_delay_ms"] = cfg::aimbot::kill_delay_ms;
	data["humanize"]["enabled"] = cfg::humanize::enabled;
	data["humanize"]["fitts_a"] = cfg::humanize::fitts_a;
	data["humanize"]["fitts_b"] = cfg::humanize::fitts_b;
	data["humanize"]["target_width"] = cfg::humanize::target_width;
	data["humanize"]["overshoot_prob"] = cfg::humanize::overshoot_prob;
	data["humanize"]["curvature_scale"] = cfg::humanize::curvature_scale;
	data["humanize"]["ou_sigma"] = cfg::humanize::ou_sigma;
	data["humanize"]["tremor_amp"] = cfg::humanize::tremor_amp;
	data["humanize"]["sdn_k"] = cfg::humanize::sdn_k;
	data["humanize"]["speed_scale"] = cfg::humanize::speed_scale;
	data["humanize"]["rebuild_on_target_switch"] = cfg::humanize::rebuild_on_target_switch;
	{
		auto arr = json::array();
		for (const auto& entry : cfg::aimbot::kWeapons)
		{
			const auto it = cfg::aimbot::weapons.find(entry.id);
			if (it == cfg::aimbot::weapons.end())
				continue;
			const auto& t = it->second;
			if (!t.override && t.fov == cfg::aimbot::fov && t.smooth == cfg::aimbot::smooth)
				continue;
			arr.push_back({
				{ "id", entry.id },
				{ "override", t.override },
				{ "fov", t.fov },
				{ "smooth", t.smooth },
				{ "hb_nearest", t.hb_nearest },
				{ "hb_head", t.hb_head },
				{ "hb_chest", t.hb_chest },
				{ "hb_stomach", t.hb_stomach },
				{ "hb_arms", t.hb_arms },
				{ "hb_legs", t.hb_legs },
				{ "trg_hit_chance", t.trg_hit_chance },
				{ "trg_reaction_time", t.trg_reaction_time },
				{ "trg_before_shot", t.trg_before_shot },
				{ "trg_between_shots", t.trg_between_shots },
				{ "trg_block_noscope", t.trg_block_noscope },
				{ "trg_hb_head", t.trg_hb_head },
				{ "trg_hb_chest", t.trg_hb_chest },
				{ "trg_hb_stomach", t.trg_hb_stomach },
				{ "trg_hb_arms", t.trg_hb_arms },
				{ "trg_hb_legs", t.trg_hb_legs },
				{ "af_hit_chance", t.af_hit_chance },
				{ "af_reaction_time", t.af_reaction_time },
				{ "af_before_shot", t.af_before_shot },
				{ "af_between_shots", t.af_between_shots },
				{ "af_block_noscope", t.af_block_noscope },
				{ "af_autoscope", t.af_autoscope }
			});
		}
		data["aimbot"]["weapons"] = arr;
	}

	data["triggerbot"]["enabled"] = cfg::triggerbot::enabled;
	data["triggerbot"]["hit_chance"] = cfg::triggerbot::hit_chance;
	data["triggerbot"]["reaction_time"] = cfg::triggerbot::reaction_time;
	data["triggerbot"]["before_shot_delay"] = cfg::triggerbot::before_shot_delay;
	data["triggerbot"]["between_shots"] = cfg::triggerbot::between_shots;
	data["triggerbot"]["block_noscope"] = cfg::triggerbot::block_noscope;
	data["triggerbot"]["hb_head"] = cfg::triggerbot::hb_head;
	data["triggerbot"]["hb_chest"] = cfg::triggerbot::hb_chest;
	data["triggerbot"]["hb_stomach"] = cfg::triggerbot::hb_stomach;
	data["triggerbot"]["hb_arms"] = cfg::triggerbot::hb_arms;
	data["triggerbot"]["hb_legs"] = cfg::triggerbot::hb_legs;

	data["autofire"]["enabled"] = cfg::autofire::enabled;
	data["autofire"]["early_stop"] = cfg::autofire::early_stop;
	data["autofire"]["autoscope"] = cfg::autofire::autoscope;
	data["autofire"]["block_noscope"] = cfg::autofire::block_noscope;
	data["autofire"]["reaction_time"] = cfg::autofire::reaction_time;
	data["autofire"]["before_shot_delay"] = cfg::autofire::before_shot_delay;
	data["autofire"]["between_shots"] = cfg::autofire::between_shots;
	data["autofire"]["hit_chance"] = cfg::autofire::hit_chance;

	data["rcs"]["enabled"] = cfg::rcs::enabled;
	data["rcs"]["smooth"] = cfg::rcs::smooth;
	data["rcs"]["strength_y"] = cfg::rcs::strength_y;
	data["rcs"]["strength_x"] = cfg::rcs::strength_x;
	data["rcs"]["reaction_ms"] = cfg::rcs::reaction_ms;


	//data["utils"]["console"] = cfg::settings::console;
	data["utils"]["watermark"] = cfg::settings::watermark;
	data["utils"]["streamproof"] = cfg::settings::streamproof;
	data["utils"]["vsync"] = cfg::settings::vsync;
	data["utils"]["free_cpu"] = cfg::settings::free_cpu;
	data["utils"]["debug_multipoint"] = cfg::settings::debug_multipoint;
	data["utils"]["auto_accept"] = cfg::settings::auto_accept;
	data["utils"]["cache_ms"] = cfg::settings::cache_ms;
	ColorToJson(data["utils"], "menu_color", cfg::settings::menu_color);
	data["utils"]["menu_bg_opacity"] = cfg::settings::menu_bg_opacity;

	//data["utils"]["open_menu_key"] = cfg::settings::open_menu_key;

	f << std::setw(4) << data << std::endl;
	if (!f)
		return false;

	LOGF(VERBOSE, "Writting configuration to file");

	return true;
}


// TODO: Refactor this
color_t Config::JsonToColor(const json& parent, const std::string& key, const color_t& def) {
	if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() != 4)
		return def;
	return color_t(
		parent[key][0].get<float>(),
		parent[key][1].get<float>(),
		parent[key][2].get<float>(),
		parent[key][3].get<float>()
	);
}

void Config::ColorToJson(json& parent, const std::string& key, const color_t& color) {
	parent[key] = { color.r, color.g, color.b, color.a };
}

Vec2_t Config::JsonToVec2(const json& parent, const std::string& key, const Vec2_t& def)
{
	if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() != 2)
		return def;

	return Vec2_t{
		parent[key][0].get<float>(),
		parent[key][1].get<float>()
	};
}

void Config::Vec2ToJson(json& parent, const std::string& key, const Vec2_t& vec)
{
	parent[key] = { vec.x, vec.y };
}