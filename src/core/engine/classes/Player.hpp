#pragma once
#include "core/engine/classes/Bones.hpp"
#include "core/engine/classes/Weapon.hpp"
#include "core/engine/classes/ObserverServices.hpp"
#include "core/engine/cache/EntityBuffer.hpp"

class Player {
public:
    Player() {}
    Player(int index, uintptr_t el, uintptr_t le) 
        : index(index), entity_list(el), list_entry(le){}

    bool Update();
    void Bind(int index, uintptr_t el, uintptr_t le);
    void Invalidate();
    void CopyStateFrom(const Player& src);
    bool GetBounds(view_matrix_t matrix, Vec2_t size, std::pair<Vec2_t, Vec2_t>& bounds);
    uintptr_t PawnAddress() const { return pawn; }
    uintptr_t ControllerAddress() const { return controller; }
    bool SameEntity(const Player& other) const
    {
        if (other.index >= 0 && index == other.index)
            return true;
        if (other.pawn && pawn && other.pawn == pawn)
            return true;
        if (other.controller && controller && other.controller == controller)
            return true;
        return false;
    }
    void RefreshSpotted(int local_index, int local_handle);
    void RefreshVisible(const Player& local);
    Vec3_t EyePos() const;
    Vec3_t HeadPos() const;
    Vec3_t ChestPos() const;
public:
    int8_t index = -1; // To use as invalid/un-initialize check

    Vec3_t pos;
    Vec3_t vel;
    Vec3_t eye_angles;
    uint32_t flags = 0;

    int ping = 0;
    int team = 0;
    int health = 0;
    int armor = 0;
    int money = 0;

    bool bot = true;
    bool alive = false;
    bool scoped = false;
    bool flashed = false;
    bool spotted = false;
    bool visible = false;
    bool can_hit = false;
    bool defusing = false;
    bool localplayer = false;
    bool has_c4 = false;
    bool has_helmet = false;
    bool has_defuser = false;
    bool immune = false;

    char name[32]{};
    uint64_t steam_id{};

    Weapon weapon;
    int32_t ammo = 0;
    bool is_reloading = false;

    std::vector<bone_pos> bone_list;


    int pawn_controller_addr = 0;
    ObserverServices observer_services;
private:
    uintptr_t list_entry = 0;
    uintptr_t entity_list = 0;

    uintptr_t pawn = 0;
    uintptr_t controller = 0;
    
    bone_data bones[30]{};
    CachedPlayer cached;
private:
    bool GetPawn();
    bool GetController();

    bool UpdatePawn();
    bool UpdateWeapon();
    bool UpdateSkeleton();
    bool UpdateController();
    bool UpdateObserverServices();
};