#include "cpu/ReferenceRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int fail(const char* msg, float max_err = 0.0f) {
    std::fprintf(stderr, "FAIL: %s (max_err=%g)\n", msg, max_err);
    return 1;
}
}

int main() {
    constexpr int W = 512, H = 320, S = W * 4;
    std::vector<float> src(static_cast<std::size_t>(W) * H * 4u, 0.0f);
    for (int y = 36; y < 284; ++y) {
        for (int x = 52; x < 460; ++x) {
            const float dx = (x - 256.0f) / 190.0f;
            const float dy = (y - 160.0f) / 112.0f;
            if (dx * dx + dy * dy < 1.0f) src[(static_cast<std::size_t>(y) * W + x) * 4u + 3u] = 1.0f;
        }
    }

    stellar::Params p;
    p.angle_deg = 31.0f;
    p.cycles = 2.7f;
    p.phase_deg = 83.0f;
    p.turbulence_amount_px = 0.18f;
    p.turbulence_size_x = 71.0f;
    p.turbulence_size_y = 93.0f;
    p.turbulence_evolution_deg = 47.0f;
    p.grain_amount = 0.025f;
    p.grain_size_px = 1.5f;
    p.grain_color = 0.35f;
    p.glow_radius_px = 20.0f;
    p.glow_falloff = 1.3f;
    p.glow_intensity = 0.7f;
    p.diffusion_blur_px = 16.0f;
    p.diffusion_focus_px = 70.0f;
    p.diffusion_feather_px = 80.0f;
    p.quality = stellar::Quality::Auto;

    std::vector<float> full(static_cast<std::size_t>(W) * H * 4u, 0.0f);
    const stellar::Bounds full_bounds{0, 0, W - 1, H - 1};
    stellar::ImageF32 full_img{W, H, S, src.data(), full.data(), full_bounds, 0, 0};
    stellar::render_reference(full_img, p, 0.0, 12);

    // Simulate a SmartFX tile with a 64px working halo aligned to 64px mip blocks.
    constexpr int out_l = 176, out_t = 112, out_r = 336, out_b = 208;
    constexpr int work_l = 64, work_t = 0, work_r = 448, work_b = 320;
    constexpr int WW = work_r - work_l, WH = work_b - work_t, WS = WW * 4;
    std::vector<float> work_src(static_cast<std::size_t>(WW) * WH * 4u, 0.0f);
    std::vector<float> work_dst(static_cast<std::size_t>(WW) * WH * 4u, 0.0f);
    for (int y = 0; y < WH; ++y) {
        const int gy = work_t + y;
        if (gy < 0 || gy >= H) continue;
        for (int x = 0; x < WW; ++x) {
            const int gx = work_l + x;
            if (gx < 0 || gx >= W) continue;
            const auto si = (static_cast<std::size_t>(gy) * W + gx) * 4u;
            const auto di = (static_cast<std::size_t>(y) * WW + x) * 4u;
            for (int c = 0; c < 4; ++c) work_src[di + c] = src[si + c];
        }
    }
    const stellar::Bounds translated_bounds{-work_l, -work_t, W - 1 - work_l, H - 1 - work_t};
    stellar::ImageF32 tile_img{WW, WH, WS, work_src.data(), work_dst.data(), translated_bounds, work_l, work_t};
    stellar::render_reference(tile_img, p, 0.0, 12);

    float max_err = 0.0f;
    for (int gy = out_t; gy < out_b; ++gy) {
        for (int gx = out_l; gx < out_r; ++gx) {
            const auto fi = (static_cast<std::size_t>(gy) * W + gx) * 4u;
            const auto ti = (static_cast<std::size_t>(gy - work_t) * WW + (gx - work_l)) * 4u;
            for (int c = 0; c < 4; ++c) max_err = std::max(max_err, std::abs(full[fi + c] - work_dst[ti + c]));
        }
    }
    if (max_err > 2.5e-3f) return fail("tile render diverges from full-frame render", max_err);

    std::printf("PASS: tile_consistency max_err=%g\n", max_err);
    return 0;
}
