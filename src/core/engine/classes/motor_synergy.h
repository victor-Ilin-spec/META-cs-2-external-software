#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

namespace motor_synergy {

struct trajectory_point {
    double x = 0.0;
    double y = 0.0;
    double t = 0.0;
};

struct config {
    double fitts_a = 50.0;
    double fitts_b = 150.0;
    double target_width = 20.0;

    double undershoot_min = 0.92;
    double undershoot_max = 0.97;
    double peak_time_ratio = 0.35;
    double primary_sigma_min = 0.18;
    double primary_sigma_max = 0.28;

    double overshoot_prob = 0.15;
    double overshoot_min = 1.02;
    double overshoot_max = 1.08;
    double correction_sigma_min = 0.12;
    double correction_sigma_max = 0.20;
    double second_correction_prob = 0.25;

    double curvature_scale = 0.025;

    double ou_theta = 3.5;
    double ou_sigma = 1.2;

    double tremor_freq_min = 8.0;
    double tremor_freq_max = 12.0;
    double tremor_amp_min = 0.15;
    double tremor_amp_max = 0.55;

    double sdn_k = 0.04;

    double sample_dt_mean = 7.8;
    double gamma_shape = 3.5;
};

namespace detail {

inline double normal_cdf(double x)
{
    return 0.5 * (1.0 + std::erf(x / std::numbers::sqrt2));
}

inline double lognormal_cdf(double t, double t0, double mu, double sigma)
{
    if (t <= t0)
        return 0.0;
    return normal_cdf((std::log(t - t0) - mu) / sigma);
}

inline double lognormal_pdf(double t, double t0, double mu, double sigma)
{
    if (t <= t0)
        return 0.0;
    const double dt = t - t0;
    const double z = (std::log(dt) - mu) / sigma;
    return 1.0 / (sigma * std::sqrt(2.0 * std::numbers::pi) * dt) * std::exp(-0.5 * z * z);
}

inline double curvature_profile(double s)
{
    if (s <= 0.0 || s >= 1.0)
        return 0.0;
    const double v = s * s * (1.0 - s) * (1.0 - s) * (1.0 - s);
    constexpr double norm = 0.4 * 0.4 * 0.6 * 0.6 * 0.6;
    return v / norm;
}

inline double direction_factor(double angle)
{
    const double sa = std::abs(std::sin(angle));
    const double ca = std::abs(std::cos(angle));
    return 0.5 + 0.8 * sa - 0.15 * ca;
}

} // namespace detail

inline std::vector<trajectory_point> generate(
    double x0, double y0, double x1, double y1,
    const config& cfg = {}, uint64_t seed = 0)
{
    std::mt19937_64 rng(seed ? seed : std::random_device{}());
    auto uniform = [&](double lo, double hi) {
        return std::uniform_real_distribution<double>(lo, hi)(rng);
    };
    auto normal = [&](double m, double s) {
        return std::normal_distribution<double>(m, s)(rng);
    };
    auto gamma_dist = [&](double shape, double scale) {
        return std::gamma_distribution<double>(shape, scale)(rng);
    };

    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double distance = std::hypot(dx, dy);
    const double direction = std::atan2(dy, dx);

    if (distance < 1.0)
        return { { x0, y0, 0.0 }, { x1, y1, 50.0 } };

    const double tx = dx / distance;
    const double ty = dy / distance;
    const double nx = -ty;
    const double ny = tx;

    const double id = std::log2(distance / cfg.target_width + 1.0);
    double mt = (cfg.fitts_a + cfg.fitts_b * id) * std::exp(normal(0.0, 0.08));
    mt = std::max(mt, 80.0);

    const bool overshoot = uniform(0.0, 1.0) < cfg.overshoot_prob;
    const double reach = overshoot
        ? uniform(cfg.overshoot_min, cfg.overshoot_max)
        : uniform(cfg.undershoot_min, cfg.undershoot_max);

    const double primary_D = distance * reach;
    const double primary_sigma = uniform(cfg.primary_sigma_min, cfg.primary_sigma_max);
    const double peak_t = mt * uniform(cfg.peak_time_ratio - 0.03, cfg.peak_time_ratio + 0.03);
    const double primary_mu = std::log(peak_t) + primary_sigma * primary_sigma;

    struct correction {
        double D, t0, mu, sigma, dir_x, dir_y;
    };
    std::vector<correction> corrections;

    const double remaining = distance - primary_D;
    if (std::abs(remaining) > 0.5)
    {
        const double dir = remaining > 0.0 ? 1.0 : -1.0;
        const double cD = std::abs(remaining) * uniform(0.88, 1.02);
        const double cS = uniform(cfg.correction_sigma_min, cfg.correction_sigma_max);
        const double cPeak = mt * uniform(0.12, 0.18);
        corrections.push_back({
            cD, mt * uniform(0.55, 0.68),
            std::log(cPeak) + cS * cS, cS,
            tx * dir, ty * dir
        });

        const double left = remaining - cD * dir;
        if (std::abs(left) > 0.3 && uniform(0.0, 1.0) < cfg.second_correction_prob)
        {
            const double d2 = left > 0.0 ? 1.0 : -1.0;
            const double cD2 = std::abs(left) * uniform(0.85, 1.05);
            const double cS2 = uniform(0.10, 0.16);
            const double cP2 = mt * uniform(0.08, 0.12);
            corrections.push_back({
                cD2, mt * uniform(0.78, 0.88),
                std::log(cP2) + cS2 * cS2, cS2,
                tx * d2, ty * d2
            });
        }
    }

    const double curv_amp = distance * cfg.curvature_scale
        * detail::direction_factor(direction) * normal(0.0, 1.0);

    const double tremor_freq = uniform(cfg.tremor_freq_min, cfg.tremor_freq_max);
    const double tremor_amp = uniform(cfg.tremor_amp_min, cfg.tremor_amp_max);
    const double tph_x = uniform(0.0, 2.0 * std::numbers::pi);
    const double tph_y = uniform(0.0, 2.0 * std::numbers::pi);
    double ou_x = 0.0;
    double ou_y = 0.0;

    const double total_t = mt * 1.15;
    const double g_scale = cfg.sample_dt_mean / cfg.gamma_shape;

    std::vector<double> times = { 0.0 };
    for (double t = 0.0; t < total_t;)
    {
        const double dt = std::clamp(gamma_dist(cfg.gamma_shape, g_scale), 2.0, 25.0);
        t += dt;
        if (t <= total_t + 15.0)
            times.push_back(t);
    }

    std::vector<trajectory_point> result;
    result.reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i)
    {
        const double t = times[i];
        const double dt_ms = (i > 0) ? (t - times[i - 1]) : cfg.sample_dt_mean;
        const double dt_s = dt_ms / 1000.0;

        const double s = detail::lognormal_cdf(t, 0.0, primary_mu, primary_sigma);

        double bx = x0 + tx * primary_D * s;
        double by = y0 + ty * primary_D * s;

        bx += nx * curv_amp * detail::curvature_profile(s);
        by += ny * curv_amp * detail::curvature_profile(s);

        for (const auto& c : corrections)
        {
            const double cs = detail::lognormal_cdf(t, c.t0, c.mu, c.sigma);
            bx += c.dir_x * c.D * cs;
            by += c.dir_y * c.D * cs;
        }

        double speed = primary_D * detail::lognormal_pdf(t, 0.0, primary_mu, primary_sigma);
        for (const auto& c : corrections)
            speed += c.D * detail::lognormal_pdf(t, c.t0, c.mu, c.sigma);

        ou_x += -cfg.ou_theta * ou_x * dt_s + cfg.ou_sigma * std::sqrt(dt_s) * normal(0.0, 1.0);
        ou_y += -cfg.ou_theta * ou_y * dt_s + cfg.ou_sigma * std::sqrt(dt_s) * normal(0.0, 1.0);

        const double t_s = t / 1000.0;
        const double trem_mod = 1.0 / (1.0 + speed * 0.3);
        const double tr_x = tremor_amp * trem_mod
            * std::sin(2.0 * std::numbers::pi * tremor_freq * t_s + tph_x);
        const double tr_y = tremor_amp * trem_mod
            * std::sin(2.0 * std::numbers::pi * tremor_freq * t_s + tph_y);

        const double sdn_x = cfg.sdn_k * speed * normal(0.0, 1.0);
        const double sdn_y = cfg.sdn_k * speed * normal(0.0, 1.0);

        result.push_back({ bx + ou_x + tr_x + sdn_x, by + ou_y + tr_y + sdn_y, t });
    }

    return result;
}

} // namespace motor_synergy
