#include "Humanize.hpp"
#include "motor_synergy.h"
#include "config/Current.hpp"
#include "common.hpp"
#include "core/engine/TickTime.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {
    std::vector<motor_synergy::trajectory_point> g_path;
    size_t g_idx = 1;
    float g_last_x = 0.f;
    float g_last_y = 0.f;
    std::chrono::steady_clock::time_point g_start{};
    bool g_active = false;
    uintptr_t g_last_id = 0;
    std::chrono::steady_clock::time_point g_last_step{};
    bool g_have_step = false;
    tick::Quantizer g_q{};
    DWORD g_last_idle_log = 0;

    FILE* LogFile()
    {
        static FILE* f = nullptr;
        static bool tried = false;
        if (tried)
            return f;
        tried = true;

        char exe[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, exe, MAX_PATH))
            return nullptr;

        char dir[MAX_PATH]{};
        strncpy_s(dir, exe, _TRUNCATE);
        for (int up = 0; up < 5; ++up)
        {
            char* slash = std::strrchr(dir, '\\');
            if (!slash)
                break;
            *slash = '\0';
            char sln[MAX_PATH]{};
            std::snprintf(sln, MAX_PATH, "%s\\cs2-external-esp.sln", dir);
            if (GetFileAttributesA(sln) != INVALID_FILE_ATTRIBUTES)
            {
                char path[MAX_PATH]{};
                std::snprintf(path, MAX_PATH, "%s\\humanize.log", dir);
                if (fopen_s(&f, path, "a") == 0 && f)
                {
                    std::setvbuf(f, nullptr, _IONBF, 0);
                    std::fprintf(f, "\n==== humanize log start exe=%s ====\n", exe);
                }
                return f;
            }
        }
        return nullptr;
    }

    void Log(const char* fmt, ...)
    {
        FILE* f = LogFile();
        if (!f)
            return;

        SYSTEMTIME st{};
        GetLocalTime(&st);
        std::fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        va_list args;
        va_start(args, fmt);
        std::vfprintf(f, fmt, args);
        va_end(args);
        std::fputc('\n', f);
    }
}

void Humanize::Reset()
{
    if (g_active || g_last_id)
        Log("reset id=%llu", static_cast<unsigned long long>(g_last_id));
    g_path.clear();
    g_idx = 1;
    g_active = false;
    g_last_id = 0;
    g_have_step = false;
    g_q.acc = 0.f;
}
void Humanize::Begin(float x0, float y0, float x1, float y1)
{
    motor_synergy::config cfg{};
    cfg.fitts_a = cfg::humanize::fitts_a;
    cfg.fitts_b = cfg::humanize::fitts_b;
    cfg.target_width = cfg::humanize::target_width;
    cfg.overshoot_prob = cfg::humanize::overshoot_prob;
    cfg.curvature_scale = cfg::humanize::curvature_scale;
    cfg.ou_sigma = cfg::humanize::ou_sigma;
    cfg.tremor_amp_max = cfg::humanize::tremor_amp;
    cfg.tremor_amp_min = cfg::humanize::tremor_amp * 0.35f;
    cfg.sdn_k = cfg::humanize::sdn_k;

    g_path = motor_synergy::generate(x0, y0, x1, y1, cfg);
    g_idx = 1;
    g_last_x = x0;
    g_last_y = y0;
    g_start = std::chrono::steady_clock::now();
    g_active = g_path.size() >= 2;
}

bool Humanize::Active()
{
    return g_active && g_idx < g_path.size();
}

bool Humanize::Tick(float& out_dx, float& out_dy)
{
    out_dx = 0.f;
    out_dy = 0.f;

    if (!g_active || g_path.size() < 2)
        return false;

    const auto now = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(now - g_start).count()
        * static_cast<double>((std::max)(0.1f, cfg::humanize::speed_scale));

    while (g_idx < g_path.size() && g_path[g_idx].t <= elapsed_ms)
        ++g_idx;

    if (g_idx >= g_path.size())
    {
        const auto& last = g_path.back();
        out_dx = static_cast<float>(last.x - g_last_x);
        out_dy = static_cast<float>(last.y - g_last_y);
        g_last_x = static_cast<float>(last.x);
        g_last_y = static_cast<float>(last.y);
        g_active = false;
        return std::fabs(out_dx) > 0.01f || std::fabs(out_dy) > 0.01f;
    }

    const size_t i1 = g_idx;
    const size_t i0 = (i1 > 0) ? i1 - 1 : 0;
    const auto& a = g_path[i0];
    const auto& b = g_path[i1];
    const double span = b.t - a.t;
    const double u = (span > 0.001) ? std::clamp((elapsed_ms - a.t) / span, 0.0, 1.0) : 1.0;
    const float px = static_cast<float>(a.x + (b.x - a.x) * u);
    const float py = static_cast<float>(a.y + (b.y - a.y) * u);
    out_dx = px - g_last_x;
    out_dy = py - g_last_y;
    g_last_x = px;
    g_last_y = py;
    return true;
}

void Humanize::Tint(float& dx, float& dy, float dt_sec)
{
    if (!cfg::humanize::enabled)
        return;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    static float ou_x = 0.f;
    static float ou_y = 0.f;
    static float t_s = 0.f;
    static float phase = 0.f;

    if (phase == 0.f)
        phase = std::uniform_real_distribution<float>(0.f, 6.2831853f)(rng);

    const float dts = std::clamp(dt_sec, 0.0005f, 0.032f);
    t_s += dts;

    std::normal_distribution<float> n01(0.f, 1.f);
    const float ou_s = (std::max)(0.f, cfg::humanize::ou_sigma);
    ou_x += -3.5f * ou_x * dts + ou_s * std::sqrt(dts) * n01(rng);
    ou_y += -3.5f * ou_y * dts + ou_s * std::sqrt(dts) * n01(rng);

    const float speed = std::sqrt(dx * dx + dy * dy) / dts;
    const float trem = (std::max)(0.f, cfg::humanize::tremor_amp)
        * (1.f / (1.f + speed * 0.02f))
        * std::sin(6.2831853f * 10.f * t_s + phase);
    const float sdn = (std::max)(0.f, cfg::humanize::sdn_k) * speed * 0.015f;

    float nx = ou_x + trem + sdn * n01(rng);
    float ny = ou_y + trem * 0.7f + sdn * n01(rng);

    const float cap = (std::max)(0.35f, 0.22f * std::sqrt(dx * dx + dy * dy));
    const float nlen = std::sqrt(nx * nx + ny * ny);
    if (nlen > cap && nlen > 1e-4f)
    {
        nx *= cap / nlen;
        ny *= cap / nlen;
    }

    dx += nx;
    dy += ny;

    Log("tint step=%.2f,%.2f noise=%.2f,%.2f dt=%.1fms", dx, dy, nx, ny, dts * 1000.f);
}
