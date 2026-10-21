#pragma once

#include "Math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace mp {

inline constexpr std::size_t kMaxMultipoints = 8;

enum class HitboxCategory : std::uint8_t {
    Head,
    Neck,
    Chest,
    Stomach,
    Pelvis,
    Arm,
    Leg,
    Foot,
    Unknown
};

enum class HitboxShape : std::uint8_t {
    Capsule,
    OBB
};

enum class MultipointType : std::uint8_t {
    Center,
    Left,
    Right,
    Top,
    UpperLeft,
    UpperRight,
    Secondary
};

struct HitboxData {
    int index = -1;
    int bone = -1;
    HitboxCategory category = HitboxCategory::Unknown;
    Vector3 mins{};
    Vector3 maxs{};
    float radius = 0.f; // > 0 → capsule; <= 0 → OBB
};

struct MultipointScaleConfig {
    float head_lateral = 0.62f;
    float head_vertical = 0.55f;
    float body = 0.58f;
    float limbs = 0.50f;
};

struct MultipointContext {
    Vector3 shooter_eye{};
    MultipointScaleConfig scale{};
    float min_scale = 0.20f;
    float max_scale = 0.92f;
    bool adaptive_scale = true;
    int max_points = 6;
};

struct PreparedHitbox {
    int index = -1;
    int bone = -1;
    HitboxCategory category = HitboxCategory::Unknown;
    HitboxShape shape = HitboxShape::Capsule;
    bool valid = false;

    Vector3 center{};
    Vector3 mins{};
    Vector3 maxs{};
    float radius = 0.f;

    Matrix3x4 bone_matrix{};
    Vector3 bone_x{};
    Vector3 bone_y{};
    Vector3 bone_z{};

    Vector3 capsule_start{};
    Vector3 capsule_end{};
    Vector3 capsule_axis{};
    float capsule_length = 0.f;
    bool sphere_like = false;

    Vector3 obb_center{};
    Vector3 obb_extents{};
};

struct MultipointCandidate {
    Vector3 position{};
    int hitbox = -1;
    MultipointType type = MultipointType::Center;
    float scale = 0.f;
    float distance_from_center = 0.f;
    float edge_margin = 0.f;
    float normalized_edge_margin = 0.f;
    float geometric_priority = 0.f;
    bool center = false;
    bool lateral = false;
    bool vertical = false;
};

struct MultipointSet {
    std::array<MultipointCandidate, kMaxMultipoints> points{};
    std::size_t count = 0;

    void clear() { count = 0; }
    bool empty() const { return count == 0; }
    const MultipointCandidate* begin() const { return points.data(); }
    const MultipointCandidate* end() const { return points.data() + count; }
};

// PROJECT ADAPTER — CS2 hitbox group / index → category.
// Studio player set is typically:
//   0 head, 1 neck, 2 pelvis/stomach, 3 stomach, 4 chest,
//   5-8 arms, 9-12 legs, 13-18 feet/hands depending on model.
inline HitboxCategory CategoryFromIndex(int index)
{
    switch (index)
    {
    case 0: return HitboxCategory::Head;
    case 1: return HitboxCategory::Neck;
    case 2: return HitboxCategory::Pelvis;
    case 3: return HitboxCategory::Stomach;
    case 4: return HitboxCategory::Chest;
    case 5:
    case 6:
    case 7:
    case 8: return HitboxCategory::Arm;
    case 9:
    case 10:
    case 11:
    case 12: return HitboxCategory::Leg;
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18: return HitboxCategory::Foot;
    default: return HitboxCategory::Unknown;
    }
}

inline HitboxCategory CategoryFromGroup(int group)
{
    // PROJECT ADAPTER — matches hitbox::kHead..kRLeg in this repo.
    switch (group)
    {
    case 0: return HitboxCategory::Head;
    case 1: return HitboxCategory::Neck;
    case 2: return HitboxCategory::Chest;
    case 3: return HitboxCategory::Stomach;
    case 4: return HitboxCategory::Pelvis;
    case 5:
    case 6: return HitboxCategory::Arm;
    case 7:
    case 8: return HitboxCategory::Leg;
    default: return HitboxCategory::Unknown;
    }
}

} // namespace mp
