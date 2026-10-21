#include "Globals.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"

bool Globals::Update() {
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();

	if (!p)
		return false;

	this->address = p->read<uintptr_t>(client.base + offsets::globalVars);

	if (!this->address)
		return false;

	this->current_time = p->read<long>(this->address + offsets::global::currentTime);

	this->max_clients = p->read<int>(this->address + offsets::global::maxClients);
	this->in_match = this->max_clients > 1; // In the lobby, it seems that it still counts one

	if (!this->in_match) {
		this->is_deathmatch = false;
	} else if (offsets::gameRules) {
		const auto rules = p->read<uintptr_t>(client.base + offsets::gameRules);
		bool dm = false;
		if (rules) {
			const bool has_bomb = p->read<bool>(rules + offsets::rules::m_bMapHasBombTarget);
			const bool has_hostage = p->read<bool>(rules + offsets::rules::m_bMapHasRescueZone);
			const int freeze = p->read<int>(rules + offsets::rules::m_iFreezeTime);
			const bool ffa_rules = !has_bomb && !has_hostage && freeze <= 1;

			bool dm_bonus = false;
			const auto mode_rules = p->read<uintptr_t>(rules + offsets::rules::m_pGameModeRules);
			if (mode_rules) {
				const auto bonus_len = p->read<float>(mode_rules + offsets::rules::m_flDMBonusTimeLength);
				dm_bonus = bonus_len >= 5.f && bonus_len <= 180.f;
			}

			dm = ffa_rules || dm_bonus;
		}
		this->is_deathmatch = dm;
	}

	auto map_name_addr = p->read<DWORD64>(this->address + offsets::global::currentMapName);
	if (!map_name_addr || !p->read_raw(map_name_addr, this->map_name, sizeof(this->map_name)))
		this->map_name[0] = '\0';
	else
		this->map_name[sizeof(this->map_name) - 1] = '\0';
	return true;
}

