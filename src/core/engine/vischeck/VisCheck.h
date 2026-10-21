#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "OptimizedGeometry.h"
#include "Math.hpp"

struct BVHNode {
    AABB bounds;
    uint32_t left = ~0u;
    uint32_t right = ~0u;
    uint32_t first = 0;
    uint32_t count = 0;

    bool IsLeaf() const {
        return left == ~0u;
    }
};

class VisCheck
{
public:
    VisCheck() = default;
    explicit VisCheck(const std::string& optimizedGeometryFile);

    bool Load(const std::string& optimizedGeometryFile);
    bool LoadPhys(const std::string& physFile, bool include_hulls = false);
    void Unload();
    bool Loaded() const { return loaded; }

    bool IsPointVisible(const Vector3& point1, const Vector3& point2);
    bool IsVisible(const Vector3& point1, const Vector3& point2) { return IsPointVisible(point1, point2); }
    bool TraceRay(const Vector3& origin, const Vector3& dir, float maxDistance, float& hitDistance);
    struct Hit {
        bool hit = false;
        float distance = 0.f;
        Vector3 end{};
        Vector3 normal{};
    };
    bool TraceClosest(const Vector3& origin, const Vector3& dir, float maxDistance, Hit& out);


private:
    OptimizedGeometry geometry;
    std::vector<BVHNode> bvhNodes;
    bool IntersectBVHClosest(uint32_t node, const std::vector<TriangleCombined>& tris,
        const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, Hit& out);

    uint32_t bvhRoot = ~0u;
    bool loaded = false;

    bool BuildWorld();
    uint32_t BuildBVH(std::vector<TriangleCombined>& tris, size_t begin, size_t end);
    bool IntersectBVH(uint32_t node, const std::vector<TriangleCombined>& tris,
        const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance);
    bool RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir,
        const TriangleCombined& triangle, float& t);
};
