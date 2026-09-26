#pragma once
#include "Params.h"
#include <algorithm>
#include <cmath>

namespace stellar {

inline float finite_or(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}

inline float finite_clamp(float v, float lo, float hi, float fallback) {
    return std::clamp(finite_or(v, fallback), lo, hi);
}

inline Color3f sanitize_color(Color3f c, Color3f fallback) {
    // AE's floating color suite can legitimately return over-range and
    // under-range working-space values. Keep those for HDR fidelity while
    // bounding corrupt project data to the finite range representable by FP16.
    constexpr float kColorLimit = 65504.0f;
    c.r = finite_clamp(c.r, -kColorLimit, kColorLimit, fallback.r);
    c.g = finite_clamp(c.g, -kColorLimit, kColorLimit, fallback.g);
    c.b = finite_clamp(c.b, -kColorLimit, kColorLimit, fallback.b);
    return c;
}

inline Params sanitized_params(const Params& in) {
    const Params defaults{};
    Params p = in;
    for (std::size_t i = 0; i < p.colors.size(); ++i) p.colors[i] = sanitize_color(p.colors[i], defaults.colors[i]);

    p.angle_deg = finite_clamp(p.angle_deg, -720.0f, 720.0f, defaults.angle_deg);
    p.cycles = finite_clamp(p.cycles, 0.1f, 20.0f, defaults.cycles);
    p.offset = finite_clamp(p.offset, -100.0f, 100.0f, defaults.offset);
    p.phase_deg = finite_clamp(p.phase_deg, -100000.0f, 100000.0f, defaults.phase_deg);
    p.saturation = finite_clamp(p.saturation, 0.0f, 2.0f, defaults.saturation);
    p.brightness = finite_clamp(p.brightness, 0.0f, 4.0f, defaults.brightness);

    p.depth_contrast = finite_clamp(p.depth_contrast, 0.05f, 8.0f, defaults.depth_contrast);
    p.bulge = finite_clamp(p.bulge, -2.0f, 2.0f, defaults.bulge);
    p.rounding = finite_clamp(p.rounding, 0.0f, 1.0f, defaults.rounding);

    p.turbulence_amount = finite_clamp(p.turbulence_amount, 0.0f, 2.0f, defaults.turbulence_amount);
    p.turbulence_size_x = finite_clamp(p.turbulence_size_x, 1.0f, 2000.0f, defaults.turbulence_size_x);
    p.turbulence_size_y = finite_clamp(p.turbulence_size_y, 1.0f, 2000.0f, defaults.turbulence_size_y);
    p.turbulence_evolution = finite_clamp(p.turbulence_evolution, -100000.0f, 100000.0f, defaults.turbulence_evolution);
    p.turbulence_softness = finite_clamp(p.turbulence_softness, 0.0f, 1.0f, defaults.turbulence_softness);

    p.glow_radius_px = finite_clamp(p.glow_radius_px, 0.0f, 2000.0f, defaults.glow_radius_px);
    p.glow_falloff = finite_clamp(p.glow_falloff, 0.25f, 4.0f, defaults.glow_falloff);
    p.glow_threshold = finite_clamp(p.glow_threshold, 0.0f, 1.0f, defaults.glow_threshold);
    p.glow_intensity = finite_clamp(p.glow_intensity, 0.0f, 10.0f, defaults.glow_intensity);
    p.glow_soft_clip = finite_clamp(p.glow_soft_clip, 0.0f, 1.0f, defaults.glow_soft_clip);

    p.grain_amount = finite_clamp(p.grain_amount, 0.0f, 0.5f, defaults.grain_amount);
    p.grain_size_px = finite_clamp(p.grain_size_px, 0.5f, 16.0f, defaults.grain_size_px);
    p.grain_color = finite_clamp(p.grain_color, 0.0f, 1.0f, defaults.grain_color);

    p.diffusion_blur_px = finite_clamp(p.diffusion_blur_px, 0.0f, 2000.0f, defaults.diffusion_blur_px);
    p.diffusion_center.x = finite_clamp(p.diffusion_center.x, 0.0f, 1.0f, defaults.diffusion_center.x);
    p.diffusion_center.y = finite_clamp(p.diffusion_center.y, 0.0f, 1.0f, defaults.diffusion_center.y);
    p.diffusion_focus_px = finite_clamp(p.diffusion_focus_px, 0.0f, 4000.0f, defaults.diffusion_focus_px);
    p.diffusion_feather_px = finite_clamp(p.diffusion_feather_px, 0.0f, 4000.0f, defaults.diffusion_feather_px);

    const auto q = static_cast<unsigned>(p.quality);
    if (q > static_cast<unsigned>(Quality::Final)) p.quality = defaults.quality;
    return p;
}

} // namespace stellar
