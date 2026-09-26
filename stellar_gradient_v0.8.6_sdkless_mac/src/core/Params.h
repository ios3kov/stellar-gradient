#pragma once
#include <array>
#include <cstdint>

namespace stellar {

struct Color3f { float r, g, b; };
struct Point2f { float x, y; };

enum class Quality : std::uint8_t { Preview = 0, Auto = 1, Final = 2 };

struct Params {
    std::array<Color3f, 5> colors{{
        {0.12f, 0.05f, 0.34f},
        {0.12f, 0.38f, 0.95f},
        {0.67f, 0.17f, 0.95f},
        {1.00f, 0.31f, 0.55f},
        {1.00f, 0.75f, 0.18f},
    }};

    float angle_deg = 90.0f;
    float cycles = 1.0f;
    float offset = 0.0f;
    float phase_deg = 0.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;

    float depth_contrast = 1.0f;
    float bulge = 0.0f;
    float rounding = 0.0f;

    float turbulence_amount = 0.0f;
    float turbulence_size_x = 120.0f;
    float turbulence_size_y = 120.0f;
    float turbulence_evolution = 0.0f;
    float turbulence_softness = 0.5f;

    float glow_radius_px = 60.0f;
    float glow_falloff = 1.6f;
    float glow_threshold = 0.2f;
    float glow_intensity = 0.8f;
    float glow_soft_clip = 0.25f;

    float grain_amount = 0.03f;
    float grain_size_px = 1.0f;
    float grain_color = 0.0f; // 0=mono, 1=fully chromatic
    bool grain_animate = true;

    float diffusion_blur_px = 0.0f;
    Point2f diffusion_center{0.5f, 0.5f};
    float diffusion_focus_px = 120.0f;
    float diffusion_feather_px = 200.0f;
    bool diffusion_invert = false;

    Quality quality = Quality::Auto;
};

struct Bounds {
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
};

} // namespace stellar
