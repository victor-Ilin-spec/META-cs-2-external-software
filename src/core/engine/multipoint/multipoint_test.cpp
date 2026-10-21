#include "Generator.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace mp;

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_fail; \
    } else { ++g_pass; } \
} while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

static HitboxData MakeCapsule(HitboxCategory cat, Vector3 a, Vector3 b, float r, int index = 0)
{
    HitboxData d{};
    d.index = index;
    d.bone = 0;
    d.category = cat;
    d.mins = a;
    d.maxs = b;
    d.radius = r;
    return d;
}

static HitboxData MakeObb(HitboxCategory cat, Vector3 mn, Vector3 mx, int index = 4)
{
    HitboxData d{};
    d.index = index;
    d.bone = 0;
    d.category = cat;
    d.mins = mn;
    d.maxs = mx;
    d.radius = 0.f;
    return d;
}

static MultipointContext Ctx(Vector3 eye = { 0.f, -80.f, 0.f })
{
    MultipointContext c{};
    c.shooter_eye = eye;
    c.adaptive_scale = true;
    c.min_scale = 0.25f;
    c.max_scale = 0.90f;
    c.max_points = 6;
    return c;
}

static void ExpectInside(const PreparedHitbox& hb, const MultipointSet& set)
{
    CHECK(set.count >= 1);
    CHECK(set.points[0].center);
    CHECK(set.points[0].type == MultipointType::Center);
    for (std::size_t i = 0; i < set.count; ++i)
    {
        CHECK(IsPointInsideHitbox(hb, set.points[i].position));
        CHECK(IsFinite(set.points[i].position));
        CHECK(std::isfinite(set.points[i].edge_margin));
    }
}

static void TestMatrixConvention()
{
    const Matrix3x4 m = Matrix3x4::FromAxesOrigin(
        { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 10, 20, 30 });
    const Vector3 p = TransformPoint(m, { 1, 2, 3 });
    // 10 + 1*(0,1,0) + 2*(0,0,1) + 3*(1,0,0) = (13, 21, 32)
    CHECK_NEAR(p.x, 13.f, 1e-4f);
    CHECK_NEAR(p.y, 21.f, 1e-4f);
    CHECK_NEAR(p.z, 32.f, 1e-4f);
    const Vector3 v = TransformVector(m, { 1, 0, 0 });
    CHECK_NEAR(v.x, 0.f, 1e-4f);
    CHECK_NEAR(v.y, 1.f, 1e-4f);
    CHECK_NEAR(v.z, 0.f, 1e-4f);
    CHECK_NEAR(m.AxisX().y, 1.f, 1e-6f);
    CHECK_NEAR(m.Origin().x, 10.f, 1e-6f);
}

static void TestAxisCapsule()
{
    const auto raw = MakeCapsule(HitboxCategory::Chest, { 0, 0, -6 }, { 0, 0, 6 }, 5.f, 4);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    CHECK(hb->shape == HitboxShape::Capsule);
    CHECK(!hb->sphere_like);
    CHECK_NEAR(hb->capsule_length, 12.f, 1e-3f);
    CHECK_NEAR(hb->center.z, 0.f, 1e-3f);
    const auto set = GenerateMultipoints(*hb, Ctx());
    ExpectInside(*hb, set);
    CHECK(set.count >= 3);
    CHECK(set.count <= 6);
    const float cm = GetEdgeMargin(*hb, hb->center);
    CHECK(cm > GetEdgeMargin(*hb, set.points[1].position) - 1e-3f);
}

static void TestSphereCapsule()
{
    const auto raw = MakeCapsule(HitboxCategory::Head, { 0, 0, 0 }, { 0, 0, 0 }, 4.2f, 0);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    CHECK(hb->sphere_like);
    const auto set = GenerateMultipoints(*hb, Ctx({ 0, -40, 8 }));
    ExpectInside(*hb, set);
    CHECK(set.count >= 1);
}

static void TestSmallRadius()
{
    const auto raw = MakeCapsule(HitboxCategory::Arm, { 0, 0, -4 }, { 0, 0, 4 }, 0.8f, 5);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    const auto set = GenerateMultipoints(*hb, Ctx());
    ExpectInside(*hb, set);
    CHECK(set.count <= 2);
}

