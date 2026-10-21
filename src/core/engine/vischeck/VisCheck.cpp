#include "VisCheck.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>

namespace {
    constexpr size_t LEAF_THRESHOLD = 32;

    float CenterOnAxis(const TriangleCombined& tri, int axis)
    {
        if (axis == 0)
            return (tri.v0.x + tri.v1.x + tri.v2.x) * (1.f / 3.f);
        if (axis == 1)
            return (tri.v0.y + tri.v1.y + tri.v2.y) * (1.f / 3.f);
        return (tri.v0.z + tri.v1.z + tri.v2.z) * (1.f / 3.f);
    }
}

VisCheck::VisCheck(const std::string& optimizedGeometryFile)
{
    Load(optimizedGeometryFile);
}

void VisCheck::Unload()
{
    bvhNodes.clear();
    bvhRoot = ~0u;
    geometry.Clear();
    loaded = false;
}

bool VisCheck::BuildWorld()
{
    size_t total = 0;
    for (const auto& mesh : geometry.meshes)
        total += mesh.size();

    std::vector<TriangleCombined> world;
    world.reserve(total);
    for (auto& mesh : geometry.meshes)
    {
        world.insert(world.end(), mesh.begin(), mesh.end());
        mesh.clear();
        mesh.shrink_to_fit();
    }
    geometry.meshes.clear();
    geometry.meshes.shrink_to_fit();
    if (world.empty())
        return false;

    geometry.meshes.push_back(std::move(world));
    bvhNodes.clear();
    bvhNodes.reserve((geometry.meshes[0].size() / LEAF_THRESHOLD) * 2 + 16);
    bvhRoot = BuildBVH(geometry.meshes[0], 0, geometry.meshes[0].size());
    loaded = bvhRoot != ~0u;
    return loaded;
}

bool VisCheck::Load(const std::string& optimizedGeometryFile)
{
    Unload();
    if (optimizedGeometryFile.empty() || !geometry.LoadFromFile(optimizedGeometryFile))
        return false;
    return BuildWorld();
}

bool VisCheck::LoadPhys(const std::string& physFile, bool include_hulls)
{
    Unload();
    if (physFile.empty() || !geometry.LoadFromPhys(physFile, include_hulls))
        return false;
    return BuildWorld();
}

uint32_t VisCheck::BuildBVH(std::vector<TriangleCombined>& tris, size_t begin, size_t end)
{
    if (begin >= end)
        return ~0u;

    const uint32_t idx = static_cast<uint32_t>(bvhNodes.size());
    bvhNodes.push_back({});

    AABB bounds = tris[begin].ComputeAABB();
    for (size_t i = begin + 1; i < end; ++i)
    {
        const AABB triAABB = tris[i].ComputeAABB();
        bounds.min.x = std::min(bounds.min.x, triAABB.min.x);
        bounds.min.y = std::min(bounds.min.y, triAABB.min.y);
        bounds.min.z = std::min(bounds.min.z, triAABB.min.z);
        bounds.max.x = std::max(bounds.max.x, triAABB.max.x);
        bounds.max.y = std::max(bounds.max.y, triAABB.max.y);
        bounds.max.z = std::max(bounds.max.z, triAABB.max.z);
    }

    const uint32_t count = static_cast<uint32_t>(end - begin);
    bvhNodes[idx].bounds = bounds;
    bvhNodes[idx].first = static_cast<uint32_t>(begin);
    bvhNodes[idx].count = count;
    bvhNodes[idx].left = ~0u;
    bvhNodes[idx].right = ~0u;

    if (count <= LEAF_THRESHOLD)
        return idx;

    const Vector3 diff = bounds.max - bounds.min;
    const int axis = (diff.x > diff.y && diff.x > diff.z) ? 0 : ((diff.y > diff.z) ? 1 : 2);
    std::nth_element(tris.begin() + static_cast<std::ptrdiff_t>(begin),
        tris.begin() + static_cast<std::ptrdiff_t>(begin + count / 2),
        tris.begin() + static_cast<std::ptrdiff_t>(end),
        [axis](const TriangleCombined& a, const TriangleCombined& b) {
            return CenterOnAxis(a, axis) < CenterOnAxis(b, axis);
        });

    const size_t mid = begin + count / 2;
    const uint32_t left = BuildBVH(tris, begin, mid);
    const uint32_t right = BuildBVH(tris, mid, end);
    bvhNodes[idx].left = left;
    bvhNodes[idx].right = right;
    return idx;
}

