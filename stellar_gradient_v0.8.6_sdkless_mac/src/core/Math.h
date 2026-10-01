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



// 4-D improved Perlin field recovered from the observable Cosmic CUDA kernel.
// The permutation is the classic 256-entry table; the gradient selector/sign
// convention below matches the supplied kernel's PTX, including its two
// independent XY displacement fields. Keeping this in the shared core lets CPU
// and Metal use the same numerical contract.
inline int perlin_perm(int i) {
    static constexpr std::uint8_t kPerm[256] = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
        140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
        247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
        57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
        74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
        60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
        65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
        200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
        52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
        207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
        119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
        129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
        218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
        81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
        184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
        222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    return static_cast<int>(kPerm[static_cast<unsigned>(i) & 255u]);
}

inline float perlin_fade(float t) { return t*t*t*(t*(t*6.0f-15.0f)+10.0f); }

inline float perlin_grad4(int hash, float x, float y, float z, float w) {
    const int h = hash & 31;
    const int group = h >> 3;
    float a = 0.0f, b = 0.0f, c = 0.0f;
    switch (group) {
        case 0: a=y; b=z; c=w; break;
        case 1: a=w; b=x; c=y; break;
        case 2: a=z; b=w; c=x; break;
        default: a=x; b=y; c=z; break;
    }
    if ((h & 4) == 0) a = -a;
    if ((h & 2) == 0) b = -b;
    if ((h & 1) == 0) c = -c;
    return a + b + c;
}

inline float perlin4(float x, float y, float z, float w) {
    const int xi0 = static_cast<int>(std::floor(x));
    const int yi0 = static_cast<int>(std::floor(y));
    const int zi0 = static_cast<int>(std::floor(z));
    const int wi0 = static_cast<int>(std::floor(w));
    const float xf=x-static_cast<float>(xi0), yf=y-static_cast<float>(yi0);
    const float zf=z-static_cast<float>(zi0), wf=w-static_cast<float>(wi0);
    const float u=perlin_fade(xf), v=perlin_fade(yf), s=perlin_fade(zf), t=perlin_fade(wf);
    // Cache the permutation hierarchy. The observable hash is unchanged, but
    // each 4-D sample now needs 30 table reads instead of recomputing 4 reads
    // independently for all 16 corners (64 reads). Gradient/interpolation order
    // remains exactly the same, so this is output-preserving.
    int px[2], pxy[2][2], pxyz[2][2][2];
    for (int dx=0; dx<2; ++dx) {
        px[dx] = perlin_perm(xi0 + dx);
        for (int dy=0; dy<2; ++dy) {
            pxy[dx][dy] = perlin_perm(px[dx] + yi0 + dy);
            for (int dz=0; dz<2; ++dz) pxyz[dx][dy][dz] = perlin_perm(pxy[dx][dy] + zi0 + dz);
        }
    }
    float g[2][2][2][2]{};
    for (int dx=0; dx<2; ++dx) for (int dy=0; dy<2; ++dy)
        for (int dz=0; dz<2; ++dz) for (int dw=0; dw<2; ++dw) {
            const int h = perlin_perm(pxyz[dx][dy][dz] + wi0 + dw);
            g[dx][dy][dz][dw] = perlin_grad4(h, xf-static_cast<float>(dx), yf-static_cast<float>(dy), zf-static_cast<float>(dz), wf-static_cast<float>(dw));
        }
    float yz[2][2][2]{};
    for(int dy=0;dy<2;++dy) for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw)
        yz[dy][dz][dw]=lerp(g[0][dy][dz][dw],g[1][dy][dz][dw],u);
    float zw[2][2]{};
    for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw)
        zw[dz][dw]=lerp(yz[0][dz][dw],yz[1][dz][dw],v);
    float ww[2]{};
    for(int dw=0;dw<2;++dw) ww[dw]=lerp(zw[0][dw],zw[1][dw],s);
    return lerp(ww[0],ww[1],t);
}

inline float cosmic_fbm4(float x, float y, float z, float w) {
    // Observable kernel contract: 3 octaves, exact frequency doubling,
    // persistence 0.5, normalized by accumulated amplitude.
    float sum=0.0f, amp=1.0f, norm=0.0f, freq=1.0f;
    for (int i=0;i<3;++i) {
        sum += perlin4(x*freq,y*freq,z*freq,w*freq)*amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
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