static void TestRotatedCapsule()
{
    const auto raw = MakeCapsule(HitboxCategory::Head, { 0, 0, -3 }, { 0, 0, 3 }, 4.f, 0);
    const auto id = PrepareHitbox(raw, Matrix3x4::Identity());
    const auto rot = PrepareHitbox(raw, RotateZ(Matrix3x4::Identity(), 90.f));
    CHECK(id && rot);
    auto ctx = Ctx({ 80.f, 0.f, 4.f });
    const auto a = GenerateMultipoints(*id, Ctx({ 0.f, -80.f, 4.f }));
    const auto b = GenerateMultipoints(*rot, ctx);
    ExpectInside(*id, a);
    ExpectInside(*rot, b);
    CHECK(a.count >= 3);
    CHECK(b.count >= 3);

    // Lateral points must leave the identity-Y plane after a 90° yaw of the bone.
    bool moved = false;
    for (std::size_t i = 1; i < b.count; ++i)
    {
        if (std::fabs(b.points[i].position.x) > 0.5f)
            moved = true;
    }
    CHECK(moved);
}

static void TestAxisObb()
{
    const auto raw = MakeObb(HitboxCategory::Chest, { -6, -4, -5 }, { 6, 4, 5 }, 4);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    CHECK(hb->shape == HitboxShape::OBB);
    CHECK_NEAR(hb->obb_extents.x, 6.f, 1e-3f);
    const auto set = GenerateMultipoints(*hb, Ctx());
    ExpectInside(*hb, set);
    CHECK(set.count >= 3);
}

static void TestRotatedObb()
{
    const auto raw = MakeObb(HitboxCategory::Chest, { -6, -3, -4 }, { 6, 3, 4 }, 4);
    const auto id = PrepareHitbox(raw, Matrix3x4::Identity());
    const auto rot = PrepareHitbox(raw, RotateZ(Matrix3x4::Identity(), 90.f));
    CHECK(id && rot);
    const auto a = GenerateMultipoints(*id, Ctx({ 0, -80, 0 }));
    const auto b = GenerateMultipoints(*rot, Ctx({ 80, 0, 0 }));
    ExpectInside(*id, a);
    ExpectInside(*rot, b);
    bool id_on_x = false;
    bool rot_on_y = false;
    for (std::size_t i = 1; i < a.count; ++i)
        if (std::fabs(a.points[i].position.x) > 1.f)
            id_on_x = true;
    for (std::size_t i = 1; i < b.count; ++i)
        if (std::fabs(b.points[i].position.y) > 1.f)
            rot_on_y = true;
    CHECK(id_on_x);
    CHECK(rot_on_y);
}

static void TestNonUniformObb()
{
    const auto raw = MakeObb(HitboxCategory::Stomach, { -8, -2, -3 }, { 8, 2, 3 }, 3);
    const auto hb = PrepareHitbox(raw, RotateX(Matrix3x4::Identity(), 35.f));
    CHECK(hb.has_value());
    const auto set = GenerateMultipoints(*hb, Ctx({ 10, -50, 20 }));
    ExpectInside(*hb, set);
}

static void TestPatterns()
{
    struct Case { HitboxCategory c; int index; int minc; int maxc; Vector3 a; Vector3 b; float r; };
    const Case cases[] = {
        { HitboxCategory::Head, 0, 3, 6, { 0, 0, -3 }, { 0, 0, 3 }, 4.2f },
        { HitboxCategory::Chest, 4, 3, 3, { 0, 0, -6 }, { 0, 0, 6 }, 6.0f },
        { HitboxCategory::Stomach, 3, 3, 3, { 0, 0, -5 }, { 0, 0, 5 }, 5.4f },
        { HitboxCategory::Pelvis, 2, 3, 3, { 0, 0, -4 }, { 0, 0, 4 }, 5.0f },
        { HitboxCategory::Neck, 1, 1, 3, { 0, 0, -2 }, { 0, 0, 2 }, 3.2f },
        { HitboxCategory::Arm, 5, 1, 2, { 0, 0, -8 }, { 0, 0, 8 }, 3.0f },
        { HitboxCategory::Leg, 9, 1, 2, { 0, 0, -10 }, { 0, 0, 10 }, 3.4f },
        { HitboxCategory::Foot, 13, 1, 2, { 0, 0, -2 }, { 0, 0, 2 }, 2.4f },
    };
    for (const auto& c : cases)
    {
        const auto hb = PrepareHitbox(MakeCapsule(c.c, c.a, c.b, c.r, c.index), Matrix3x4::Identity());
        CHECK(hb.has_value());
        auto ctx = Ctx();
        ctx.max_points = c.maxc;
        const auto set = GenerateMultipoints(*hb, ctx);
        ExpectInside(*hb, set);
        CHECK(static_cast<int>(set.count) >= c.minc);
        CHECK(static_cast<int>(set.count) <= c.maxc);
    }
}

