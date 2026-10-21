#pragma once
#include "core/engine/types/Weapons.hpp"
#include <algorithm>
#include <unordered_map>

namespace cfg::aimbot {
	struct WeaponTune {
		bool override = false;
		float fov = 4.f;
		float smooth = 4.f;
		bool hb_nearest = false;
		bool hb_head = true;
		bool hb_chest = false;
		bool hb_stomach = false;
		bool hb_arms = false;
		bool hb_legs = false;

		int trg_hit_chance = 0;
		int trg_reaction_time = 0;
		int trg_before_shot = 0;
		int trg_between_shots = 20;
		bool trg_block_noscope = true;
		bool trg_hb_head = true;
		bool trg_hb_chest = true;
		bool trg_hb_stomach = false;
		bool trg_hb_arms = false;
		bool trg_hb_legs = false;

		int af_hit_chance = 70;
		int af_reaction_time = 100;
		int af_before_shot = 0;
		int af_between_shots = 45;
		bool af_block_noscope = true;
		bool af_autoscope = false;
	};

	struct WeaponEntry {
		short id;
		const char* name;
		const char* group;
	};

	inline constexpr WeaponEntry kWeapons[] = {
		{ weapon_glock, "Glock-18", "Pistols" },
		{ weapon_usp_silencer, "USP-S", "Pistols" },
		{ weapon_hkp2000, "P2000", "Pistols" },
		{ weapon_p250, "P250", "Pistols" },
		{ weapon_tec9, "Tec-9", "Pistols" },
		{ weapon_cz75a, "CZ75-Auto", "Pistols" },
		{ weapon_fiveseven, "Five-SeveN", "Pistols" },
		{ weapon_deagle, "Desert Eagle", "Pistols" },
		{ weapon_elite, "Dual Berettas", "Pistols" },
		{ weapon_revolver, "R8 Revolver", "Pistols" },

		{ weapon_mp5sd, "MP5-SD", "SMGs" },
		{ weapon_mp7, "MP7", "SMGs" },
		{ weapon_mp9, "MP9", "SMGs" },
		{ weapon_mac10, "MAC-10", "SMGs" },
		{ weapon_ump45, "UMP-45", "SMGs" },
		{ weapon_bizon, "PP-Bizon", "SMGs" },
		{ weapon_p90, "P90", "SMGs" },

		{ weapon_ak47, "AK-47", "Rifles" },
		{ weapon_aug, "AUG", "Rifles" },
		{ weapon_famas, "FAMAS", "Rifles" },
		{ weapon_galilar, "Galil AR", "Rifles" },
		{ weapon_m4a1_silencer, "M4A1-S", "Rifles" },
		{ weapon_m4a1, "M4A4", "Rifles" },
		{ weapon_sg556, "SG 553", "Rifles" },

		{ weapon_awp, "AWP", "Snipers" },
		{ weapon_g3sg1, "G3SG1", "Snipers" },
		{ weapon_scar20, "SCAR-20", "Snipers" },
		{ weapon_ssg08, "SSG 08", "Snipers" },

		{ weapon_sawedoff, "Sawed-Off", "Shotguns" },
		{ weapon_xm1014, "XM1014", "Shotguns" },
		{ weapon_mag7, "MAG-7", "Shotguns" },
		{ weapon_nova, "Nova", "Shotguns" },

		{ weapon_m249, "M249", "Heavy" },
		{ weapon_negev, "Negev", "Heavy" },
	};

	inline constexpr int kWeaponCount = (int)(sizeof(kWeapons) / sizeof(kWeapons[0]));

	inline std::unordered_map<short, WeaponTune> weapons;

	inline WeaponTune& Tune(short id)
	{
		auto it = weapons.find(id);
		if (it == weapons.end())
			it = weapons.emplace(id, WeaponTune{}).first;
		return it->second;
	}

	inline int IndexOf(short id)
	{
		for (int i = 0; i < kWeaponCount; ++i)
		{
			if (kWeapons[i].id == id)
				return i;
		}
		return -1;
	}

	inline float ActiveFov(short id)
	{
		if (!cfg::aimbot::weapon_override)
			return cfg::aimbot::fov;
		const auto it = weapons.find(id);
		if (it != weapons.end() && it->second.override)
			return it->second.fov;
		return cfg::aimbot::fov;
	}

	inline float ActiveSmooth(short id)
	{
		float s = cfg::aimbot::smooth;
		if (cfg::aimbot::weapon_override)
		{
			const auto it = weapons.find(id);
			if (it != weapons.end() && it->second.override)
				s = it->second.smooth;
		}
		return std::clamp(s, 1.f, 12.f);
	}

	inline const WeaponTune* ActiveTune(short id)
	{
		if (!cfg::aimbot::weapon_override)
			return nullptr;
		const auto it = weapons.find(id);
		if (it == weapons.end() || !it->second.override)
			return nullptr;
		return &it->second;
	}


	inline bool IsScopeWeapon(short id)
	{
		return id == weapon_awp
			|| id == weapon_ssg08
			|| id == weapon_scar20
			|| id == weapon_g3sg1
			|| id == weapon_aug
			|| id == weapon_sg556;
	}

	inline bool ActiveAutoscope(short id)
	{
		if (!IsScopeWeapon(id))
			return false;
		const auto it = weapons.find(id);
		if (it != weapons.end())
			return it->second.af_autoscope;
		return cfg::autofire::autoscope;
	}
}
