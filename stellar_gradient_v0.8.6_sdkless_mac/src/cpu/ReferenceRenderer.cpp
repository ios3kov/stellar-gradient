#include "ReferenceRenderer.h"
#include "../core/Math.h"
#include "../core/MipPyramid.h"
#include "../core/RenderPlan.h"
#include "../core/Sanitize.h"
#include "../core/Parallel.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace stellar {

namespace {

Bounds resolve_bounds(const ImageF32& img) {
    Bounds b = img.content_bounds;
    if (b.max_x < b.min_x || b.max_y < b.min_y) {
        b = {0, 0, img.width - 1, img.height - 1};
    }
    // Do not clamp explicit bounds to the current working tile. SmartFX can
    // render only a small region while the gradient/depth model is defined in
    // full layer space; clamping here would create visible tile seams.
    return b;
}

inline float soft_clip(float v, float soft) {
    return soft > 0.0f ? v / (1.0f + soft * std::max(0.0f, v - 1.0f)) : v;
}

struct CpuRenderWorkspace {
    MipPyramidRGBA pyramid;
    std::vector<float> depth_alpha;
};

thread_local CpuRenderWorkspace g_workspace;

struct WorkspaceTrimGuard {
    ~WorkspaceTrimGuard() {
        // Keep common 1080p/4K single-render scratch hot, but do not let an
        // 8K/MFR render leave hundreds of MB pinned on every render thread.
        constexpr std::size_t kBaseRetentionBudget = 192u * 1024u * 1024u;
        const unsigned active = std::max(1u, g_active_render_calls.load(std::memory_order_relaxed));
        const std::size_t per_render_budget = std::max<std::size_t>(32u * 1024u * 1024u, kBaseRetentionBudget / active);
        g_workspace.pyramid.trim_retained_bytes(per_render_budget);
        if (g_workspace.depth_alpha.capacity() * sizeof(float) > per_render_budget / 2u) {
            std::vector<float>().swap(g_workspace.depth_alpha);
        }
    }
};

inline float sample_alpha_bilinear(const std::vector<float>& alpha, int w, int h, float x, float y) {
    if (w <= 0 || h <= 0 || alpha.empty()) return 0.0f;
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    auto at = [&](int xx, int yy) {
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) return 0.0f;
        return alpha[static_cast<std::size_t>(yy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(xx)];
    };
    return lerp(lerp(at(x0,y0),at(x0+1,y0),fx), lerp(at(x0,y0+1),at(x0+1,y0+1),fx), fy);
}

} // namespace

