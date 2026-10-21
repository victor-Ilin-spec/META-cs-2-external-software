#pragma once

#include <algorithm>
#include <limits>
#include <cmath>
#include <initializer_list>

struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    float dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3 cross(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};

struct Triangle {
    int a, b, c;
    Triangle() : a(0), b(0), c(0) {}
    Triangle(int a_, int b_, int c_) : a(a_), b(b_), c(c_) {}
};

struct AABB {
    Vector3 min;
    Vector3 max;

    bool RayIntersects(const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float* t_enter = nullptr) const {
        float tmin = 0.f;
        float tmax = maxDistance;

        const float* rayOriginArr = &rayOrigin.x;
        const float* rayDirArr = &rayDir.x;
        const float* minArr = &min.x;
        const float* maxArr = &max.x;

        for (int i = 0; i < 3; ++i) {
            const float dir = rayDirArr[i];
            if (dir > -1.0e-8f && dir < 1.0e-8f) {
                if (rayOriginArr[i] < minArr[i] || rayOriginArr[i] > maxArr[i])
                    return false;
                continue;
            }
            const float invDir = 1.0f / dir;
            float t0 = (minArr[i] - rayOriginArr[i]) * invDir;
            float t1 = (maxArr[i] - rayOriginArr[i]) * invDir;
            if (t0 > t1)
                std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmin > tmax)
                return false;
        }

        if (t_enter)
            *t_enter = tmin;
        return true;
    }
};

struct TriangleCombined {
    Vector3 v0, v1, v2;
    int materialIndex = -1;

    TriangleCombined() = default;
    TriangleCombined(const Vector3& v0_, const Vector3& v1_, const Vector3& v2_)
        : v0(v0_), v1(v1_), v2(v2_) {
    }

    AABB ComputeAABB() const {
        Vector3 min_point, max_point;

        min_point.x = std::min({ v0.x, v1.x, v2.x });
        min_point.y = std::min({ v0.y, v1.y, v2.y });
        min_point.z = std::min({ v0.z, v1.z, v2.z });

        max_point.x = std::max({ v0.x, v1.x, v2.x });
        max_point.y = std::max({ v0.y, v1.y, v2.y });
        max_point.z = std::max({ v0.z, v1.z, v2.z });

        return { min_point, max_point };
    }
};
