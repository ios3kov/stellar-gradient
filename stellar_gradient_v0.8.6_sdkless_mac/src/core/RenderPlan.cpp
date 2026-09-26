#include "RenderPlan.h"
#include "MipPyramid.h"
#include <algorithm>
#include <cmath>

namespace stellar {

RenderPlan make_render_plan(const Params& p, int width, int height, Backend backend) {
    (void)backend; // CPU/GPU deliberately share the same quality plan for visual parity.
    RenderPlan r;
    r.turbulence = std::abs(p.turbulence_amount) > 1e-6f;
    r.grain = p.grain_amount > 1e-6f;
    r.glow = p.glow_intensity > 1e-6f && p.glow_radius_px > 0.5f;
    r.diffusion = p.diffusion_blur_px > 0.5f;

    const int max_dim = std::max(width, height);
    const int full_levels = max_dim > 0 ? (1 + static_cast<int>(std::floor(std::log2(static_cast<float>(max_dim))))) : 1;
    int quality_level_cap = full_levels;
    switch (p.quality) {
        case Quality::Preview:
            r.glow_samples = 1;
            quality_level_cap = std::min(full_levels, 8);
            break;
        case Quality::Auto:
            r.glow_samples = 3;
            quality_level_cap = std::min(full_levels, 10);
            break;
        case Quality::Final:
            r.glow_samples = 5;
            quality_level_cap = full_levels;
            break;
    }

    // Build only mip levels that can actually be sampled. This is important on
    // 4K frames where blindly constructing the entire chain wastes bandwidth
    // and transient memory for small/medium blur radii.
    float max_requested_lod = 0.0f;
    const float raw_glow_lod = radius_to_lod(std::max(1.0f, p.glow_radius_px));
    if (r.glow) {
        const float spread = r.glow_samples > 1 ? std::max(0.35f, 0.45f * p.glow_falloff) : 0.0f;
        max_requested_lod = std::max(max_requested_lod, raw_glow_lod + spread);
    }
    const float raw_diffusion_lod = radius_to_lod(std::max(1.0f, p.diffusion_blur_px));
    if (r.diffusion) max_requested_lod = std::max(max_requested_lod, raw_diffusion_lod);

    const int required_levels = (r.glow || r.diffusion)
        ? std::max(1, static_cast<int>(std::ceil(max_requested_lod)) + 1)
        : 1;
    r.max_mip_levels = std::max(1, std::min(quality_level_cap, required_levels));
    r.glow_lod = std::clamp(raw_glow_lod, 0.0f, static_cast<float>(std::max(0, r.max_mip_levels - 1)));
    r.diffusion_max_lod = std::clamp(raw_diffusion_lod, 0.0f, static_cast<float>(std::max(0, r.max_mip_levels - 1)));
    return r;
}

} // namespace stellar
