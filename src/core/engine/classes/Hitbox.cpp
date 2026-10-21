#include "Hitbox.hpp"

#include "Bones.hpp"
#include "Player.hpp"
#include "core/engine/Engine.hpp"
#include "core/memory/Memory.hpp"
#include "core/offsets/Offsets.hpp"

#include <algorithm>
#include <cmath>

namespace hitbox {
namespace {

struct Fallback {
    int bone;
    int group;
    int index;
    Vec3_t min;
    Vec3_t max;
    float radius;
};

// Типичные локальные OBB игрока CS2. Если RPM CHitBox не встал — те же формулы.
static const Fallback kPlayer[] = {
    { bone_index::head,     kHead,    0, { -3.2f, -3.0f, -3.0f }, { 3.2f, 3.2f, 4.2f }, 4.2f },
    { bone_index::neck,     kNeck,    1, { -2.4f, -2.2f, -2.0f }, { 2.4f, 2.2f, 2.4f }, 3.4f },
    { bone_index::chest,    kChest,   2, { -6.0f, -4.5f, -4.0f }, { 6.0f, 4.5f, 5.0f }, 5.5f },
    { bone_index::spine_1,  kStomach, 3, { -5.4f, -4.2f, -3.6f }, { 5.4f, 4.2f, 4.2f }, 5.2f },
    { bone_index::pelvis,   kPelvis,  4, { -5.0f, -4.0f, -3.2f }, { 5.0f, 4.0f, 3.6f }, 4.8f },
    { bone_index::elbow_L,  kLArm,    5, { -2.2f, -2.0f, -8.0f }, { 2.2f, 2.0f, 8.0f }, 3.0f },
    { bone_index::elbow_R,  kRArm,    6, { -2.2f, -2.0f, -8.0f }, { 2.2f, 2.0f, 8.0f }, 3.0f },
    { bone_index::knee_L,   kLLeg,    7, { -2.6f, -2.4f, -10.f }, { 2.6f, 2.4f, 10.f }, 3.4f },
    { bone_index::knee_R,   kRLeg,    8, { -2.6f, -2.4f, -10.f }, { 2.6f, 2.4f, 10.f }, 3.4f },
};

int BoneFromGroup(int group)
{
    switch (group)
    {
    case 1: return bone_index::head;
    case 2: return bone_index::chest;
    case 3: return bone_index::spine_1;
    default: return -1;
    }
}

} // namespace

int ClampCount(int n)
{
    if (n <= 1)
        return 1;
    if (n <= 5)
        return 5;
    if (n <= 9)
        return 9;
    return 12;
}

Vec3_t Rotate(const Vec3_t& v, float qx, float qy, float qz, float qw)
{
    const Vec3_t q{ qx, qy, qz };
    const Vec3_t t = q.cross(v) * 2.f;
    return v + t * qw + q.cross(t);
}

Vec3_t Xform(const Vec3_t& local, const Vec3_t& origin, float qx, float qy, float qz, float qw)
{
    // Костный буфер часто без валидного quat — крутить нельзя, иначе точки улетают.
    (void)qx; (void)qy; (void)qz; (void)qw;
    return origin + local;
}

void Expand(Vec3_t& mn, Vec3_t& mx, float radius, float scale)
{
    // Как в UC-посте: AABB раздувается на radius со всех сторон.
    const float r = (std::max)(0.f, radius);
    mn.x -= r; mn.y -= r; mn.z -= r;
    mx.x += r; mx.y += r; mx.z += r;

    const float s = std::clamp(scale, 0.05f, 1.f);
    if (s >= 0.999f)
        return;

    const Vec3_t c{
        (mn.x + mx.x) * 0.5f,
        (mn.y + mx.y) * 0.5f,
        (mn.z + mx.z) * 0.5f
    };
    mn = c + (mn - c) * s;
    mx = c + (mx - c) * s;
}

void GenerateInto(std::vector<Vec3_t>& out, const HitboxRaw& box, const Vec3_t& bone_pos,
    float qx, float qy, float qz, float qw, int count, float scale)
{
    Vec3_t mn = box.min;
    Vec3_t mx = box.max;
    Expand(mn, mx, box.radius, scale);

    const Vec3_t c{
        (mn.x + mx.x) * 0.5f,
        (mn.y + mx.y) * 0.5f,
        (mn.z + mx.z) * 0.5f
    };

    const int want = ClampCount(count);
    Vec3_t local[12];
    int n = 1;
    local[0] = c;
    if (want >= 5)
    {
        local[1] = { mn.x, c.y, c.z };
        local[2] = { mx.x, c.y, c.z };
        local[3] = { c.x, mn.y, c.z };
        local[4] = { c.x, mx.y, c.z };
        n = 5;
    }
    if (want >= 9)
    {
        local[5] = { mn.x, mn.y, c.z };
        local[6] = { mx.x, mn.y, c.z };
        local[7] = { mn.x, mx.y, c.z };
        local[8] = { mx.x, mx.y, c.z };
        n = 9;
    }
    if (want >= 12)
    {
        local[9] = { c.x, c.y, mn.z };
        local[10] = { c.x, c.y, mx.z };
        local[11] = { c.x, c.y, mx.z * 0.55f + mn.z * 0.45f };
        n = 12;
    }

    out.reserve(out.size() + static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(Xform(local[i], bone_pos, qx, qy, qz, qw));
}

std::vector<Vec3_t> Generate(const HitboxRaw& box, const Vec3_t& bone_pos,
    float qx, float qy, float qz, float qw, int count, float scale)
{
    std::vector<Vec3_t> world;
    GenerateInto(world, box, bone_pos, qx, qy, qz, qw, count, scale);
    return world;
}

bool ReadLive(pProcess* proc, uintptr_t pawn, std::vector<HitboxRaw>& out)
{
    out.clear();
    if (!proc || !pawn)
        return false;

    const auto scene = proc->read<uintptr_t>(pawn + offsets::pawn::m_pGameSceneNode);
    if (!scene)
        return false;

    // CStrongHandle<CModel> в CModelState. Первый указатель — ресурс модели.
    const auto handle = proc->read<uintptr_t>(scene + offsets::bone::m_modelState + 0xA0);
    if (!handle)
        return false;
    uintptr_t model = proc->read<uintptr_t>(handle);
    if (!model || model < 0x10000)
        model = handle;

    // CHitBoxSetList сидит в модели как CUtlVector. Ищем вектор с разумным count и stride 0x70.
    for (std::ptrdiff_t off = 0x80; off <= 0x400; off += 8)
    {
        const int count = proc->read<int>(model + static_cast<uintptr_t>(off));
        const auto data = proc->read<uintptr_t>(model + static_cast<uintptr_t>(off) + 8);
        if (count < 4 || count > 32 || !data || data < 0x10000)
            continue;

        const float r0 = proc->read<float>(data + kRadius);
        const Vec3_t mn = proc->read<Vec3_t>(data + kMin);
        const Vec3_t mx = proc->read<Vec3_t>(data + kMax);
        if (!(r0 >= 0.f && r0 < 40.f))
            continue;
        if (mn.x > mx.x || mn.y > mx.y || mn.z > mx.z)
            continue;
        if (std::fabs(mn.x) > 80.f || std::fabs(mx.x) > 80.f)
            continue;

        out.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const uintptr_t row = data + static_cast<uintptr_t>(i) * kStride;
            HitboxRaw hb{};
            hb.min = proc->read<Vec3_t>(row + kMin);
            hb.max = proc->read<Vec3_t>(row + kMax);
            hb.radius = proc->read<float>(row + kRadius);
            hb.group = proc->read<int>(row + kGroup);
            hb.index = proc->read<uint16_t>(row + kIndex);
            hb.bone = BoneFromGroup(hb.group);
            if (hb.radius < 0.f || hb.radius > 40.f)
                continue;
            if (hb.min.x > hb.max.x)
                continue;
            out.push_back(hb);
        }
        if (out.size() >= 4)
            return true;
        out.clear();
    }
    return false;
}

void FallbackPlayer(std::vector<HitboxRaw>& out)
{
    out.clear();
    out.reserve(std::size(kPlayer));
    for (const auto& f : kPlayer)
    {
        HitboxRaw hb{};
        hb.min = f.min;
        hb.max = f.max;
        hb.radius = f.radius;
        hb.group = f.group;
        hb.bone = f.bone;
        hb.index = f.index;
        out.push_back(hb);
    }
}

bool GroupEnabled(int group, bool head, bool chest, bool stomach, bool arms, bool legs)
{
    switch (group)
    {
    case kHead:
        return head;
    case kNeck:
    case kChest:
        return chest;
    case kStomach:
    case kPelvis:
        return stomach;
    case kLArm:
    case kRArm:
        return arms;
    case kLLeg:
    case kRLeg:
        return legs;
    default:
        return chest;
    }
}

} // namespace hitbox