void render_reference(const ImageF32& img, const Params& p, double time_seconds, std::uint32_t frame_index) {
    RenderConcurrencyScope concurrency_scope;
    WorkspaceTrimGuard workspace_trim_guard;
    (void)time_seconds;
    if (!img.src_rgba || !img.dst_rgba || img.width <= 0 || img.height <= 0 || img.stride_floats < img.width * 4) return;

    const Params q = sanitized_params(p);
    const RenderPlan plan = make_render_plan(q, img.width, img.height, Backend::CPU);
    const Bounds b = resolve_bounds(img);
    const float bw = static_cast<float>(std::max(1, b.max_x - b.min_x + 1));
    const float bh = static_cast<float>(std::max(1, b.max_y - b.min_y + 1));
    const float cx = 0.5f * static_cast<float>(b.min_x + b.max_x);
    const float cy = 0.5f * static_cast<float>(b.min_y + b.max_y);
    const float a = q.angle_deg * 3.14159265358979323846f / 180.0f;
    const float dir_x = std::cos(a);
    const float dir_y = std::sin(a);
    const std::uint32_t seed = q.grain_animate ? frame_index * 1664525u + 1013904223u : 0x12345678u;
    const bool depth_active = std::abs(q.bulge) > 1.0e-6f;
    const bool depth_turbulence = depth_active && q.turbulence_amount_px > 1.0e-6f;
    const float rounding = clamp01(q.rounding);
    const float depth_angle = q.depth_angle_deg * 3.14159265358979323846f / 180.0f;
    const float depth_dir_x = std::cos(depth_angle);
    const float depth_dir_y = std::sin(depth_angle);
    const float depth_sample_radius = std::max(0.75f, 0.5f * q.depth_softness_px + 0.75f);
    const float turbulence_sx = std::max(0.1f, q.turbulence_size_x) * 64.0f;
    const float turbulence_sy = std::max(0.1f, q.turbulence_size_y) * 64.0f;
    const float turbulence_evo_x = q.turbulence_evolution_deg * 0.013f;
    const float turbulence_evo_y = q.turbulence_evolution_deg * 0.017f;
    const float grain_size = std::max(0.5f, q.grain_size_px);
    const float glow_threshold_inv = 1.0f / std::max(1e-5f, 1.0f - q.glow_threshold);
    const float glow_spread = std::max(0.35f, 0.45f * q.glow_falloff);

    if (depth_active) {
        g_workspace.depth_alpha.resize(static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height));
        parallel_rows(0, img.height, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                const float* src = img.src_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
                float* arow = g_workspace.depth_alpha.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width);
                for (int x = 0; x < img.width; ++x) arow[x] = clamp01(src[x * 4 + 3]);
            }
        });
    } else {
        g_workspace.depth_alpha.clear();
    }

    float* glow_source = nullptr;
    if (plan.glow) {
        glow_source = g_workspace.pyramid.prepare_level0(img.width, img.height);
        if (!glow_source) return;
    } else {
        g_workspace.pyramid.clear();
    }

    // Fused base shading. The destination doubles as the working base image,
    // eliminating an entire full-resolution scratch buffer.
    parallel_rows(0, img.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* src = img.src_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
            float* dst = img.dst_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
            for (int x = 0; x < img.width; ++x) {
                const float alpha = clamp01(src[x * 4 + 3]);
                const float nx = (static_cast<float>(x) - cx) / bw;
                const float ny = (static_cast<float>(y) - cy) / bh;
                float u = (nx * dir_x + ny * dir_y) * q.cycles + q.offset + q.phase_deg / 360.0f;

                if (depth_active) {
                    const float layer_x = static_cast<float>(img.origin_x + x);
                    const float layer_y = static_cast<float>(img.origin_y + y);
                    float sample_x = static_cast<float>(x);
                    float sample_y = static_cast<float>(y);
                    if (depth_turbulence) {
                        const float jx = fbm(layer_x / turbulence_sx + turbulence_evo_x,
                                             layer_y / turbulence_sy + turbulence_evo_y,
                                             0.5f, 0x6d2b79f5u);
                        const float jy = fbm(layer_x / turbulence_sx - turbulence_evo_y,
                                             layer_y / turbulence_sy + turbulence_evo_x,
                                             0.5f, 0x9e3779b9u);
                        sample_x += jx * q.turbulence_amount_px;
                        sample_y += jy * q.turbulence_amount_px;
                    }
                    const float a_minus = sample_alpha_bilinear(g_workspace.depth_alpha, img.width, img.height,
                        sample_x - depth_dir_x * depth_sample_radius, sample_y - depth_dir_y * depth_sample_radius);
                    const float a_plus = sample_alpha_bilinear(g_workspace.depth_alpha, img.width, img.height,
                        sample_x + depth_dir_x * depth_sample_radius, sample_y + depth_dir_y * depth_sample_radius);
                    const float signed_edge = 0.5f * (a_minus - a_plus);
                    const float magnitude = clamp01(std::abs(signed_edge) * std::max(0.0f, q.depth_contrast) * 2.0f);
                    const float shaped = lerp(magnitude, smooth01(magnitude), rounding);
                    u += std::copysign(shaped, signed_edge) * q.bulge;
                }

                Color3f c = adjust_sat_brightness(sample_palette(q.colors, u), q.saturation, q.brightness);
                if (plan.grain) {
                    const int gx = static_cast<int>(std::floor(static_cast<float>(img.origin_x + x) / grain_size));
                    const int gy = static_cast<int>(std::floor(static_cast<float>(img.origin_y + y) / grain_size));
                    const std::uint32_t h = hash32(seed ^ static_cast<std::uint32_t>(gx) * 73856093u ^ static_cast<std::uint32_t>(gy) * 19349663u);
                    const float mono = (hash01(h) - 0.5f) * 2.0f * q.grain_amount;
                    const float rr = (hash01(h ^ 0x68bc21ebu) - 0.5f) * 2.0f * q.grain_amount;
                    const float gg = (hash01(h ^ 0x02e5be93u) - 0.5f) * 2.0f * q.grain_amount;
                    const float bb = (hash01(h ^ 0x967a889bu) - 0.5f) * 2.0f * q.grain_amount;
                    c.r += lerp(mono, rr, q.grain_color);
                    c.g += lerp(mono, gg, q.grain_color);
                    c.b += lerp(mono, bb, q.grain_color);
                }

                const float r = c.r * alpha;
                const float g = c.g * alpha;
                const float bl = c.b * alpha;
                dst[x * 4 + 0] = r;
                dst[x * 4 + 1] = g;
                dst[x * 4 + 2] = bl;
                dst[x * 4 + 3] = alpha;

                if (plan.glow) {
                    const float lum = r * 0.2126f + g * 0.7152f + bl * 0.0722f;
                    const float k = clamp01((lum - q.glow_threshold) * glow_threshold_inv);
                    const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x)) * 4u;
                    glow_source[i + 0] = r * k;
                    glow_source[i + 1] = g * k;
                    glow_source[i + 2] = bl * k;
                    glow_source[i + 3] = alpha * k;
                }
            }
        }
    });

    // Glow owns level 0 instead of copying it again. The same pyramid storage is
    // rebuilt for diffusion after glow composition, so the two effects never
    // retain two full mip chains at once.
    if (plan.glow) {
        g_workspace.pyramid.generate_mips(plan.max_mip_levels);
        const MipPyramidRGBA& glow_pyramid = g_workspace.pyramid;
        parallel_rows(0, img.height, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                float* dst = img.dst_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
                for (int x = 0; x < img.width; ++x) {
                    float glow[4] = {0, 0, 0, 0};
                    const float spread = glow_spread;
                    const int n = plan.glow_samples;
                    float wsum = 0.0f;
                    for (int s = 0; s < n; ++s) {
                        const float centered = n == 1 ? 0.0f : (static_cast<float>(s) / static_cast<float>(n - 1) - 0.5f) * 2.0f;
                        float sample[4];
                        glow_pyramid.sample_lod(static_cast<float>(x), static_cast<float>(y), plan.glow_lod + centered * spread, sample);
                        const float w = 1.0f - 0.22f * std::abs(centered);
                        for (int c = 0; c < 4; ++c) glow[c] += sample[c] * w;
                        wsum += w;
                    }
                    const float inv = 1.0f / std::max(1.0e-6f, wsum);
                    for (float& v : glow) v *= inv;
                    dst[x * 4 + 0] = soft_clip(dst[x * 4 + 0] + glow[0] * q.glow_intensity, q.glow_soft_clip);
                    dst[x * 4 + 1] = soft_clip(dst[x * 4 + 1] + glow[1] * q.glow_intensity, q.glow_soft_clip);
                    dst[x * 4 + 2] = soft_clip(dst[x * 4 + 2] + glow[2] * q.glow_intensity, q.glow_soft_clip);
                    dst[x * 4 + 3] = clamp01(dst[x * 4 + 3] + glow[3] * q.glow_intensity);
                }
            }
        });
    }

    if (!plan.diffusion) return;

    g_workspace.pyramid.build_copy(img.dst_rgba, img.width, img.height, img.stride_floats, plan.max_mip_levels);
    const MipPyramidRGBA& diffusion_pyramid = g_workspace.pyramid;
    const float dcx = static_cast<float>(b.min_x) + q.diffusion_center.x * static_cast<float>(std::max(0, b.max_x - b.min_x));
    const float dcy = static_cast<float>(b.min_y) + q.diffusion_center.y * static_cast<float>(std::max(0, b.max_y - b.min_y));

    parallel_rows(0, img.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* dst = img.dst_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
            for (int x = 0; x < img.width; ++x) {
                const float dxp = static_cast<float>(x) - dcx;
                const float dyp = static_cast<float>(y) - dcy;
                const float dist = std::sqrt(dxp * dxp + dyp * dyp);
                float amount = smooth01((dist - q.diffusion_focus_px) / std::max(1.0f, q.diffusion_feather_px));
                if (q.diffusion_invert) amount = 1.0f - amount;
                const float lod = amount * plan.diffusion_max_lod;
                float blurred[4];
                diffusion_pyramid.sample_lod(static_cast<float>(x), static_cast<float>(y), lod, blurred);
                for (int c = 0; c < 4; ++c) dst[x * 4 + c] = lerp(dst[x * 4 + c], blurred[c], amount);
            }
        }
    });
}

} // namespace stellar
