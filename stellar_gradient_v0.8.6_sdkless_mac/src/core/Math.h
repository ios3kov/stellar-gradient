#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Params.h"

namespace stellar {

inline float clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float fract(float x) { return x - std::floor(x); }
inline float smooth01(float x) { x = clamp01(x); return x*x*(3.0f - 2.0f*x); }

inline std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}

inline float hash01(std::uint32_t x) {
    return static_cast<float>(hash32(x) & 0x00ffffffu) * (1.0f / 16777215.0f);
}

inline float value_noise(float x, float y, std::uint32_t seed) {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = smooth01(x - static_cast<float>(ix)), fy = smooth01(y - static_cast<float>(iy));
    auto h = [seed](int xx, int yy) {
        return hash01(static_cast<std::uint32_t>(xx) * 0x9e3779b9u ^ static_cast<std::uint32_t>(yy) * 0x85ebca6bu ^ seed);
    };
    const float a = h(ix, iy), b = h(ix+1, iy), c = h(ix, iy+1), d = h(ix+1, iy+1);
    return lerp(lerp(a,b,fx), lerp(c,d,fx), fy) * 2.0f - 1.0f;
}

inline float fbm(float x, float y, float softness, std::uint32_t seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    const int oct = 2 + static_cast<int>(clamp01(softness) * 3.0f);
    for (int i=0;i<oct;++i) {
        sum += value_noise(x, y, seed + static_cast<std::uint32_t>(i)*911u) * amp;
        norm += amp; x *= 2.03f; y *= 2.03f; amp *= 0.5f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

inline Color3f sample_palette(const std::array<Color3f,5>& c, float t) {
    t = fract(t);
    const float x = t * 5.0f;
    const int i0 = static_cast<int>(std::floor(x)) % 5;
    const int i1 = (i0 + 1) % 5;
    // Palette segments are linear in working-space RGB; easing shifts their colors.
    const float f = x - std::floor(x);
    return {lerp(c[i0].r,c[i1].r,f), lerp(c[i0].g,c[i1].g,f), lerp(c[i0].b,c[i1].b,f)};
}

inline Color3f adjust_sat_brightness(Color3f c, float sat, float bri) {
    const float y = c.r*0.2126f + c.g*0.7152f + c.b*0.0722f;
    c.r = (y + (c.r-y)*sat) * bri;
    c.g = (y + (c.g-y)*sat) * bri;
    c.b = (y + (c.b-y)*sat) * bri;
    return c;
}

} // namespace stellar
