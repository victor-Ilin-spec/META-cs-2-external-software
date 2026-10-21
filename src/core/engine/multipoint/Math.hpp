#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

// Isolated math. No game types, no RPM, no Source internals.
// Matrix3x4 convention (Source-style):
//   columns 0..2 = bone X/Y/Z axes in world
//   column 3     = origin
//   storage: m[row][col], row-major 3x4
// TransformPoint:  origin + x*axisX + y*axisY + z*axisZ
// TransformVector:         x*axisX + y*axisY + z*axisZ
namespace mp {

inline constexpr float kEps = 1e-6f;
inline constexpr float kLenEps = 1e-4f;
inline constexpr float kParallelAbs = 0.995f;

struct Vector3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;

    constexpr Vector3() = default;
    constexpr Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vector3 operator-() const { return { -x, -y, -z }; }
    constexpr Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    constexpr Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }
    constexpr Vector3& operator+=(const Vector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vector3& operator-=(const Vector3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline constexpr Vector3 operator*(float s, const Vector3& v) { return v * s; }

inline constexpr float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline constexpr float LengthSquared(const Vector3& v) { return Dot(v, v); }
inline float Length(const Vector3& v) { return std::sqrt(LengthSquared(v)); }

inline constexpr float DistanceSquared(const Vector3& a, const Vector3& b)
{
    return LengthSquared(a - b);
}

inline float Distance(const Vector3& a, const Vector3& b)
{
    return std::sqrt(DistanceSquared(a, b));
}

inline bool IsFinite(float v)
{
    return std::isfinite(v);
}

inline bool IsFinite(const Vector3& v)
{
    return IsFinite(v.x) && IsFinite(v.y) && IsFinite(v.z);
}

inline float Clamp(float v, float lo, float hi)
{
    if (hi < lo)
        std::swap(lo, hi);
    return std::clamp(v, lo, hi);
}

inline std::optional<Vector3> NormalizeSafe(const Vector3& v, float eps = kLenEps)
{
    const float ls = LengthSquared(v);
    if (!std::isfinite(ls) || ls < eps * eps)
        return std::nullopt;
    const float inv = 1.f / std::sqrt(ls);
    const Vector3 n{ v.x * inv, v.y * inv, v.z * inv };
    if (!IsFinite(n))
        return std::nullopt;
    return n;
}

inline Vector3 NormalizeOr(const Vector3& v, const Vector3& fallback, float eps = kLenEps)
{
    if (const auto n = NormalizeSafe(v, eps))
        return *n;
    return fallback;
}

struct Matrix3x4 {
    float m[3][4]{};

    static Matrix3x4 Identity()
    {
        Matrix3x4 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.f;
        return r;
    }

    // Columns 0..2 = axes, column 3 = origin.
    static Matrix3x4 FromAxesOrigin(const Vector3& x, const Vector3& y, const Vector3& z, const Vector3& o)
    {
        Matrix3x4 r{};
        r.m[0][0] = x.x; r.m[0][1] = y.x; r.m[0][2] = z.x; r.m[0][3] = o.x;
        r.m[1][0] = x.y; r.m[1][1] = y.y; r.m[1][2] = z.y; r.m[1][3] = o.y;
        r.m[2][0] = x.z; r.m[2][1] = y.z; r.m[2][2] = z.z; r.m[2][3] = o.z;
        return r;
    }

    Vector3 AxisX() const { return { m[0][0], m[1][0], m[2][0] }; }
    Vector3 AxisY() const { return { m[0][1], m[1][1], m[2][1] }; }
    Vector3 AxisZ() const { return { m[0][2], m[1][2], m[2][2] }; }
    Vector3 Origin() const { return { m[0][3], m[1][3], m[2][3] }; }

    bool Finite() const
    {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                if (!std::isfinite(m[r][c]))
                    return false;
        return true;
    }
};

inline Vector3 TransformVector(const Matrix3x4& mtx, const Vector3& v)
{
    return {
        mtx.m[0][0] * v.x + mtx.m[0][1] * v.y + mtx.m[0][2] * v.z,
        mtx.m[1][0] * v.x + mtx.m[1][1] * v.y + mtx.m[1][2] * v.z,
        mtx.m[2][0] * v.x + mtx.m[2][1] * v.y + mtx.m[2][2] * v.z
    };
}

inline Vector3 TransformPoint(const Matrix3x4& mtx, const Vector3& v)
{
    return TransformVector(mtx, v) + mtx.Origin();
}

// Rotation about world Z by degrees. Origin unchanged.
inline Matrix3x4 RotateZ(const Matrix3x4& src, float degrees)
{
    const float rad = degrees * 0.01745329252f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const Vector3 x = src.AxisX();
    const Vector3 y = src.AxisY();
    const Vector3 z = src.AxisZ();
    const auto rot = [](const Vector3& v, float c, float s) {
        return Vector3{ v.x * c - v.y * s, v.x * s + v.y * c, v.z };
    };
    return Matrix3x4::FromAxesOrigin(rot(x, c, s), rot(y, c, s), rot(z, c, s), src.Origin());
}

inline Matrix3x4 RotateX(const Matrix3x4& src, float degrees)
{
    const float rad = degrees * 0.01745329252f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const auto rot = [](const Vector3& v, float c, float s) {
        return Vector3{ v.x, v.y * c - v.z * s, v.y * s + v.z * c };
    };
    return Matrix3x4::FromAxesOrigin(rot(src.AxisX(), c, s), rot(src.AxisY(), c, s),
        rot(src.AxisZ(), c, s), src.Origin());
}

// Distance from point to finite segment [a,b]. Degenerate segment → distance to a.
inline float DistanceToSegment(const Vector3& point, const Vector3& a, const Vector3& b)
{
    const Vector3 ab = b - a;
    const float ab2 = LengthSquared(ab);
    if (ab2 < kLenEps * kLenEps)
        return Distance(point, a);
    float t = Dot(point - a, ab) / ab2;
    t = Clamp(t, 0.f, 1.f);
    const Vector3 closest = a + ab * t;
    return Distance(point, closest);
}

} // namespace mp
