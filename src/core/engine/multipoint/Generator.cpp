#include "Generator.hpp"

#include <utility>

namespace mp {
namespace {

constexpr float kContainEps = 1e-3f;
constexpr float kDupEps = 0.35f;
constexpr float kMinUsefulDist = 0.40f;
constexpr float kMinEdgeAbs = 0.12f;
constexpr float kMinEdgeNorm = 0.08f;

float SanitizeScale(float s, const MultipointContext& ctx)
{
    if (!std::isfinite(s) || s < 0.f)
        s = ctx.scale.body;
    return Clamp(s, ctx.min_scale, ctx.max_scale);
}

float CategoryScale(const PreparedHitbox& hb, const MultipointContext& ctx, bool vertical)
{
    switch (hb.category)
    {
    case HitboxCategory::Head:
        return vertical ? ctx.scale.head_vertical : ctx.scale.head_lateral;
    case HitboxCategory::Neck:
        return ctx.scale.head_lateral * 0.85f;
    case HitboxCategory::Chest:
    case HitboxCategory::Stomach:
    case HitboxCategory::Pelvis:
        return ctx.scale.body;
    case HitboxCategory::Arm:
    case HitboxCategory::Leg:
    case HitboxCategory::Foot:
        return ctx.scale.limbs;
    default:
        return ctx.scale.body;
    }
}

int DefaultBudget(HitboxCategory cat)
{
    switch (cat)
    {
    case HitboxCategory::Head: return 6;
    case HitboxCategory::Chest:
    case HitboxCategory::Stomach:
    case HitboxCategory::Pelvis: return 3;
    case HitboxCategory::Neck: return 2;
    case HitboxCategory::Arm:
    case HitboxCategory::Leg:
    case HitboxCategory::Foot: return 2;
    default: return 3;
    }
}

float TypePriority(MultipointType t)
{
    switch (t)
    {
    case MultipointType::Center: return 1.00f;
    case MultipointType::Left:
    case MultipointType::Right: return 0.80f;
    case MultipointType::Top: return 0.70f;
    case MultipointType::UpperLeft:
    case MultipointType::UpperRight: return 0.50f;
    case MultipointType::Secondary: return 0.35f;
    }
    return 0.f;
}

Vector3 LeastParallelBoneAxis(const PreparedHitbox& hb, const Vector3& axis)
{
    const Vector3 cand[3] = { hb.bone_x, hb.bone_y, hb.bone_z };
    float best_abs = 2.f;
    Vector3 best = hb.bone_x;
    for (const auto& c : cand)
    {
        const auto n = NormalizeSafe(c);
        if (!n)
            continue;
        const float a = std::fabs(Dot(*n, axis));
        if (a < best_abs)
        {
            best_abs = a;
            best = *n;
        }
    }
    return best;
}

Vector3 WorldToObbLocal(const PreparedHitbox& hb, const Vector3& world)
{
    const Vector3 d = world - hb.obb_center;
    return {
        Dot(d, hb.bone_x),
        Dot(d, hb.bone_y),
        Dot(d, hb.bone_z)
    };
}

bool InsideCapsule(const PreparedHitbox& hb, const Vector3& p, float extra)
{
    if (hb.radius <= 0.f)
        return false;
    return DistanceToSegment(p, hb.capsule_start, hb.capsule_end) <= hb.radius + extra + kContainEps;
}

bool InsideObb(const PreparedHitbox& hb, const Vector3& p, float extra)
{
    const Vector3 local = WorldToObbLocal(hb, p);
    const float e = extra + kContainEps;
    return std::fabs(local.x) <= hb.obb_extents.x + e
        && std::fabs(local.y) <= hb.obb_extents.y + e
        && std::fabs(local.z) <= hb.obb_extents.z + e;
}

float CapsuleMargin(const PreparedHitbox& hb, const Vector3& p)
{
    if (hb.radius <= 0.f)
        return 0.f;
    return hb.radius - DistanceToSegment(p, hb.capsule_start, hb.capsule_end);
}

float ObbMargin(const PreparedHitbox& hb, const Vector3& p)
{
    const Vector3 local = WorldToObbLocal(hb, p);
    const float mx = hb.obb_extents.x - std::fabs(local.x);
    const float my = hb.obb_extents.y - std::fabs(local.y);
    const float mz = hb.obb_extents.z - std::fabs(local.z);
    return (std::min)(mx, (std::min)(my, mz));
}

float AdaptiveRadial(const PreparedHitbox& hb, const MultipointContext& ctx, float configured)
{
    float s = SanitizeScale(configured, ctx);
    if (!ctx.adaptive_scale)
        return s;

    // Keep a floor of usable volume. Tiny hitboxes pull scale in.
    if (hb.shape == HitboxShape::Capsule)
    {
        if (hb.radius < 2.5f)
            s *= 0.82f;
        else if (hb.radius > 7.f)
            s = (std::min)(s, 0.72f);
        if (hb.sphere_like)
            s *= 0.90f;
    }
    else
    {
        const float min_ext = (std::min)(hb.obb_extents.x, (std::min)(hb.obb_extents.y, hb.obb_extents.z));
        if (min_ext < 2.0f)
            s *= 0.80f;
    }
    return SanitizeScale(s, ctx);
}

bool PushCandidate(MultipointSet& out, const PreparedHitbox& hb, const MultipointContext& ctx,
    const Vector3& pos, MultipointType type, float used_scale)
{
    if (out.count >= kMaxMultipoints)
        return false;
    if (ctx.max_points > 0 && static_cast<int>(out.count) >= ctx.max_points)
        return false;
    if (!IsFinite(pos))
        return false;
    if (!IsPointInsideHitbox(hb, pos))
        return false;

    const float margin = GetEdgeMargin(hb, pos);
    const float norm = GetNormalizedEdgeMargin(hb, pos);
    if (type != MultipointType::Center)
    {
        if (margin < kMinEdgeAbs && norm < kMinEdgeNorm)
            return false;
        if (DistanceSquared(pos, hb.center) < kMinUsefulDist * kMinUsefulDist)
            return false;
    }

    for (std::size_t i = 0; i < out.count; ++i)
    {
        if (DistanceSquared(out.points[i].position, pos) < kDupEps * kDupEps)
            return false;
    }

    MultipointCandidate c{};
    c.position = pos;
    c.hitbox = hb.index;
    c.type = type;
    c.scale = used_scale;
    c.distance_from_center = Distance(pos, hb.center);
    c.edge_margin = margin;
    c.normalized_edge_margin = norm;
    c.geometric_priority = TypePriority(type) + Clamp(norm, 0.f, 1.f) * 0.08f;
    c.center = (type == MultipointType::Center);
    c.lateral = (type == MultipointType::Left || type == MultipointType::Right
        || type == MultipointType::UpperLeft || type == MultipointType::UpperRight);
    c.vertical = (type == MultipointType::Top
        || type == MultipointType::UpperLeft || type == MultipointType::UpperRight);
    out.points[out.count++] = c;
    return true;
}

Vector3 OffsetAlong(const PreparedHitbox& hb, const Vector3& dir, float offset)
{
    return hb.center + dir * offset;
}

void AddLateralPair(MultipointSet& out, const PreparedHitbox& hb, const MultipointContext& ctx,
    const Vector3& side, float offset, float scale)
{
    PushCandidate(out, hb, ctx, OffsetAlong(hb, side, offset), MultipointType::Left, scale);
    PushCandidate(out, hb, ctx, OffsetAlong(hb, -side, offset), MultipointType::Right, scale);
}

void GenerateCapsule(MultipointSet& out, const PreparedHitbox& hb, const MultipointContext& ctx)
{
    const auto basis = BuildCapsuleBasis(hb, ctx.shooter_eye);
    if (!basis.valid)
        return;

    const float lat = AdaptiveRadial(hb, ctx, CategoryScale(hb, ctx, false));
    const float vert = AdaptiveRadial(hb, ctx, CategoryScale(hb, ctx, true));
    const float r = hb.radius;

    switch (hb.category)
    {
    case HitboxCategory::Head:
    {
        AddLateralPair(out, hb, ctx, basis.side, r * lat, lat);
        // Top along capsule axis toward the "upper" pole (axis that points more like bone Z).
        Vector3 up_axis = hb.capsule_axis;
        if (Dot(up_axis, hb.bone_z) < 0.f)
            up_axis = -up_axis;
        const float half = hb.capsule_length * 0.5f;
        const float top_off = half + r * vert;
        PushCandidate(out, hb, ctx, hb.center + up_axis * top_off, MultipointType::Top, vert);
        if (ctx.max_points >= 6)
        {
            const Vector3 diag_l = NormalizeOr(basis.side + basis.toward, basis.side);
            const Vector3 diag_r = NormalizeOr(-basis.side + basis.toward, -basis.side);
            const float d = r * lat * 0.78f;
            PushCandidate(out, hb, ctx, OffsetAlong(hb, diag_l, d), MultipointType::UpperLeft, lat);
            PushCandidate(out, hb, ctx, OffsetAlong(hb, diag_r, d), MultipointType::UpperRight, lat);
        }
        break;
    }
    case HitboxCategory::Neck:
        AddLateralPair(out, hb, ctx, basis.side, r * lat * 0.70f, lat);
        break;
    case HitboxCategory::Chest:
    case HitboxCategory::Stomach:
    case HitboxCategory::Pelvis:
        AddLateralPair(out, hb, ctx, basis.side, r * lat, lat);
        break;
    case HitboxCategory::Arm:
    case HitboxCategory::Leg:
        PushCandidate(out, hb, ctx, OffsetAlong(hb, basis.side, r * lat), MultipointType::Left, lat);
        break;
    case HitboxCategory::Foot:
        PushCandidate(out, hb, ctx, OffsetAlong(hb, basis.toward, r * lat * 0.65f),
            MultipointType::Secondary, lat);
        break;
    default:
        AddLateralPair(out, hb, ctx, basis.side, r * lat, lat);
        break;
    }
}

void GenerateObb(MultipointSet& out, const PreparedHitbox& hb, const MultipointContext& ctx)
{
    const Vector3 to_eye = ctx.shooter_eye - hb.obb_center;
    Vector3 face = hb.bone_x;
    float best = -1.f;
    const Vector3 axes[3] = { hb.bone_x, hb.bone_y, hb.bone_z };
    const float ext[3] = { hb.obb_extents.x, hb.obb_extents.y, hb.obb_extents.z };
    int face_i = 0;
    for (int i = 0; i < 3; ++i)
    {
        const auto n = NormalizeSafe(axes[i]);
        if (!n)
            continue;
        const float a = std::fabs(Dot(*n, to_eye));
        if (a > best)
        {
            best = a;
            face = (Dot(*n, to_eye) >= 0.f) ? *n : -(*n);
            face_i = i;
        }
    }

    // Lateral = widest remaining axis, perpendicular to the facing axis.
    int lat_i = (face_i == 0) ? 1 : 0;
    for (int i = 0; i < 3; ++i)
    {
        if (i == face_i)
            continue;
        if (ext[i] > ext[lat_i])
            lat_i = i;
    }
    Vector3 side = axes[lat_i];
    if (const auto n = NormalizeSafe(side))
        side = *n;
    else
        return;

    int vert_i = 3 - face_i - lat_i;
    Vector3 up = axes[vert_i];
    if (const auto n = NormalizeSafe(up))
        up = *n;

    const float lat = AdaptiveRadial(hb, ctx, CategoryScale(hb, ctx, false));
    const float vert = AdaptiveRadial(hb, ctx, CategoryScale(hb, ctx, true));

    auto along_ext = [&](const Vector3& dir, int axis, float scale) {
        return hb.obb_center + dir * (ext[axis] * scale);
    };

    switch (hb.category)
    {
    case HitboxCategory::Head:
        PushCandidate(out, hb, ctx, along_ext(side, lat_i, lat), MultipointType::Left, lat);
        PushCandidate(out, hb, ctx, along_ext(-side, lat_i, lat), MultipointType::Right, lat);
        PushCandidate(out, hb, ctx, along_ext(up, vert_i, vert), MultipointType::Top, vert);
        if (ctx.max_points >= 6)
        {
            const Vector3 ul = NormalizeOr(side + up, side);
            const Vector3 ur = NormalizeOr(-side + up, -side);
            const float mix = (ext[lat_i] + ext[vert_i]) * 0.5f * lat * 0.72f;
            PushCandidate(out, hb, ctx, hb.obb_center + ul * mix, MultipointType::UpperLeft, lat);
            PushCandidate(out, hb, ctx, hb.obb_center + ur * mix, MultipointType::UpperRight, lat);
        }
        break;
    case HitboxCategory::Chest:
    case HitboxCategory::Stomach:
    case HitboxCategory::Pelvis:
        PushCandidate(out, hb, ctx, along_ext(side, lat_i, lat), MultipointType::Left, lat);
        PushCandidate(out, hb, ctx, along_ext(-side, lat_i, lat), MultipointType::Right, lat);
        break;
    case HitboxCategory::Neck:
        PushCandidate(out, hb, ctx, along_ext(side, lat_i, lat * 0.70f), MultipointType::Left, lat);
        break;
    case HitboxCategory::Arm:
    case HitboxCategory::Leg:
        PushCandidate(out, hb, ctx, along_ext(side, lat_i, lat), MultipointType::Left, lat);
        break;
    case HitboxCategory::Foot:
        PushCandidate(out, hb, ctx, hb.obb_center + face * (ext[face_i] * lat * 0.55f),
            MultipointType::Secondary, lat);
        break;
    default:
        PushCandidate(out, hb, ctx, along_ext(side, lat_i, lat), MultipointType::Left, lat);
        PushCandidate(out, hb, ctx, along_ext(-side, lat_i, lat), MultipointType::Right, lat);
        break;
    }
}

} // namespace

std::optional<PreparedHitbox> PrepareHitbox(const HitboxData& hitbox, const Matrix3x4& bone_matrix)
{
    if (!bone_matrix.Finite())
        return std::nullopt;
    if (!IsFinite(hitbox.mins) || !IsFinite(hitbox.maxs))
        return std::nullopt;
    if (!std::isfinite(hitbox.radius))
        return std::nullopt;

    PreparedHitbox hb{};
    hb.index = hitbox.index;
    hb.bone = hitbox.bone;
    hb.category = (hitbox.category == HitboxCategory::Unknown)
        ? CategoryFromIndex(hitbox.index)
        : hitbox.category;
    hb.mins = hitbox.mins;
    hb.maxs = hitbox.maxs;
    hb.radius = hitbox.radius;
    hb.bone_matrix = bone_matrix;

    const auto nx = NormalizeSafe(bone_matrix.AxisX());
    const auto ny = NormalizeSafe(bone_matrix.AxisY());
    const auto nz = NormalizeSafe(bone_matrix.AxisZ());
    if (!nx || !ny || !nz)
        return std::nullopt;
    hb.bone_x = *nx;
    hb.bone_y = *ny;
    hb.bone_z = *nz;

    const bool capsule = hitbox.radius > kLenEps;
    hb.shape = capsule ? HitboxShape::Capsule : HitboxShape::OBB;

    if (capsule)
    {
        hb.capsule_start = TransformPoint(bone_matrix, hitbox.mins);
        hb.capsule_end = TransformPoint(bone_matrix, hitbox.maxs);
        if (!IsFinite(hb.capsule_start) || !IsFinite(hb.capsule_end))
            return std::nullopt;

        const Vector3 delta = hb.capsule_end - hb.capsule_start;
        hb.capsule_length = Length(delta);
        hb.sphere_like = hb.capsule_length < kLenEps;
        if (hb.sphere_like)
        {
            hb.capsule_axis = hb.bone_z;
            hb.capsule_end = hb.capsule_start;
            hb.capsule_length = 0.f;
            hb.center = hb.capsule_start;
        }
        else
        {
            hb.capsule_axis = delta / hb.capsule_length;
            hb.center = (hb.capsule_start + hb.capsule_end) * 0.5f;
        }
        if (!IsFinite(hb.capsule_axis) || !IsFinite(hb.center))
            return std::nullopt;
    }
    else
    {
        const Vector3 local_c = (hitbox.mins + hitbox.maxs) * 0.5f;
        Vector3 ext{
            std::fabs(hitbox.maxs.x - hitbox.mins.x) * 0.5f,
            std::fabs(hitbox.maxs.y - hitbox.mins.y) * 0.5f,
            std::fabs(hitbox.maxs.z - hitbox.mins.z) * 0.5f
        };
        if (ext.x < kEps && ext.y < kEps && ext.z < kEps)
            return std::nullopt;
        ext.x = (std::max)(ext.x, kEps);
        ext.y = (std::max)(ext.y, kEps);
        ext.z = (std::max)(ext.z, kEps);
        hb.obb_extents = ext;
        hb.obb_center = TransformPoint(bone_matrix, local_c);
        hb.center = hb.obb_center;
        if (!IsFinite(hb.center))
            return std::nullopt;
    }

    hb.valid = true;
    return hb;
}

CapsuleBasis BuildCapsuleBasis(const PreparedHitbox& hitbox, const Vector3& shooter_eye)
{
    CapsuleBasis b{};
    if (!hitbox.valid)
        return b;

    const Vector3 axis = hitbox.sphere_like
        ? NormalizeOr(hitbox.bone_z, Vector3{ 0.f, 0.f, 1.f })
        : hitbox.capsule_axis;

    Vector3 to_shooter = shooter_eye - hitbox.center;
    if (!IsFinite(to_shooter) || LengthSquared(to_shooter) < kLenEps * kLenEps)
        to_shooter = LeastParallelBoneAxis(hitbox, axis);

    const Vector3 projected = to_shooter - axis * Dot(to_shooter, axis);
    Vector3 toward{};
    if (const auto n = NormalizeSafe(projected))
    {
        toward = *n;
    }
    else
    {
        toward = LeastParallelBoneAxis(hitbox, axis);
        toward = toward - axis * Dot(toward, axis);
        if (const auto n2 = NormalizeSafe(toward))
            toward = *n2;
        else
            return b;
    }

    const auto side = NormalizeSafe(Cross(axis, toward));
    if (!side)
        return b;

    b.toward = toward;
    b.side = *side;
    b.valid = IsFinite(b.toward) && IsFinite(b.side);
    return b;
}

bool IsPointInsideHitbox(const PreparedHitbox& hitbox, const Vector3& point, float extra)
{
    if (!hitbox.valid || !IsFinite(point))
        return false;
    if (hitbox.shape == HitboxShape::Capsule)
        return InsideCapsule(hitbox, point, extra);
    return InsideObb(hitbox, point, extra);
}

float GetEdgeMargin(const PreparedHitbox& hitbox, const Vector3& point)
{
    if (!hitbox.valid || !IsFinite(point))
        return 0.f;
    if (hitbox.shape == HitboxShape::Capsule)
        return CapsuleMargin(hitbox, point);
    return ObbMargin(hitbox, point);
}

float GetNormalizedEdgeMargin(const PreparedHitbox& hitbox, const Vector3& point)
{
    const float m = GetEdgeMargin(hitbox, point);
    if (hitbox.shape == HitboxShape::Capsule)
    {
        if (hitbox.radius <= kLenEps)
            return 0.f;
        return Clamp(m / hitbox.radius, 0.f, 1.f);
    }
    const float maxm = (std::min)(hitbox.obb_extents.x, (std::min)(hitbox.obb_extents.y, hitbox.obb_extents.z));
    if (maxm <= kLenEps)
        return 0.f;
    return Clamp(m / maxm, 0.f, 1.f);
}

MultipointSet GenerateMultipoints(const PreparedHitbox& hitbox, const MultipointContext& ctx)
{
    MultipointSet out{};
    if (!hitbox.valid || !IsFinite(hitbox.center))
        return out;

    MultipointContext local = ctx;
    if (!std::isfinite(local.min_scale) || local.min_scale < 0.f)
        local.min_scale = 0.20f;
    if (!std::isfinite(local.max_scale) || local.max_scale < local.min_scale)
        local.max_scale = (std::max)(local.min_scale, 0.92f);
    local.scale.head_lateral = SanitizeScale(local.scale.head_lateral, local);
    local.scale.head_vertical = SanitizeScale(local.scale.head_vertical, local);
    local.scale.body = SanitizeScale(local.scale.body, local);
    local.scale.limbs = SanitizeScale(local.scale.limbs, local);

    const int budget = (std::min)(
        local.max_points > 0 ? local.max_points : DefaultBudget(hitbox.category),
        static_cast<int>(kMaxMultipoints));
    local.max_points = (std::max)(1, budget);

    PushCandidate(out, hitbox, local, hitbox.center, MultipointType::Center, 0.f);
    if (out.count == 0)
        return out;

    if (hitbox.shape == HitboxShape::Capsule)
        GenerateCapsule(out, hitbox, local);
    else
        GenerateObb(out, hitbox, local);

    return out;
}

} // namespace mp