bool VisCheck::IntersectBVH(uint32_t node, const std::vector<TriangleCombined>& tris,
    const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance)
{
    if (node == ~0u || node >= bvhNodes.size())
        return false;

    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = node;

    while (sp > 0)
    {
        const uint32_t idx = stack[--sp];
        if (idx == ~0u || idx >= bvhNodes.size())
            continue;
        const BVHNode& n = bvhNodes[idx];
        if (!n.bounds.RayIntersects(rayOrigin, rayDir, maxDistance))
            continue;

        if (n.IsLeaf())
        {
            const size_t last = static_cast<size_t>(n.first) + n.count;
            for (size_t i = n.first; i < last && i < tris.size(); ++i)
            {
                float t = 0.f;
                if (RayIntersectsTriangle(rayOrigin, rayDir, tris[i], t) && t < maxDistance)
                {
                    hitDistance = t;
                    return true;
                }
            }
            continue;
        }

        float tL = 0.f, tR = 0.f;
        const bool hitL = n.left != ~0u && n.left < bvhNodes.size() &&
            bvhNodes[n.left].bounds.RayIntersects(rayOrigin, rayDir, maxDistance, &tL);
        const bool hitR = n.right != ~0u && n.right < bvhNodes.size() &&
            bvhNodes[n.right].bounds.RayIntersects(rayOrigin, rayDir, maxDistance, &tR);
        if (hitL && hitR)
        {
            if (sp + 2 > 64)
                continue;
            if (tL > tR)
            {
                stack[sp++] = n.left;
                stack[sp++] = n.right;
            }
            else
            {
                stack[sp++] = n.right;
                stack[sp++] = n.left;
            }
        }
        else if (hitL && sp < 64)
            stack[sp++] = n.left;
        else if (hitR && sp < 64)
            stack[sp++] = n.right;
    }
    return false;
}

bool VisCheck::IsPointVisible(const Vector3& point1, const Vector3& point2)
{
    if (!loaded || bvhRoot == ~0u || geometry.meshes.empty())
        return false;

    Vector3 rayDir = { point2.x - point1.x, point2.y - point1.y, point2.z - point1.z };
    const float distance = std::sqrt(rayDir.dot(rayDir));
    if (distance <= 1e-4f)
        return true;

    rayDir = { rayDir.x / distance, rayDir.y / distance, rayDir.z / distance };
    float hitDistance = distance;
    return !IntersectBVH(bvhRoot, geometry.meshes[0], point1, rayDir, distance, hitDistance);
}

bool VisCheck::TraceRay(const Vector3& origin, const Vector3& dir, float maxDistance, float& hitDistance)
{
    if (!loaded || bvhRoot == ~0u || geometry.meshes.empty() || maxDistance <= 1e-4f)
        return false;

    const float len = std::sqrt(dir.dot(dir));
    if (len <= 1e-6f)
        return false;

    const Vector3 rayDir{ dir.x / len, dir.y / len, dir.z / len };
    hitDistance = maxDistance;
    return IntersectBVH(bvhRoot, geometry.meshes[0], origin, rayDir, maxDistance, hitDistance);
}

