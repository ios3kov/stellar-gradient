#pragma once
#include <array>
#include <cstdint>

namespace stellar {

struct Color3f { float r, g, b; };
struct Point2f { float x, y; };

enum class Quality : std::uint8_t { Preview = 0, Auto = 1, Final = 2 };

struct Params {
    std::array<Color3f, 5> colors{{
        {40.0f/255.0f, 13.0f/255.0f, 140.0f/255.0f},
        {55.0f/255.0f, 17.0f/255.0f, 191.0f/255.0f},
        {24.0f/255.0f, 14.0f/255.0f, 89.0f/255.0f},
        {7.0f/255.0f, 12.0f/255.0f, 38.0f/255.0f},
        {242.0f/255.0f, 75.0f/255.0f, 75.0f/255.0f},
    }};

    float angle_deg = 90.0f;
    float cycles = 1.0f;
    float offset = 0.0f;
    float phase_deg = 0.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;

    float depth_angle_deg = 0.0f;
    float depth_contrast = 1.0f;
    float bulge = 0.6f;
    float rounding = 1.0f;
    float turbulence_amount_px = 40.0f;
    float turbulence_size_x = 3.0f;
    float turbulence_size_y = 3.0f;
    float turbulence_evolution_deg = 0.0f;
    float depth_softness_px = 40.0f;

    float glow_radius_px = 194.0f;
    float glow_falloff = 0.5f;
    float glow_threshold = 0.0f;
    float glow_intensity = 1.6f;
    float glow_soft_clip = 0.0f;

    float grain_amount = 0.2f;
    float grain_size_px = 1.0f;
    float grain_color = 1.0f;
    bool grain_animate = true;

    float diffusion_blur_px = 15.0f;
    Point2f diffusion_center{0.5f, 0.5f};
    float diffusion_focus_px = 50.0f;
    float diffusion_feather_px = 450.0f;
    bool diffusion_invert = false;

    Quality quality = Quality::Final;
};

struct Bounds {
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
};

} // namespace stellar
