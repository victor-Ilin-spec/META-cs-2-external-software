#include "Triggerbot.hpp"

#include "core/engine/Engine.hpp"
#include "core/engine/cache/Cache.hpp"
#include "core/engine/types/Weapons.hpp"
#include "core/offsets/Offsets.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <vector>

namespace {
    constexpr float kEyeHeight = 64.f;
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kDegToRad = kPi / 180.f;

    constexpr float kRadHead = 3.6f;
    constexpr float kRadNeck = 3.8f;
    constexpr float kRadChest = 7.0f;
    constexpr float kRadSpine = 7.4f;
    constexpr float kRadStomach = 7.0f;
    constexpr float kRadPelvis = 6.6f;
    constexpr float kRadUpperArm = 3.8f;
    constexpr float kRadForearm = 3.2f;
    constexpr float kRadThigh = 5.0f;
    constexpr float kRadCalf = 4.2f;

    struct HitCapsule {
        Vec3_t a{};
        Vec3_t b{};
        float radius = 4.3f;
    };

    bool IsSniper(short id)
    {
        return id == weapon_awp || id == weapon_ssg08 ||
               id == weapon_scar20 || id == weapon_g3sg1;
    }

    uintptr_t ResolveHandle(const std::shared_ptr<pProcess>& process, uintptr_t entity_list, uint32_t handle)
    {
        if (!process || !entity_list || !handle)
            return 0;

        const auto bucket = process->read<uintptr_t>(
            entity_list + 0x10 + 0x8 * ((handle & 0x7FFF) >> 9));
        if (!bucket)
            return 0;

        return process->read<uintptr_t>(bucket + 0x70 * (handle & 0x1FF));
    }

    void AngleVectors(const Vec3_t& angles, Vec3_t& forward, Vec3_t& right, Vec3_t& up)
    {
        const float pitch = angles.x * kDegToRad;
        const float yaw = angles.y * kDegToRad;

        const float sp = std::sin(pitch);
        const float cp = std::cos(pitch);
        const float sy = std::sin(yaw);
        const float cy = std::cos(yaw);

        forward = { cp * cy, cp * sy, -sp };
        right = { -sy, cy, 0.f };
        up = { sp * cy, sp * sy, cp };
    }

    bool BoneAt(const Player& player, int index, Vec3_t& out)
    {
        if (index < 0 || player.bone_list.size() <= static_cast<size_t>(index))
            return false;
        out = player.bone_list[index].pos;
        return true;
    }

    void AddCapsule(std::vector<HitCapsule>& out, const Vec3_t& a, const Vec3_t& b, float radius)
    {
        out.push_back({ a, b, radius });
    }

    void AddSphere(std::vector<HitCapsule>& out, const Vec3_t& p, float radius)
    {
        out.push_back({ p, p, radius });
    }

    struct TriggerSettings {
        bool hb_head = true;
        bool hb_chest = true;
        bool hb_stomach = false;
        bool hb_arms = false;
        bool hb_legs = false;
        int hit_chance = 0;
        int reaction_time = 0;
        int before_shot = 0;
        int between_shots = 20;
        bool block_noscope = true;
    };

    TriggerSettings ResolveTrigger(short id)
    {
        TriggerSettings s{
            cfg::triggerbot::hb_head,
            cfg::triggerbot::hb_chest,
            cfg::triggerbot::hb_stomach,
            cfg::triggerbot::hb_arms,
            cfg::triggerbot::hb_legs,
            cfg::triggerbot::hit_chance,
            cfg::triggerbot::reaction_time,
            cfg::triggerbot::before_shot_delay,
            cfg::triggerbot::between_shots,
            cfg::triggerbot::block_noscope
        };
        if (const auto* tune = cfg::aimbot::ActiveTune(id))
        {
            s.hb_head = tune->trg_hb_head;
            s.hb_chest = tune->trg_hb_chest;
            s.hb_stomach = tune->trg_hb_stomach;
            s.hb_arms = tune->trg_hb_arms;
            s.hb_legs = tune->trg_hb_legs;
            s.hit_chance = tune->trg_hit_chance;
            s.reaction_time = tune->trg_reaction_time;
            s.before_shot = tune->trg_before_shot;
            s.between_shots = tune->trg_between_shots;
            s.block_noscope = tune->trg_block_noscope;
        }
        return s;
    }

