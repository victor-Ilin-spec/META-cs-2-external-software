#include "Player.hpp"

#include "Weapon.hpp"
#include "core/engine/Engine.hpp"
#include "core/offsets/Dumper.hpp"
#include "core/engine/cache/Cache.hpp"

#include "core/engine/classes/ObserverServices.hpp"
#include "core/engine/vischeck/MapVisibility.hpp"

#include <cmath>
#include <cstring>

bool Player::Update() {
	if (!Engine::GetProcess())
		return false;

	if (!GetController())
		return false;

	if (!cached.update(controller, entity_list))
		return false;

	pawn = cached.pawn;
	pawn_controller_addr = static_cast<int>(cached.pawn_handle);

	if (!UpdateController())
		return false;

	if (!UpdatePawn())
		return false;

	return true;
}

void Player::Bind(int idx, uintptr_t el, uintptr_t le)
{
	index = static_cast<int8_t>(idx);
	entity_list = el;
	list_entry = le;
}

void Player::Invalidate()
{
	alive = false;
	localplayer = false;
	spotted = false;
	visible = false;
	can_hit = false;
	pawn = 0;
	controller = 0;
	pawn_controller_addr = 0;
	bone_list.clear();
	cached.valid = false;
	cached.alive = false;
	cached.pawn = 0;
	cached.controller = 0;
	cached.pawn_handle = 0;
	cached.resolved_list = 0;
}

void Player::CopyStateFrom(const Player& src)
{
	if (this == &src)
		return;
	index = src.index;
	pos = src.pos;
	vel = src.vel;
	eye_angles = src.eye_angles;
	flags = src.flags;
	ping = src.ping;
	team = src.team;
	health = src.health;
	armor = src.armor;
	money = src.money;
	bot = src.bot;
	alive = src.alive;
	scoped = src.scoped;
	flashed = src.flashed;
	spotted = src.spotted;
	visible = src.visible;
	can_hit = src.can_hit;
	defusing = src.defusing;
	localplayer = src.localplayer;
	has_c4 = src.has_c4;
	has_helmet = src.has_helmet;
	has_defuser = src.has_defuser;
	immune = src.immune;
	std::memcpy(name, src.name, sizeof(name));
	steam_id = src.steam_id;
	weapon = src.weapon;
	ammo = src.ammo;
	is_reloading = src.is_reloading;
	bone_list = src.bone_list;
	pawn_controller_addr = src.pawn_controller_addr;
	observer_services = src.observer_services;
	list_entry = src.list_entry;
	entity_list = src.entity_list;
	pawn = src.pawn;
	controller = src.controller;
	std::memcpy(bones, src.bones, sizeof(bones));
}

bool Player::GetController() {
	if (index >= 0 && index < 64)
	{
		const auto ctrl = Cache::Get().game.controllers[index + 1];
		if (ctrl)
		{
			this->controller = ctrl;
			return true;
		}
	}

	auto p = Engine::GetProcess();
	if (!p || !list_entry)
		return false;
	this->controller = p->read<DWORD64>(list_entry + (index + 1) * 0x70);
	return this->controller != 0;
}

bool Player::GetPawn() {
	return cached.valid && cached.pawn != 0;
}

bool Player::UpdateController() {
	this->steam_id = cached.controller_buf.get<uint64_t>(offsets::controller::m_steamID);
	this->bot = this->steam_id == 0;

	const auto name_off = static_cast<size_t>(offsets::controller::m_iszPlayerName);
	if (name_off + sizeof(this->name) > cached.controller_buf.buffer.size())
		return false;
	std::memcpy(this->name, cached.controller_buf.buffer.data() + name_off, sizeof(this->name));
	this->name[sizeof(this->name) - 1] = '\0';

	this->localplayer = cached.controller_buf.get<bool>(offsets::controller::m_bIsLocalPlayerController);
	this->ping = cached.controller_buf.get<int>(offsets::controller::m_iPing);
	this->team = cached.controller_buf.get<uint8_t>(offsets::controller::m_iTeamNum);

	// money/observer — отдельные RPM, ESP их не рисует. Не зовём каждый тик.

	return true;
}

bool Player::UpdatePawn() {
	this->health = cached.health();
	this->alive = cached.alive;

	if (this->health > 255 || this->health < 0)
		LOGF(FATAL,
			"Health seems to have a random value (over 100 or under 0) with a value of ({}). Game has probably updated pawn structure",
			this->health
		);

	// UpdateObserverServices();

	if (!alive)
		return true;

	this->pos = cached.old_origin();
	if (this->pos.zero())
		return false;

	this->vel = cached.velocity();
	this->team = cached.team();
	this->armor = cached.armor();
	this->defusing = cached.defusing();
	this->flashed = cached.flash() > 0.f || cached.flash_duration() > 0.f;
	this->scoped = cached.scoped();
	this->eye_angles = cached.eye_angles();
	this->flags = cached.flags();
	this->immune = cached.immune();
	this->has_helmet = false;
	this->has_defuser = false;
	if (auto p = Engine::GetProcess())
	{
		const auto items = cached.item_services();
		if (items)
		{
			this->has_defuser = p->read<bool>(items + offsets::pawn::m_bHasDefuser);
			this->has_helmet = p->read<bool>(items + offsets::pawn::m_bHasHelmet);
		}
	}

	if (!UpdateSkeleton())
		bone_list.clear();

	UpdateWeapon();
	return true;
}

