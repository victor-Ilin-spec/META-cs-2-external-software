#pragma once

#include "core/engine/types/Vec3.hpp"

#include <cstdint>
#include <vector>

class Player;
class pProcess;

// Source 2 CHitBox (animationsystem.dll schema).
// Поля как в дампе: min/max в локали кости, radius раздувает AABB.
struct HitboxRaw {
    Vec3_t min{};
    Vec3_t max{};
    float radius = 0.f;
    int group = 0;
    int bone = -1;
    int index = -1;
};

struct Multipoint {
    Vec3_t pos{};
    int hitbox = -1;
    int group = 0;
    bool visible = false;
    float score = 0.f;
};

namespace hitbox {
    constexpr int kHead = 0;
    constexpr int kNeck = 1;
    constexpr int kChest = 2;
    constexpr int kStomach = 3;
    constexpr int kPelvis = 4;
    constexpr int kLArm = 5;
    constexpr int kRArm = 6;
    constexpr int kLLeg = 7;
    constexpr int kRLeg = 8;

    // CHitBox offsets (animationsystem CHitBox).
    constexpr std::ptrdiff_t kMin = 0x18;
    constexpr std::ptrdiff_t kMax = 0x24;
    constexpr std::ptrdiff_t kRadius = 0x30;
    constexpr std::ptrdiff_t kGroup = 0x38;
    constexpr std::ptrdiff_t kIndex = 0x48;
    constexpr std::ptrdiff_t kStride = 0x70;

    // CHitBoxSet::m_HitBoxes — CUtlVector at +0x10.
    constexpr std::ptrdiff_t kSetVec = 0x10;

    int ClampCount(int n);
    Vec3_t Rotate(const Vec3_t& v, float qx, float qy, float qz, float qw);
    Vec3_t Xform(const Vec3_t& local, const Vec3_t& origin, float qx, float qy, float qz, float qw);

    // UC: min -= r; max += r. Потом shrink scale к центру.
    void Expand(Vec3_t& mn, Vec3_t& mx, float radius, float scale);

    void GenerateInto(std::vector<Vec3_t>& out, const HitboxRaw& box, const Vec3_t& bone_pos,
        float qx, float qy, float qz, float qw, int count, float scale);
    std::vector<Vec3_t> Generate(const HitboxRaw& box, const Vec3_t& bone_pos,
        float qx, float qy, float qz, float qw, int count, float scale);

    bool ReadLive(pProcess* proc, uintptr_t pawn, std::vector<HitboxRaw>& out);
    void FallbackPlayer(std::vector<HitboxRaw>& out);

    bool GroupEnabled(int group, bool head, bool chest, bool stomach, bool arms, bool legs);
}