    std::vector<HitCapsule> BuildHitboxes(const Player& player, const TriggerSettings& s)
    {
        std::vector<HitCapsule> boxes;
        Vec3_t head, neck, chest, spine2, spine1, spine0, pelvis;
        const bool has_head = BoneAt(player, bone_index::head, head);
        const bool has_neck = BoneAt(player, bone_index::neck, neck);
        const bool has_chest = BoneAt(player, bone_index::chest, chest);
        const bool has_spine2 = BoneAt(player, bone_index::spine_2, spine2);
        const bool has_spine1 = BoneAt(player, bone_index::spine_1, spine1);
        const bool has_spine0 = BoneAt(player, bone_index::spine_0, spine0);
        const bool has_pelvis = BoneAt(player, bone_index::pelvis, pelvis);

        if (s.hb_head)
        {
            if (has_head)
                AddSphere(boxes, head, kRadHead);
        }

        if (s.hb_chest)
        {
            if (has_chest && has_neck)
                AddCapsule(boxes, chest, neck, kRadChest);
            else if (has_chest)
                AddSphere(boxes, chest, kRadChest);
            if (has_chest && has_spine2)
                AddCapsule(boxes, spine2, chest, kRadSpine);
            else if (has_spine2)
                AddSphere(boxes, spine2, kRadSpine);
        }

        if (s.hb_stomach)
        {
            if (has_pelvis && has_spine1)
                AddCapsule(boxes, pelvis, spine1, kRadStomach);
            else if (has_pelvis && has_spine0)
                AddCapsule(boxes, pelvis, spine0, kRadStomach);
            else if (has_pelvis)
                AddSphere(boxes, pelvis, kRadPelvis);
            if (has_spine0 && has_spine1)
                AddCapsule(boxes, spine0, spine1, kRadStomach);
            if (has_spine1 && has_spine2)
                AddCapsule(boxes, spine1, spine2, kRadSpine);
        }

        if (s.hb_arms)
        {
            Vec3_t sl, el, hl, sr, er, hr;
            if (BoneAt(player, bone_index::shoulder_L, sl) && BoneAt(player, bone_index::elbow_L, el))
                AddCapsule(boxes, sl, el, kRadUpperArm);
            if (BoneAt(player, bone_index::elbow_L, el) && BoneAt(player, bone_index::hand_L, hl))
                AddCapsule(boxes, el, hl, kRadForearm);
            if (BoneAt(player, bone_index::shoulder_R, sr) && BoneAt(player, bone_index::elbow_R, er))
                AddCapsule(boxes, sr, er, kRadUpperArm);
            if (BoneAt(player, bone_index::elbow_R, er) && BoneAt(player, bone_index::hand_R, hr))
                AddCapsule(boxes, er, hr, kRadForearm);
        }

        if (s.hb_legs)
        {
            Vec3_t hip, knee, foot;
            if (BoneAt(player, bone_index::hip_L, hip) && BoneAt(player, bone_index::knee_L, knee))
                AddCapsule(boxes, hip, knee, kRadThigh);
            if (BoneAt(player, bone_index::knee_L, knee) && BoneAt(player, bone_index::foot_heel_L, foot))
                AddCapsule(boxes, knee, foot, kRadCalf);
            if (BoneAt(player, bone_index::hip_R, hip) && BoneAt(player, bone_index::knee_R, knee))
                AddCapsule(boxes, hip, knee, kRadThigh);
            if (BoneAt(player, bone_index::knee_R, knee) && BoneAt(player, bone_index::foot_heel_R, foot))
                AddCapsule(boxes, knee, foot, kRadCalf);
        }

        if (boxes.empty() && has_head)
            AddSphere(boxes, head, kRadHead);

        return boxes;
    }

