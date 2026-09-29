#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace stellar::gpu {

struct Color3GPU {
    float r, g, b;
};

struct ParamsGPU {
    Color3GPU colors[5];
    float angle_rad, cycles, offset, phase;
    float saturation, brightness;
    float depth_contrast, bulge, rounding;
    float turbulence_amount, turbulence_scale_x, turbulence_scale_y, turbulence_evolution, turbulence_softness;
    float grain_amount, grain_size, grain_color;
    std::uint32_t grain_seed;
    float glow_radius, glow_falloff, glow_threshold, glow_intensity, glow_soft_clip;
    float diffusion_blur, center_x, center_y, focus, feather;
    std::int32_t diffusion_invert;
    std::int32_t glow_samples;
    std::int32_t max_mip_level;
    std::int32_t width, height, src_pitch, dst_pitch;
    std::int32_t out_width, out_height, crop_x, crop_y;
    std::int32_t src_width, src_height, src_offset_x, src_offset_y;
    std::int32_t origin_x, origin_y;
    std::int32_t min_x, min_y, max_x, max_y;

    // Frame-constant values precomputed on CPU. Keeping transcendental math
    // out of the per-pixel Metal path is both faster and improves CPU/GPU parity.
    float dir_x, dir_y, inv_bw, inv_bh;
    float bound_cx, bound_cy, phase_offset, depth_inv_diag;
    float depth_dir_x, depth_dir_y, rounding_clamped, turbulence_inv_x;
    float turbulence_inv_y, turbulence_evo_x, turbulence_evo_y, grain_inv_size;
    // During GPU dispatch, the original turbulence_evolution/turbulence_softness
    // slots plus turbulence_evo_x/y carry the two precomputed 4-D evolution pairs.
    float glow_lod, glow_spread, glow_threshold_inv, diffusion_lod;
    float diffusion_cx, diffusion_cy, diffusion_inv_feather;
    std::uint32_t depth_enabled;
};

static_assert(sizeof(Color3GPU) == 12, "GPU color layout changed");
static_assert(std::is_standard_layout<ParamsGPU>::value, "GPU params must stay standard-layout");
static_assert(sizeof(ParamsGPU) == 352, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, angle_rad) == 60, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, grain_seed) == 128, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, glow_radius) == 132, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, diffusion_blur) == 152, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, diffusion_invert) == 172, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, width) == 184, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, out_width) == 200, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, src_width) == 216, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, origin_x) == 232, "GPU params ABI changed");
static_assert(offsetof(ParamsGPU, min_x) == 240, "GPU params ABI changed");

} // namespace stellar::gpu
