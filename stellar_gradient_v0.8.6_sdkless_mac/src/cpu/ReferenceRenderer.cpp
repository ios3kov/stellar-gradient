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
    // Glow and diffusion never need their mip pyramids at the same time.
    // Reusing one pyramid cuts peak/retained scratch memory roughly in half.
    MipPyramidRGBA pyramid;
    std::vector<float> depth_a;
    std::vector<float> depth_b;
};

void box_blur_h(const std::vector<float>& src, std::vector<float>& dst, int width, int height, int radius) {
    const int diameter = radius * 2 + 1;
    const float inv = 1.0f / static_cast<float>(diameter);
    parallel_rows(0, height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) sum += src[row + static_cast<std::size_t>(std::clamp(k, 0, width - 1))];
            for (int x = 0; x < width; ++x) {
                dst[row + static_cast<std::size_t>(x)] = sum * inv;
                const int remove_x = std::clamp(x - radius, 0, width - 1);
                const int add_x = std::clamp(x + radius + 1, 0, width - 1);
                sum += src[row + static_cast<std::size_t>(add_x)] - src[row + static_cast<std::size_t>(remove_x)];
            }
        }
    });
}

void box_blur_v(const std::vector<float>& src, std::vector<float>& dst, int width, int height, int radius) {
    const int diameter = radius * 2 + 1;
    const float inv = 1.0f / static_cast<float>(diameter);
    parallel_rows(0, width, [&](int x0, int x1) {
        for (int x = x0; x < x1; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int yy = std::clamp(k, 0, height - 1);
                sum += src[static_cast<std::size_t>(yy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            }
            for (int y = 0; y < height; ++y) {
                const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
                dst[i] = sum * inv;
                const int remove_y = std::clamp(y - radius, 0, height - 1);
                const int add_y = std::clamp(y + radius + 1, 0, height - 1);
                sum += src[static_cast<std::size_t>(add_y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]
                     - src[static_cast<std::size_t>(remove_y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            }
        }
    });
}

thread_local CpuRenderWorkspace g_workspace;

struct WorkspaceTrimGuard {
    ~WorkspaceTrimGuard() {
        // Keep common 1080p/4K single-render scratch hot, but do not let an
        // 8K/MFR render leave hundreds of MB pinned on every render thread.
        constexpr std::size_t kBaseRetentionBudget = 192u * 1024u * 1024u;
        const unsigned active = std::max(1u, g_active_render_calls.load(std::memory_order_relaxed));
        const std::size_t per_render_budget = std::max<std::size_t>(32u * 1024u * 1024u, kBaseRetentionBudget / active);
        g_workspace.pyramid.trim_retained_bytes(per_render_budget);
    }
};

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
    const float rounding = clamp01(q.rounding);
    const float depth_a = q.depth_angle_deg * 3.14159265358979323846f / 180.0f;
    const float depth_dir_x = std::cos(depth_a);
    const float depth_dir_y = std::sin(depth_a);
    const float depth_inv_diag = 1.0f / std::max(1.0f, 0.5f * std::sqrt(bw * bw + bh * bh));
    const float turbulence_sx = std::max(1.0f, q.turbulence_size_x);
    const float turbulence_sy = std::max(1.0f, q.turbulence_size_y);
    const float turbulence_evo_x = q.turbulence_evolution * 0.013f;
    const float turbulence_evo_y = q.turbulence_evolution * 0.017f;
    const float grain_size = std::max(0.5f, q.grain_size_px);
    const float glow_threshold_inv = 1.0f / std::max(1e-5f, 1.0f - q.glow_threshold);
    const float glow_spread = std::max(0.35f, 0.45f * q.glow_falloff);

    const float* depth_map = nullptr;
    if (depth_active) {
        const std::size_t pixels = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);
        g_workspace.depth_a.resize(pixels);
        g_workspace.depth_b.resize(pixels);
        parallel_rows(0, img.height, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                const float* src = img.src_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
                for (int x = 0; x < img.width; ++x) {
                    const float alpha = clamp01(src[x * 4 + 3]);
                    const float dx = static_cast<float>(x) - cx;
                    const float dy = static_cast<float>(y) - cy;
                    const float ramp = clamp01(0.5f + 0.5f * (dx * depth_dir_x + dy * depth_dir_y) * depth_inv_diag);
                    float depth = alpha * ramp;
                    if (alpha > 0.0f && std::abs(q.depth_contrast - 1.0f) > 1.0e-3f) {
                        depth = alpha * clamp01((depth - 0.5f) * q.depth_contrast + 0.5f);
                    }
                    g_workspace.depth_a[static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x)] = depth;
                }
            }
        });

        // Exact Cosmic.aex rounding path:
        // R = min(width,height) * 0.125 * 0.7 * rounding.
        // If R >= 0.5, run 3 separable box-blur passes, each radius max(1,lround(R/3)).
        const float rounding_radius = static_cast<float>(std::min(img.width, img.height)) * 0.0875f * rounding;
        if (rounding_radius >= 0.5f) {
            const int pass_radius = std::max(1, static_cast<int>(std::lround(rounding_radius / 3.0f)));
            for (int pass = 0; pass < 3; ++pass) {
                box_blur_h(g_workspace.depth_a, g_workspace.depth_b, img.width, img.height, pass_radius);
                box_blur_v(g_workspace.depth_b, g_workspace.depth_a, img.width, img.height, pass_radius);
            }
        }
        depth_map = g_workspace.depth_a.data();
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

                if (plan.turbulence) {
                    const float layer_x = static_cast<float>(img.origin_x + x);
                    const float layer_y = static_cast<float>(img.origin_y + y);
                    u += fbm(layer_x / turbulence_sx + turbulence_evo_x,
                             layer_y / turbulence_sy + turbulence_evo_y,
                             q.turbulence_softness,
                             0x6d2b79f5u) * q.turbulence_amount;
                }

                if (depth_active) {
                    const std::size_t depth_i = static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x);
                    u += depth_map[depth_i] * q.bulge;
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