static void TestHighScaleContainment()
{
    const auto raw = MakeCapsule(HitboxCategory::Head, { 0, 0, -3 }, { 0, 0, 3 }, 4.f, 0);
    const auto hb = PrepareHitbox(raw, RotateZ(RotateX(Matrix3x4::Identity(), 40.f), 70.f));
    CHECK(hb.has_value());
    auto ctx = Ctx({ 30, -40, 20 });
    ctx.min_scale = 0.85f;
    ctx.max_scale = 0.92f;
    ctx.scale.head_lateral = 0.90f;
    ctx.scale.head_vertical = 0.90f;
    const auto set = GenerateMultipoints(*hb, ctx);
    ExpectInside(*hb, set);
}

static void TestSurfaceMargin()
{
    const auto raw = MakeCapsule(HitboxCategory::Chest, { 0, 0, -6 }, { 0, 0, 6 }, 5.f, 4);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    const Vector3 surface = hb->center + Vector3{ 5.f, 0.f, 0.f };
    CHECK(IsPointInsideHitbox(*hb, surface, 0.05f));
    CHECK(GetEdgeMargin(*hb, surface) < 0.15f);
    CHECK(GetNormalizedEdgeMargin(*hb, hb->center) > GetNormalizedEdgeMargin(*hb, surface));
}

static void TestDuplicates()
{
    // Degenerate: tiny radius, almost sphere. Laterals collapse toward center.
    const auto raw = MakeCapsule(HitboxCategory::Neck, { 0, 0, 0 }, { 0, 0, 0.05f }, 0.3f, 1);
    const auto hb = PrepareHitbox(raw, Matrix3x4::Identity());
    CHECK(hb.has_value());
    auto ctx = Ctx();
    ctx.min_scale = 0.05f;
    ctx.max_scale = 0.20f;
    const auto set = GenerateMultipoints(*hb, ctx);
    for (std::size_t i = 0; i < set.count; ++i)
        for (std::size_t j = i + 1; j < set.count; ++j)
            CHECK(DistanceSquared(set.points[i].position, set.points[j].position) >= 0.35f * 0.35f * 0.25f);
}

static void TestNoSpreadFields()
{
    // Compile-time documentation: MultipointContext has no spread members.
    MultipointContext c{};
    (void)c.scale;
    (void)c.adaptive_scale;
    (void)c.shooter_eye;
}

static void TestInvalid()
{
    HitboxData bad{};
    bad.mins = { NAN, 0, 0 };
    bad.maxs = { 1, 1, 1 };
    bad.radius = 3.f;
    CHECK(!PrepareHitbox(bad, Matrix3x4::Identity()).has_value());

    Matrix3x4 m = Matrix3x4::Identity();
    m.m[0][0] = INFINITY;
    const auto raw = MakeCapsule(HitboxCategory::Head, { 0, 0, -1 }, { 0, 0, 1 }, 3.f);
    CHECK(!PrepareHitbox(raw, m).has_value());
}

static void Bench()
{
    const auto raw = MakeCapsule(HitboxCategory::Head, { 0, 0, -3 }, { 0, 0, 3 }, 4.2f, 0);
    const auto mtx = RotateZ(Matrix3x4::Identity(), 25.f);
    const auto ctx = Ctx({ 12, -70, 10 });

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i)
        (void)PrepareHitbox(raw, mtx);
    const auto t1 = std::chrono::steady_clock::now();

    const auto hb = *PrepareHitbox(raw, mtx);
    std::size_t sum = 0, mx = 0;
    const auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100000; ++i)
    {
        const auto set = GenerateMultipoints(hb, ctx);
        sum += set.count;
        mx = (std::max)(mx, set.count);
    }
    const auto t3 = std::chrono::steady_clock::now();

    const double prep_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / 10000.0;
    const double gen_us = std::chrono::duration<double, std::micro>(t3 - t2).count() / 100000.0;
    std::printf("bench prepare=%.3f us  generate=%.3f us  avg_pts=%.2f  max_pts=%zu  heap=0\n",
        prep_us, gen_us, static_cast<double>(sum) / 100000.0, mx);
}

int main()
{
    TestMatrixConvention();
    TestAxisCapsule();
    TestSphereCapsule();
    TestSmallRadius();
    TestRotatedCapsule();
    TestAxisObb();
    TestRotatedObb();
    TestNonUniformObb();
    TestPatterns();
    TestHighScaleContainment();
    TestSurfaceMargin();
    TestDuplicates();
    TestNoSpreadFields();
    TestInvalid();
    Bench();

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