    float ClosestRaySegment(const Vec3_t& origin, const Vec3_t& dir, const Vec3_t& a, const Vec3_t& b)
    {
        const Vec3_t u = b - a;
        const Vec3_t w0 = a - origin;
        const float uu = u.dot(u);
        const float uv = u.dot(dir);
        const float vv = dir.dot(dir);
        const float uw = w0.dot(u);
        const float vw = w0.dot(dir);
        const float det = uu * vv - uv * uv;

        float s = 0.f;
        float t = 0.f;
        if (det > 1.0e-8f)
        {
            s = (uv * vw - vv * uw) / det;
            t = (uu * vw - uv * uw) / det;
        }
        else
        {
            t = vw;
        }

        s = std::clamp(s, 0.f, 1.f);
        const Vec3_t on_seg = a + u * s;
        t = (on_seg - origin).dot(dir);
        if (t < 0.f)
            t = 0.f;
        return (origin + dir * t - on_seg).length();
    }

    float RayCapsuleMiss(const Vec3_t& origin, const Vec3_t& dir, const HitCapsule& cap)
    {
        if (cap.a.x == cap.b.x && cap.a.y == cap.b.y && cap.a.z == cap.b.z)
            return ClosestRaySegment(origin, dir, cap.a, cap.a);
        return ClosestRaySegment(origin, dir, cap.a, cap.b);
    }

    bool RayHitsCapsule(const Vec3_t& shoot_pos, const Vec3_t& forward, const HitCapsule& cap)
    {
        const float miss = RayCapsuleMiss(shoot_pos, forward, cap);
        const bool head = cap.radius <= kRadHead + 0.05f;
        const float slack = head ? 1.05f : 1.12f;
        return miss <= cap.radius * slack;
    }

    bool CapsuleHitChance(
        const Vec3_t& shoot_pos,
        const Vec3_t& forward,
        const HitCapsule& cap,
        float inaccuracy,
        float spread,
        int hit_chance_percent)
    {
        if (!RayHitsCapsule(shoot_pos, forward, cap))
            return false;

        if (hit_chance_percent <= 0)
            return true;

        const Vec3_t mid = (cap.a + cap.b) * 0.5f;
        const float distance = (mid - shoot_pos).length();
        if (distance < 1.f)
            return true;

        const float miss = RayCapsuleMiss(shoot_pos, forward, cap);
        const float total_spread = (std::min)(0.45f,
            (std::max)(0.f, inaccuracy) + (std::max)(0.f, spread));
        const float spread_radius = std::tan(total_spread) * distance;
        const float head_scale = (cap.radius <= kRadHead + 0.05f) ? 1.15f : 1.6f;
        float chance = 1.f - (miss / (spread_radius + cap.radius * head_scale + 1.5f));
        chance = std::clamp(chance, 0.f, 1.f);
        return (chance * 100.f) >= static_cast<float>(hit_chance_percent);
    }
}

