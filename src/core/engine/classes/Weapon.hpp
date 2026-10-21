#pragma once
#include "core/engine/types/Weapons.hpp"

class Weapon
{
public:
	Weapon(uintptr_t entity_list, int slot_index)
		: entity_list(entity_list), slot_index(slot_index) {}
	Weapon() 
		: item_index(-1), name("Invalid"), icon("?"), slot_index(0), entity_list(0) { }

	bool Update();
	short item_index;
	std::string name;
	const char* icon;
	int32_t ammo = 0;
	bool is_reloading = false;
	int zoom_level = 0;


	static const char* NameFromIndex(short id);
	static const char* IconFromIndex(short id);

private:
	const char* ToString() const;
	const char* ToIcon() const;

	int slot_index;
	uintptr_t entity_list;
};