bool VisCheck::RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir, const TriangleCombined& triangle, float& t)
{
    constexpr float EPSILON = 1e-7f;

    const Vector3 edge1 = triangle.v1 - triangle.v0;
    const Vector3 edge2 = triangle.v2 - triangle.v0;
    const Vector3 h = rayDir.cross(edge2);
    const float a = edge1.dot(h);
    if (a > -EPSILON && a < EPSILON)
        return false;

    const float f = 1.0f / a;
    const Vector3 s = rayOrigin - triangle.v0;
    const float u = f * s.dot(h);
    if (u < 0.0f || u > 1.0f)
        return false;

    const Vector3 q = s.cross(edge1);
    const float v = f * rayDir.dot(q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * edge2.dot(q);
    return t > EPSILON;
}

bool VisCheck::TraceClosest(const Vector3& origin, const Vector3& dir, float maxDistance, Hit& out)
{
    out = {};
    out.distance = maxDistance;
    if (!loaded || bvhRoot == ~0u || geometry.meshes.empty() || maxDistance <= 1e-4f)
        return false;

    const float len = std::sqrt(dir.dot(dir));
    if (len <= 1e-6f)
        return false;

    const Vector3 rayDir{ dir.x / len, dir.y / len, dir.z / len };
    if (!IntersectBVHClosest(bvhRoot, geometry.meshes[0], origin, rayDir, maxDistance, out))
        return false;

    out.end = Vector3{
        origin.x + rayDir.x * out.distance,
        origin.y + rayDir.y * out.distance,
        origin.z + rayDir.z * out.distance
    };
    return out.hit;
}

bool VisCheck::IntersectBVHClosest(uint32_t node, const std::vector<TriangleCombined>& tris,
    const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, Hit& out)
{
    if (node == ~0u || node >= bvhNodes.size())
        return false;

    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = node;
    float best = maxDistance;
    const TriangleCombined* best_tri = nullptr;

    while (sp > 0)
    {
        const uint32_t idx = stack[--sp];
        if (idx == ~0u || idx >= bvhNodes.size())
            continue;
        const BVHNode& n = bvhNodes[idx];
        if (!n.bounds.RayIntersects(rayOrigin, rayDir, best))
            continue;

        if (n.IsLeaf())
        {
            const size_t last = static_cast<size_t>(n.first) + n.count;
            for (size_t i = n.first; i < last && i < tris.size(); ++i)
            {
                float t = 0.f;
                if (RayIntersectsTriangle(rayOrigin, rayDir, tris[i], t) && t < best)
                {
                    best = t;
                    best_tri = &tris[i];
                }
            }
            continue;
        }

        float tL = 0.f, tR = 0.f;
        const bool hitL = n.left != ~0u && n.left < bvhNodes.size() &&
            bvhNodes[n.left].bounds.RayIntersects(rayOrigin, rayDir, best, &tL);
        const bool hitR = n.right != ~0u && n.right < bvhNodes.size() &&
            bvhNodes[n.right].bounds.RayIntersects(rayOrigin, rayDir, best, &tR);
        if (hitL && hitR)
        {
            if (sp + 2 > 64)
                continue;
            if (tL > tR)
            {
                stack[sp++] = n.left;
                stack[sp++] = n.right;
            }
            else
            {
                stack[sp++] = n.right;
                stack[sp++] = n.left;
            }
        }
        else if (hitL && sp < 64)
            stack[sp++] = n.left;
        else if (hitR && sp < 64)
            stack[sp++] = n.right;
    }

    if (!best_tri)
        return false;

    Vector3 e1 = best_tri->v1 - best_tri->v0;
    Vector3 e2 = best_tri->v2 - best_tri->v0;
    Vector3 nrm = e1.cross(e2);
    const float nlen = std::sqrt(nrm.dot(nrm));
    if (nlen > 1e-8f)
    {
        nrm.x /= nlen;
        nrm.y /= nlen;
        nrm.z /= nlen;
    }
    if (nrm.dot(rayDir) > 0.f)
    {
        nrm.x = -nrm.x;
        nrm.y = -nrm.y;
        nrm.z = -nrm.z;
    }

    out.hit = true;
    out.distance = best;
    out.normal = nrm;
    return true;
}

