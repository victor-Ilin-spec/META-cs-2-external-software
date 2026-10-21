#pragma once

#include "Types.hpp"

#include <optional>

namespace mp {

std::optional<PreparedHitbox> PrepareHitbox(const HitboxData& hitbox, const Matrix3x4& bone_matrix);

MultipointSet GenerateMultipoints(const PreparedHitbox& hitbox, const MultipointContext& ctx);

bool IsPointInsideHitbox(const PreparedHitbox& hitbox, const Vector3& point, float extra = 0.f);

float GetEdgeMargin(const PreparedHitbox& hitbox, const Vector3& point);
float GetNormalizedEdgeMargin(const PreparedHitbox& hitbox, const Vector3& point);

// View-relative unit vectors in the plane perpendicular to capsule axis.
// left = Cross(axis, toward_shooter_in_plane), up = toward_shooter_in_plane (or bone fallback).
struct CapsuleBasis {
    Vector3 toward{}; // unit, ⟂ axis, generally faces shooter
    Vector3 side{};   // unit, ⟂ axis and toward
    bool valid = false;
};

CapsuleBasis BuildCapsuleBasis(const PreparedHitbox& hitbox, const Vector3& shooter_eye);

} // namespace mp