void Triggerbot::Tick()
{
    static bool was_on_target = false;
    static auto on_target_since = std::chrono::steady_clock::now();
    static auto ready_since = std::chrono::steady_clock::now();
    static auto last_shot = std::chrono::steady_clock::now();
    static bool waiting_before_shot = false;

    auto lose_target = [&] {
        was_on_target = false;
        waiting_before_shot = false;
    };

    if (!cfg::triggerbot::enabled)
    {
        lose_target();
        return;
    }

    if (!Engine::IsGameFocused())
    {
        lose_target();
        return;
    }

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        return;

    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    if (!process || !process->handle_ || !client.base)
    {
        lose_target();
        return;
    }

    const auto& snapshot = Cache::Current();
    const auto local_pawn = snapshot.local.PawnAddress();
    if (!snapshot.local.alive || !local_pawn || IsKnife(snapshot.local.weapon.item_index))
    {
        lose_target();
        return;
    }

    const auto crosshair_handle = process->read<int32_t>(local_pawn + offsets::pawn::m_iIDEntIndex);
    if (crosshair_handle <= 0)
    {
        lose_target();
        return;
    }

    const auto entity_list = process->read<uintptr_t>(client.base + offsets::entityList);
    if (!entity_list)
    {
        lose_target();
        return;
    }

    const auto target_pawn = ResolveHandle(process, entity_list, static_cast<uint32_t>(crosshair_handle));
    if (!target_pawn || target_pawn == local_pawn)
    {
        lose_target();
        return;
    }

    const Player* target = nullptr;
    for (const auto& p : snapshot.players)
    {
        if (p.PawnAddress() != target_pawn)
            continue;
        if (p.localplayer || p.SameEntity(snapshot.local))
            break;
        target = &p;
        break;
    }

    const int health = target ? target->health : 0;
    if (health <= 0 || health > 100)
    {
        lose_target();
        return;
    }

    if (!snapshot.globals.is_deathmatch)
    {
        const auto team = target ? target->team : 0;
        if (team == snapshot.local.team)
        {
            lose_target();
            return;
        }
    }

    const auto weapon_services = process->read<uintptr_t>(local_pawn + offsets::pawn::m_pWeaponServices);
    if (!weapon_services)
    {
        lose_target();
        return;
    }

    const auto weapon = ResolveHandle(
        process, entity_list,
        process->read<uint32_t>(weapon_services + offsets::pawn::m_hActiveWeapon));
    if (!weapon)
    {
        lose_target();
        return;
    }

    const float inaccuracy = process->read<float>(weapon + offsets::pawn::m_fAccuracyPenalty);
    const float spread = process->read<float>(weapon + offsets::pawn::m_flTurningInaccuracy);
    short item = snapshot.local.weapon.item_index;
    if (item <= 0)
    {
        item = static_cast<short>(process->read<uint16_t>(
            weapon + offsets::pawn::m_AttributeManager +
            offsets::pawn::m_Item + offsets::pawn::m_iItemDefinitionIndex));
    }

    const auto trg = ResolveTrigger(item);
    const bool is_sniper = IsSniper(item);
    const bool scoped_flag = snapshot.local.scoped;
    const bool scoped = scoped_flag || (is_sniper && inaccuracy < 0.02f);
    if (trg.block_noscope && is_sniper && !scoped)
        return;

    const Vec3_t shoot_pos = snapshot.local.EyePos();

    const auto aim_angles = snapshot.local.eye_angles;
    Vec3_t forward, right, up;
    AngleVectors(aim_angles, forward, right, up);
    (void)right;
    (void)up;

    std::vector<HitCapsule> boxes;
    if (target)
        boxes = BuildHitboxes(*target, trg);

    if (boxes.empty())
    {
        Vec3_t head{};
        if (target && target->bone_list.size() > bone_index::head)
            head = target->bone_list[bone_index::head].pos;
        if (head.zero() && target)
            head = { target->pos.x, target->pos.y, target->pos.z + 72.f };
        if (!head.zero())
            boxes.push_back({ head, head, kRadHead });
    }

    const int required_hc = std::clamp(trg.hit_chance, 0, 100);
    bool hitchance_ok = false;
    for (const auto& box : boxes)
    {
        if (CapsuleHitChance(shoot_pos, forward, box, inaccuracy, spread, required_hc))
        {
            hitchance_ok = true;
            break;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (!was_on_target)
    {
        was_on_target = true;
        on_target_since = now;
        waiting_before_shot = false;
        return;
    }

    const int reaction_ms = std::clamp(trg.reaction_time, 0, 1000);
    if (now - on_target_since < std::chrono::milliseconds(reaction_ms))
        return;

    if (!hitchance_ok)
        return;

    const int before_ms = std::clamp(trg.before_shot, 0, 100);
    if (before_ms > 0)
    {
        if (!waiting_before_shot)
        {
            waiting_before_shot = true;
            ready_since = now;
            return;
        }
        if (now - ready_since < std::chrono::milliseconds(before_ms))
            return;
    }

    const int between_ms = std::clamp(trg.between_shots, 0, 500);
    if (now - last_shot < std::chrono::milliseconds((std::max)(between_ms, 15)))
        return;

    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));

    last_shot = now;
    waiting_before_shot = false;
}
