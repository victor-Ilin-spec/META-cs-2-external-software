#pragma once

#include "core/engine/Engine.hpp"
#include "core/offsets/Offsets.hpp"
#include "core/engine/classes/Bones.hpp"

#include <cstring>
#include <vector>

struct EntityBuffer {
    uintptr_t base_address = 0;
    std::vector<uint8_t> buffer;

    bool update(uintptr_t address, size_t size)
    {
        if (!address || !size)
            return false;

        auto process = Engine::GetProcess();
        if (!process || !process->handle_)
            return false;

        base_address = address;
        if (buffer.size() != size)
            buffer.resize(size);

        return process->read_raw(address, buffer.data(), size);
    }

    template <typename T>
    T get(size_t offset) const
    {
        T value{};
        if (offset + sizeof(T) > buffer.size())
            return value;
        std::memcpy(&value, buffer.data() + offset, sizeof(T));
        return value;
    }

    void invalidate()
    {
        base_address = 0;
    }
};

struct CachedPlayer {
    static constexpr size_t kControllerBytes = 0x900;
    static constexpr size_t kPawnBytes = 0x3380;
    static constexpr size_t kBoneBytes = 30 * sizeof(bone_data);

    uintptr_t controller = 0;
    uintptr_t pawn = 0;
    uintptr_t resolved_list = 0;
    uint32_t pawn_handle = 0;

    EntityBuffer controller_buf;
    EntityBuffer pawn_buf;
    EntityBuffer bone_buf;

    bool valid = false;
    bool alive = false;

    bool update(uintptr_t controller_ptr, uintptr_t entity_list)
    {
        valid = false;
        alive = false;

        if (!controller_ptr || !entity_list)
        {
            controller = 0;
            pawn = 0;
            pawn_handle = 0;
            return false;
        }

        auto process = Engine::GetProcess();
        if (!process)
            return false;

        controller = controller_ptr;
        if (!controller_buf.update(controller, kControllerBytes))
            return false;

        const auto handle = controller_buf.get<uint32_t>(offsets::controller::m_hPawn);
        if (!handle)
        {
            pawn = 0;
            pawn_handle = 0;
            return false;
        }

        if (handle != pawn_handle || !pawn || resolved_list != entity_list)
        {
            pawn_handle = handle;
            resolved_list = entity_list;
            const auto list_entry = process->read<uintptr_t>(
                entity_list + 0x10 + 0x8 * ((pawn_handle & 0x7FFF) >> 9));
            if (!list_entry)
            {
                pawn = 0;
                return false;
            }
            pawn = process->read<uintptr_t>(list_entry + 0x70 * (pawn_handle & 0x1FF));
            if (!pawn)
                return false;
        }

        if (!pawn_buf.update(pawn, kPawnBytes))
            return false;

        const auto scene_node = pawn_buf.get<uintptr_t>(offsets::pawn::m_pGameSceneNode);
        if (scene_node)
        {
            const auto bone_ptr = process->read<uintptr_t>(
                scene_node + offsets::bone::m_modelState + 0x80);
            if (bone_ptr)
                bone_buf.update(bone_ptr, kBoneBytes);
        }

        valid = true;
        const int hp = health();
        alive = hp > 0 && hp <= 255;
        return true;
    }

    int health() const { return pawn_buf.get<int>(offsets::pawn::m_iHealth); }
    uint8_t team() const { return pawn_buf.get<uint8_t>(offsets::pawn::m_iTeamNum); }
    bool scoped() const { return pawn_buf.get<bool>(offsets::pawn::m_bIsScoped); }
    uint32_t flags() const { return pawn_buf.get<uint32_t>(offsets::pawn::m_fFlags); }
    Vec3_t old_origin() const { return pawn_buf.get<Vec3_t>(offsets::pawn::m_vOldOrigin); }
    Vec3_t velocity() const { return pawn_buf.get<Vec3_t>(offsets::pawn::m_vecAbsVelocity); }
    Vec3_t eye_angles() const { return pawn_buf.get<Vec3_t>(offsets::pawn::m_angEyeAngles); }
    int armor() const { return pawn_buf.get<int>(offsets::pawn::m_ArmorValue); }
    bool defusing() const { return pawn_buf.get<bool>(offsets::pawn::m_bIsDefusing); }
    float flash() const { return pawn_buf.get<float>(offsets::pawn::m_flFlashOverlayAlpha); }
    float flash_duration() const { return pawn_buf.get<float>(offsets::pawn::m_flFlashDuration); }
    bool immune() const { return pawn_buf.get<bool>(offsets::pawn::m_bGunGameImmunity); }
    uintptr_t item_services() const { return pawn_buf.get<uintptr_t>(offsets::pawn::m_pItemServices); }

    Vec3_t bone_pos(int id) const
    {
        return bone_buf.get<Vec3_t>(static_cast<size_t>(id) * sizeof(bone_data));
    }
};
