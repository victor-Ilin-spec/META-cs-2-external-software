#pragma once

// PROJECT ADAPTER — maps this repo's Vec3_t / bone_pos / HitboxRaw into mp::*.
// No RPM, no engine calls. Fill HitboxData + Matrix3x4 from whatever the
// existing cache already read, then call PrepareHitbox / GenerateMultipoints.

#include "Generator.hpp"
#include "core/engine/classes/Bones.hpp"
#include "core/engine/classes/Hitbox.hpp"
#include "core/engine/types/Vec3.hpp"

namespace mp {

inline Vector3 FromVec(const Vec3_t& v) { return { v.x, v.y, v.z }; }
inline Vec3_t ToVec(const Vector3& v) { return { v.x, v.y, v.z }; }

// CTransform: pos + quaternion (x,y,z,w). Same layout as bone_data / bone_pos.
inline Matrix3x4 MatrixFromQuat(const Vec3_t& origin, float qx, float qy, float qz, float qw)
{
    const float n2 = qx * qx + qy * qy + qz * qz + qw * qw;
    if (!(n2 > 0.81f && n2 < 1.21f) || !std::isfinite(n2))
        return Matrix3x4::FromAxesOrigin({ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, FromVec(origin));

    const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    const Vector3 x{ 1.f - 2.f * (yy + zz), 2.f * (xy + wz), 2.f * (xz - wy) };
    const Vector3 y{ 2.f * (xy - wz), 1.f - 2.f * (xx + zz), 2.f * (yz + wx) };
    const Vector3 z{ 2.f * (xz + wy), 2.f * (yz - wx), 1.f - 2.f * (xx + yy) };
    return Matrix3x4::FromAxesOrigin(x, y, z, FromVec(origin));
}

inline Matrix3x4 MatrixFromBone(const bone_pos& b)
{
    return MatrixFromQuat(b.pos, b.qx, b.qy, b.qz, b.qw);
}

inline HitboxData FromRaw(const HitboxRaw& raw)
{
    HitboxData d{};
    d.index = raw.index;
    d.bone = raw.bone;
    d.category = CategoryFromGroup(raw.group);
    d.mins = FromVec(raw.min);
    d.maxs = FromVec(raw.max);
    d.radius = raw.radius;
    return d;
}

} // namespace mp
