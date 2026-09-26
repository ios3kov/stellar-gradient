#include "cpu/ReferenceRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct Signature {
    std::array<double, 4> mean{};
    std::array<float, 4> maxv{};
    double energy = 0.0;
};

Signature signature(const std::vector<float>& rgba) {
    Signature s;
    const std::size_t pixels = rgba.size() / 4u;
    for (std::size_t i = 0; i < pixels; ++i) {
        for (int c = 0; c < 4; ++c) {
            const float v = rgba[i * 4u + static_cast<std::size_t>(c)];
            s.mean[static_cast<std::size_t>(c)] += static_cast<double>(v);
            s.maxv[static_cast<std::size_t>(c)] = std::max(s.maxv[static_cast<std::size_t>(c)], v);
            s.energy += static_cast<double>(v) * static_cast<double>(v);
        }
    }
    const double inv = pixels ? 1.0 / static_cast<double>(pixels) : 0.0;
    for (double& v : s.mean) v *= inv;
    s.energy *= inv;
    return s;
}

bool close(double a, double b, double eps) { return std::abs(a - b) <= eps; }

int check_signature(const char* label, const Signature& got, const Signature& expected) {
    // These are deliberately tight enough to catch visual/math regressions but
    // loose enough for tiny libm differences between x86_64 and arm64.
    constexpr double mean_eps = 2.0e-6;
    constexpr double max_eps = 5.0e-6;
    constexpr double energy_eps = 8.0e-6;
    for (int c = 0; c < 4; ++c) {
        if (!close(got.mean[static_cast<std::size_t>(c)], expected.mean[static_cast<std::size_t>(c)], mean_eps)) {
            std::fprintf(stderr, "FAIL: %s mean[%d] got=%.10g expected=%.10g\n", label, c,
                         got.mean[static_cast<std::size_t>(c)], expected.mean[static_cast<std::size_t>(c)]);
            return 1;
        }
        if (!close(got.maxv[static_cast<std::size_t>(c)], expected.maxv[static_cast<std::size_t>(c)], max_eps)) {
            std::fprintf(stderr, "FAIL: %s max[%d] got=%.10g expected=%.10g\n", label, c,
                         static_cast<double>(got.maxv[static_cast<std::size_t>(c)]),
                         static_cast<double>(expected.maxv[static_cast<std::size_t>(c)]));
            return 1;
        }
    }
    if (!close(got.energy, expected.energy, energy_eps)) {
        std::fprintf(stderr, "FAIL: %s energy got=%.10g expected=%.10g\n", label, got.energy, expected.energy);
        return 1;
    }
    return 0;
}

std::vector<float> make_source(int w, int h) {
    std::vector<float> src(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u, 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
            const float ny = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
            const float dx = (nx - 0.5f) / 0.39f;
            const float dy = (ny - 0.5f) / 0.31f;
            const float d = dx * dx + dy * dy;
            float a = std::clamp((1.08f - d) * 8.0f, 0.0f, 1.0f);
            if (((x / 13) + (y / 17)) % 5 == 0) a *= 0.7f;
            src[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4u + 3u] = a;
        }
    }
    return src;
}

Signature render_case(stellar::Params p, std::uint32_t frame) {
    constexpr int W = 173, H = 127, S = W * 4;
    auto src = make_source(W, H);
    std::vector<float> dst(src.size(), 0.0f);
    const stellar::Bounds bounds{0, 0, W - 1, H - 1};
    stellar::ImageF32 img{W, H, S, src.data(), dst.data(), bounds, 0, 0};
    p.quality = stellar::Quality::Final;
    stellar::render_reference(img, p, static_cast<double>(frame) / 30.0, frame);
    return signature(dst);
}

} // namespace

int main() {
    stellar::Params base;
    base.glow_intensity = 0.0f;
    base.grain_amount = 0.0f;
    base.diffusion_blur_px = 0.0f;
    base.turbulence_amount = 0.0f;
    base.bulge = 0.0f;
    base.angle_deg = 37.0f;
    base.cycles = 2.35f;
    base.phase_deg = 123.0f;
    base.saturation = 1.17f;
    base.brightness = 0.91f;

    stellar::Params heavy = base;
    heavy.bulge = 0.42f;
    heavy.rounding = 0.65f;
    heavy.depth_contrast = 1.7f;
    heavy.turbulence_amount = 0.21f;
    heavy.turbulence_size_x = 68.0f;
    heavy.turbulence_size_y = 104.0f;
    heavy.turbulence_evolution = 57.0f;
    heavy.turbulence_softness = 0.72f;
    heavy.grain_amount = 0.031f;
    heavy.grain_size_px = 1.4f;
    heavy.grain_color = 0.33f;
    heavy.glow_radius_px = 31.0f;
    heavy.glow_falloff = 1.55f;
    heavy.glow_threshold = 0.16f;
    heavy.glow_intensity = 0.86f;
    heavy.glow_soft_clip = 0.28f;

    stellar::Params diffusion = heavy;
    diffusion.diffusion_blur_px = 25.0f;
    diffusion.diffusion_center = {0.37f, 0.62f};
    diffusion.diffusion_focus_px = 21.0f;
    diffusion.diffusion_feather_px = 56.0f;

    // Baselines are generated from the audited CPU F32 reference renderer.
    // Update only when an intentional visual change has been reviewed.
    const Signature expected_base{{0.204531780142,0.111062071743,0.183825664775,0.363154532137},{0.991368830204f,0.680644214153f,0.960009276867f,1.0f},0.650493346755};
    const Signature expected_heavy{{0.278179450769,0.151995469293,0.200844051448,0.396885251969},{1.13813924789f,0.833556175232f,1.05600976944f,1.0f},0.779296694444};
    const Signature expected_diffusion{{0.29132207647,0.159103445168,0.208560703857,0.414318991492},{1.13684248924f,0.830838620663f,1.05317437649f,1.0f},0.720076978888};

    const Signature got_base = render_case(base, 7);
    const Signature got_heavy = render_case(heavy, 19);
    const Signature got_diffusion = render_case(diffusion, 19);

    // Bootstrap mode: zeros mean print signatures so the source can be pinned.
    if (expected_base.energy == 0.0) {
        auto dump=[](const char* label,const Signature& s){
            std::printf("%s mean={%.12g,%.12g,%.12g,%.12g} max={%.12g,%.12g,%.12g,%.12g} energy=%.12g\n",
                        label,s.mean[0],s.mean[1],s.mean[2],s.mean[3],
                        static_cast<double>(s.maxv[0]),static_cast<double>(s.maxv[1]),
                        static_cast<double>(s.maxv[2]),static_cast<double>(s.maxv[3]),s.energy);
        };
        dump("base",got_base); dump("heavy",got_heavy); dump("diffusion",got_diffusion);
        return 2;
    }

    if (check_signature("base", got_base, expected_base)) return 1;
    if (check_signature("heavy", got_heavy, expected_heavy)) return 1;
    if (check_signature("diffusion", got_diffusion, expected_diffusion)) return 1;

    std::puts("PASS: quality_reference (CPU F32 golden signatures locked)");
    return 0;
}