void Player::RefreshSpotted(int local_index, int local_handle)
{
	if (!pawn)
	{
		spotted = false;
		return;
	}

	const auto spotted_off = static_cast<size_t>(
		offsets::pawn::m_entitySpottedState + offsets::pawn::m_bSpottedByMask);
	uint32_t words[2]{};
	if (spotted_off + sizeof(words) <= cached.pawn_buf.buffer.size())
		std::memcpy(words, cached.pawn_buf.buffer.data() + spotted_off, sizeof(words));

	auto bit_set = [&](int idx) {
		if (idx < 0)
			return false;
		return (words[(idx / 32) & 1] & (1u << (idx % 32))) != 0;
	};

	const int handle_idx = local_handle & 0x7FFF;
	spotted = bit_set(local_index) ||
		bit_set(local_index + 1) ||
		bit_set(handle_idx) ||
		bit_set(handle_idx - 1);
}

Vec3_t Player::EyePos() const
{
	constexpr uint32_t kDucking = 1u << 1;
	if (bone_list.size() > bone_index::head)
	{
		const Vec3_t& h = bone_list[bone_index::head].pos;
		if (!h.zero())
			return { h.x, h.y, h.z - 8.f };
	}
	Vec3_t eye = pos;
	eye.z += (flags & kDucking) ? 46.f : 64.f;
	return eye;
}
Vec3_t Player::HeadPos() const
{
	if (bone_list.size() > bone_index::head)
	{
		const Vec3_t& h = bone_list[bone_index::head].pos;
		if (!h.zero())
			return h;
	}
	Vec3_t head = pos;
	head.z += (flags & (1u << 1)) ? 46.f : 64.f;
	return head;
}

Vec3_t Player::ChestPos() const
{
	if (bone_list.size() > bone_index::chest)
	{
		const Vec3_t& c = bone_list[bone_index::chest].pos;
		if (!c.zero())
			return c;
	}
	if (bone_list.size() > bone_index::spine_2)
	{
		const Vec3_t& s = bone_list[bone_index::spine_2].pos;
		if (!s.zero())
			return s;
	}
	Vec3_t chest = pos;
	chest.z += (flags & (1u << 1)) ? 34.f : 48.f;
	return chest;
}

void Player::RefreshVisible(const Player& local)
{
	visible = false;
	can_hit = false;
	if (!alive || localplayer)
		return;
	auto& vis = MapVisibility::Get();
	if (vis.Loaded())
	{
		visible = vis.IsVisible(local.EyePos(), HeadPos());
		can_hit = visible;
		return;
	}
	visible = spotted;
	can_hit = spotted;
}


bool Player::UpdateSkeleton() {
	if (cached.bone_buf.buffer.size() < 30 * sizeof(bone_data))
		return false;

	std::memcpy(bones, cached.bone_buf.buffer.data(), sizeof(bones));
	bone_list.resize(30);
	for (int i = 0; i < 30; i++)
		bone_list[i] = { bones[i].pos, bones[i].qx, bones[i].qy, bones[i].qz, bones[i].qw };
	return true;
}

bool Player::UpdateWeapon() {
	auto p = Engine::GetProcess();

	auto weapon_services = cached.pawn_buf.get<uintptr_t>(offsets::pawn::m_pWeaponServices);
	if (!weapon_services)
		return false;

	auto active_weapon_index = p->read<int>(weapon_services + offsets::pawn::m_hActiveWeapon);
	if (!active_weapon_index)
		return false;

	auto weapon = Weapon(this->entity_list, active_weapon_index);
	if (!weapon.Update())
		return false;

	this->weapon = weapon;
	this->ammo = weapon.ammo;
	this->is_reloading = weapon.is_reloading;
	return true;
}

bool Player::GetBounds(view_matrix_t matrix, Vec2_t size, std::pair<Vec2_t, Vec2_t>& bounds) {
	Vec2_t origin;
	bool pt1 = matrix.wts(this->pos, size, origin);


	Vec3_t pos_top;
	if (this->bone_list.empty())
		pos_top = this->pos + Vec3_t(0, 0, 65.f); // 75.f
	else
		pos_top = this->bone_list[bone_index::head].pos;

	//auto head_bone = this->bone_list[bone_index::head];
	//head_bone.pos.z *= 1.09; // little offset to cover the entire head
	//bone_pos head_bone = origin + ImVec3

	Vec2_t top;
	bool pt2 = matrix.wts(pos_top, size, top);

	float height = origin.y - top.y;
	float width = height / 2.4f;

	top.x -= width / 2;
	origin.x += width / 2;

	top.y -= width / 4;

	// Top to bottom
	bounds = { top, origin };

	return pt1 || pt2;
}

// Does not update if match is started
bool Player::UpdateObserverServices() {
	auto p = Engine::GetProcess();
	if (!p) 
		return false;

	DWORD64 address = cached.pawn_buf.get<DWORD64>(offsets::pawn::m_pObserverServices);
	if (!address) 
		return false;

	this->observer_services.SetAddress(address);
	return this->observer_services.Update();
}