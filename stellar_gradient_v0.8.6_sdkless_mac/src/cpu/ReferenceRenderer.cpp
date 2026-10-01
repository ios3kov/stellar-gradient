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

void box_blur_h(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
    if (radius <= 0) { dst = src; return; }
    const int span = radius * 2 + 1;
    dst.resize(src.size());
    for (int y = 0; y < h; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float sum = 0.0f;
        for (int k = -radius; k <= radius; ++k) sum += src[row + static_cast<std::size_t>(std::clamp(k, 0, w - 1))];
        for (int x = 0; x < w; ++x) {
            dst[row + static_cast<std::size_t>(x)] = sum / static_cast<float>(span);
            const int remove_x = std::clamp(x - radius, 0, w - 1);
            const int add_x = std::clamp(x + radius + 1, 0, w - 1);
            sum += src[row + static_cast<std::size_t>(add_x)] - src[row + static_cast<std::size_t>(remove_x)];
        }
    }
}

void box_blur_v(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
    if (radius <= 0) { dst = src; return; }
    const int span = radius * 2 + 1;
    dst.resize(src.size());
    for (int x = 0; x < w; ++x) {
        float sum = 0.0f;
        for (int k = -radius; k <= radius; ++k) {
            const int yy = std::clamp(k, 0, h - 1);
            sum += src[static_cast<std::size_t>(yy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        }
        for (int y = 0; y < h; ++y) {
            dst[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = sum / static_cast<float>(span);
            const int remove_y = std::clamp(y - radius, 0, h - 1);
            const int add_y = std::clamp(y + radius + 1, 0, h - 1);
            sum += src[static_cast<std::size_t>(add_y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)]
                 - src[static_cast<std::size_t>(remove_y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        }
    }
}


void box_blur_rgb_h(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
    if (radius <= 0) { dst = src; return; }
    const int span = radius * 2 + 1;
    dst.resize(src.size());
    // Each scanline is independent. Parallelizing scanlines preserves the exact
    // accumulation order inside a row, so the output remains bit-identical while
    // avoiding a single-thread bottleneck for Cosmic-style Softness.
    parallel_rows(0, h, [&](int y0, int y1) {
        for (int y=y0; y<y1; ++y) {
            const std::size_t row=static_cast<std::size_t>(y)*static_cast<std::size_t>(w)*3u;
            float sum[3]={0,0,0};
            for(int k=-radius;k<=radius;++k){
                const int xx=std::clamp(k,0,w-1); const std::size_t i=row+static_cast<std::size_t>(xx)*3u;
                for(int c=0;c<3;++c) sum[c]+=src[i+static_cast<std::size_t>(c)];
            }
            for(int x=0;x<w;++x){
                const std::size_t i=row+static_cast<std::size_t>(x)*3u;
                for(int c=0;c<3;++c) dst[i+static_cast<std::size_t>(c)]=sum[c]/static_cast<float>(span);
                const int rx=std::clamp(x-radius,0,w-1), ax=std::clamp(x+radius+1,0,w-1);
                const std::size_t ri=row+static_cast<std::size_t>(rx)*3u, ai=row+static_cast<std::size_t>(ax)*3u;
                for(int c=0;c<3;++c) sum[c]+=src[ai+static_cast<std::size_t>(c)]-src[ri+static_cast<std::size_t>(c)];
            }
        }
    }, 48);
}

void box_blur_rgb_v(const std::vector<float>& src, std::vector<float>& dst, int w, int h, int radius) {
    if (radius <= 0) { dst = src; return; }
    const int span = radius * 2 + 1;
    dst.resize(src.size());
    // Columns are independent too. `parallel_rows` is a generic range splitter;
    // here its range is X. Per-column floating-point accumulation order is unchanged.
    parallel_rows(0, w, [&](int x0, int x1) {
        for(int x=x0;x<x1;++x){
            float sum[3]={0,0,0};
            for(int k=-radius;k<=radius;++k){
                const int yy=std::clamp(k,0,h-1); const std::size_t i=(static_cast<std::size_t>(yy)*w+static_cast<std::size_t>(x))*3u;
                for(int c=0;c<3;++c) sum[c]+=src[i+static_cast<std::size_t>(c)];
            }
            for(int y=0;y<h;++y){
                const std::size_t i=(static_cast<std::size_t>(y)*w+static_cast<std::size_t>(x))*3u;
                for(int c=0;c<3;++c) dst[i+static_cast<std::size_t>(c)]=sum[c]/static_cast<float>(span);
                const int ry=std::clamp(y-radius,0,h-1), ay=std::clamp(y+radius+1,0,h-1);
                const std::size_t ri=(static_cast<std::size_t>(ry)*w+static_cast<std::size_t>(x))*3u;
                const std::size_t ai=(static_cast<std::size_t>(ay)*w+static_cast<std::size_t>(x))*3u;
                for(int c=0;c<3;++c) sum[c]+=src[ai+static_cast<std::size_t>(c)]-src[ri+static_cast<std::size_t>(c)];
            }
        }
    }, 48);
}


// CPU optimization for the recovered 4-D turbulence. z/w are frame-constant
// for each octave, and x/y advance over a regular layer-space grid. Cache the
// nested permutation hashes for the tiny 2-D lattice touched by each octave;
// per-pixel math keeps the same gradient and interpolation order as perlin4().
struct Perlin4SliceCache {
    struct Node { std::uint8_t hash[4]{}; }; // dz*2 + dw
    struct AxisState {
        int lattice=0;
        float frac=0.0f;
        float fade=0.0f;
    };

    int x0=0,y0=0,w=0,h=0,zi=0,wi=0;
    float zf=0.0f,wf=0.0f,fade_z=0.0f,fade_w=0.0f;
    std::vector<Node> nodes;

    static AxisState axis_state(float v) {
        const int lattice=static_cast<int>(std::floor(v));
        const float frac=v-static_cast<float>(lattice);
        return {lattice,frac,perlin_fade(frac)};
    }

    void build(float xmin,float xmax,float ymin,float ymax,float z,float ww) {
        if (xmin>xmax) std::swap(xmin,xmax);
        if (ymin>ymax) std::swap(ymin,ymax);
        x0=static_cast<int>(std::floor(xmin)); y0=static_cast<int>(std::floor(ymin));
        const int x1=static_cast<int>(std::floor(xmax))+1;
        const int y1=static_cast<int>(std::floor(ymax))+1;
        w=x1-x0+1; h=y1-y0+1;
        zi=static_cast<int>(std::floor(z)); wi=static_cast<int>(std::floor(ww));
        zf=z-static_cast<float>(zi); wf=ww-static_cast<float>(wi);
        fade_z=perlin_fade(zf); fade_w=perlin_fade(wf);
        nodes.resize(static_cast<std::size_t>(w)*static_cast<std::size_t>(h));
        for(int yy=0;yy<h;++yy) for(int xx=0;xx<w;++xx){
            Node& n=nodes[static_cast<std::size_t>(yy)*static_cast<std::size_t>(w)+static_cast<std::size_t>(xx)];
            for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw){
                int hash=perlin_perm(x0+xx);
                hash=perlin_perm(hash+y0+yy);
                hash=perlin_perm(hash+zi+dz);
                hash=perlin_perm(hash+wi+dw);
                n.hash[dz*2+dw]=static_cast<std::uint8_t>(hash);
            }
        }
    }

    float sample(const AxisState& xs,const AxisState& ys) const {
        const int xi=xs.lattice, yi=ys.lattice;
        const float xf=xs.frac, yf=ys.frac;
        const float u=xs.fade, v=ys.fade;
        float g[2][2][2][2]{};
        for(int dx=0;dx<2;++dx) for(int dy=0;dy<2;++dy){
            const int nx=xi+dx-x0, ny=yi+dy-y0;
            const Node& n=nodes[static_cast<std::size_t>(ny)*static_cast<std::size_t>(w)+static_cast<std::size_t>(nx)];
            for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw)
                g[dx][dy][dz][dw]=perlin_grad4(static_cast<int>(n.hash[dz*2+dw]),
                    xf-static_cast<float>(dx),yf-static_cast<float>(dy),zf-static_cast<float>(dz),wf-static_cast<float>(dw));
        }
        float yz[2][2][2]{};
        for(int dy=0;dy<2;++dy) for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw)
            yz[dy][dz][dw]=lerp(g[0][dy][dz][dw],g[1][dy][dz][dw],u);
        float zw[2][2]{};
        for(int dz=0;dz<2;++dz) for(int dw=0;dw<2;++dw) zw[dz][dw]=lerp(yz[0][dz][dw],yz[1][dz][dw],v);
        const float a=lerp(zw[0][0],zw[1][0],fade_z), b=lerp(zw[0][1],zw[1][1],fade_z);
        return lerp(a,b,fade_w);
    }

    float sample(float x,float y) const {
        return sample(axis_state(x),axis_state(y));
    }
};

struct CosmicTurbulenceCache {
    using AxisState=Perlin4SliceCache::AxisState;

    Perlin4SliceCache x[3],y[3];
    std::vector<AxisState> x_cols[3],x_rows[3],y_cols[3],y_rows[3];

    void build(int origin_x,int origin_y,int render_width,int render_height,float inv_x,float inv_y,
               float zx,float wx,float zy,float wy){
        const float x0=static_cast<float>(origin_x)*inv_x;
        const float x1=static_cast<float>(origin_x+render_width-1)*inv_x;
        const float y0=static_cast<float>(origin_y)*inv_y;
        const float y1=static_cast<float>(origin_y+render_height-1)*inv_y;
        float freq=1.0f;
        for(int o=0;o<3;++o){
            x[o].build(x0*freq,x1*freq,y0*freq,y1*freq,zx*freq,wx*freq);
            y[o].build((x0+137.5f)*freq,(x1+137.5f)*freq,(y0+91.3f)*freq,(y1+91.3f)*freq,zy*freq,wy*freq);

            x_cols[o].resize(static_cast<std::size_t>(render_width));
            y_cols[o].resize(static_cast<std::size_t>(render_width));
            for(int px=0;px<render_width;++px){
                const float base=static_cast<float>(origin_x+px)*inv_x;
                x_cols[o][static_cast<std::size_t>(px)]=Perlin4SliceCache::axis_state(base*freq);
                y_cols[o][static_cast<std::size_t>(px)]=Perlin4SliceCache::axis_state((base+137.5f)*freq);
            }

            x_rows[o].resize(static_cast<std::size_t>(render_height));
            y_rows[o].resize(static_cast<std::size_t>(render_height));
            for(int py=0;py<render_height;++py){
                const float base=static_cast<float>(origin_y+py)*inv_y;
                x_rows[o][static_cast<std::size_t>(py)]=Perlin4SliceCache::axis_state(base*freq);
                y_rows[o][static_cast<std::size_t>(py)]=Perlin4SliceCache::axis_state((base+91.3f)*freq);
            }
            freq*=2.0f;
        }
    }

    float sample_x(int px,int py) const {
        const std::size_t sx=static_cast<std::size_t>(px), sy=static_cast<std::size_t>(py);
        return (x[0].sample(x_cols[0][sx],x_rows[0][sy])
              +0.5f*x[1].sample(x_cols[1][sx],x_rows[1][sy])
              +0.25f*x[2].sample(x_cols[2][sx],x_rows[2][sy]))/1.75f;
    }

    float sample_y(int px,int py) const {
        const std::size_t sx=static_cast<std::size_t>(px), sy=static_cast<std::size_t>(py);
        return (y[0].sample(y_cols[0][sx],y_rows[0][sy])
              +0.5f*y[1].sample(y_cols[1][sx],y_rows[1][sy])
              +0.25f*y[2].sample(y_cols[2][sx],y_rows[2][sy]))/1.75f;
    }
};

// Final-stage, channel-weighted grain. Existing independent noise/seed policy is
// retained; this is not pixel-identical to Cosmic's random pattern. For a premultiplied
// channel v and alpha a, a*(0.15 + 0.85*clamp(v/a)) is evaluated without division.
// Clamp the envelope only: preserve negative/HDR RGB and leave alpha unchanged.
void finish_grain(float* pixel, int layer_x, int layer_y, const Params& p,
                  float size, std::uint32_t seed) {
    const float alpha = pixel[3];
    if (alpha <= 0.0f) return;
    const auto gx = static_cast<std::uint32_t>(static_cast<int>(std::floor(static_cast<float>(layer_x) / size)));
    const auto gy = static_cast<std::uint32_t>(static_cast<int>(std::floor(static_cast<float>(layer_y) / size)));
    const auto h = hash32(seed ^ gx * 73856093u ^ gy * 19349663u);
    const float mono = (hash01(h) - 0.5f) * 2.0f;
    constexpr std::uint32_t salts[3] = {0x68bc21ebu, 0x02e5be93u, 0x967a889bu};
    for (int channel = 0; channel < 3; ++channel) {
        const float chromatic = (hash01(h ^ salts[channel]) - 0.5f) * 2.0f;
        const float noise = lerp(mono, chromatic, p.grain_color);
        const float weight = 0.3f * (0.15f * alpha + 0.85f * std::clamp(pixel[channel], 0.0f, alpha));
        pixel[channel] += noise * p.grain_amount * weight;
    }
}

struct CpuRenderWorkspace {
    // Glow and diffusion never need their mip pyramids at the same time.
    // Reusing one pyramid cuts peak/retained scratch memory roughly in half.
    MipPyramidRGBA pyramid;
    std::vector<float> depth_a;
    std::vector<float> depth_b;
    std::vector<float> softness_a;
    std::vector<float> softness_b;
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
    const float rounding = std::clamp(q.rounding, 0.0f, 4.0f);
    const float depth_a = q.depth_angle_deg * 3.14159265358979323846f / 180.0f;
    const float depth_dir_x = std::cos(depth_a);
    const float depth_dir_y = std::sin(depth_a);
    const float depth_inv_diag = 1.0f / std::max(1.0f, 0.5f * std::sqrt(bw * bw + bh * bh));
    const float turbulence_inv_x = 1.0f / (32.0f * std::max(0.1f, q.turbulence_size_x));
    const float turbulence_inv_y = 1.0f / (32.0f * std::max(0.1f, q.turbulence_size_y));
    const float turbulence_turns = q.turbulence_evolution / 360.0f;
    const float turbulence_angle = turbulence_turns * 6.28318530717958647692f;
    const float turbulence_angle_y = (turbulence_turns + 43.7f) * 6.28318530717958647692f;
    const float turbulence_zx = 0.5f * std::cos(turbulence_angle);
    const float turbulence_wx = 0.5f * std::sin(turbulence_angle);
    const float turbulence_zy = 0.5f * std::cos(turbulence_angle_y);
    const float turbulence_wy = 0.5f * std::sin(turbulence_angle_y);
    const float turbulence_pixels = q.turbulence_amount * 100.0f;
    const float softness_pixels = q.turbulence_softness * 100.0f;
    const int softness_radius = softness_pixels > 0.5f ? std::max(1, static_cast<int>(std::lround(softness_pixels / 3.0f))) : 0;
    const float grain_size = std::max(0.5f, q.grain_size_px);
    const float glow_threshold_inv = 1.0f / std::max(1e-5f, 1.0f - q.glow_threshold);
    const float glow_spread = std::max(0.35f, 0.45f * q.glow_falloff);

    const float* depth_map = nullptr;
    if (depth_active) {
        const std::size_t pixels = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);
        g_workspace.depth_a.resize(pixels);
        g_workspace.depth_b.resize(pixels);
        auto& depth_a_buf = g_workspace.depth_a;
        auto& depth_b_buf = g_workspace.depth_b;
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
                    depth_a_buf[static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x)] = depth;
                }
            }
        });

        // Recovered from Cosmic.aex: sigma = min(width,height) * 0.125 * 0.7 * Rounding,
        // then three separable H/V box passes with radius round(sigma/3), skipped at <=0.5.
        const float sigma = static_cast<float>(std::min(img.width, img.height)) * 0.0875f * rounding;
        const int radius = sigma > 0.5f ? std::max(1, static_cast<int>(std::lround(sigma / 3.0f))) : 0;
        if (radius > 0) {
            for (int pass = 0; pass < 3; ++pass) {
                box_blur_h(depth_a_buf, depth_b_buf, img.width, img.height, radius);
                box_blur_v(depth_b_buf, depth_a_buf, img.width, img.height, radius);
            }
        }
        depth_map = depth_a_buf.data();
    }

    float* glow_source = nullptr;
    if (plan.glow) {
        glow_source = g_workspace.pyramid.prepare_level0(img.width, img.height);
        if (!glow_source) return;
    } else {
        g_workspace.pyramid.clear();
    }

    CosmicTurbulenceCache turbulence_cache;
    if (plan.turbulence) {
        turbulence_cache.build(img.origin_x,img.origin_y,img.width,img.height,turbulence_inv_x,turbulence_inv_y,
                               turbulence_zx,turbulence_wx,turbulence_zy,turbulence_wy);
    }

    // Colorize in layer-space. Cosmic's Turbulence displaces X/Y pixel
    // coordinates using two independent normalized 4-D Perlin fields before the
    // directional gradient is evaluated. Amount is in pixels (host percent value),
    // Size is scaled by 32 pixels per UI unit, Evolution is an angle/turn.
    const bool softness_active = softness_radius > 0;
    if (softness_active) {
        const std::size_t samples=static_cast<std::size_t>(img.width)*static_cast<std::size_t>(img.height)*3u;
        g_workspace.softness_a.resize(samples);
        g_workspace.softness_b.resize(samples);
    }
    auto& softness_a=g_workspace.softness_a;
    auto& softness_b=g_workspace.softness_b;
    parallel_rows(0, img.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* src = img.src_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
            float* dst = img.dst_rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.stride_floats);
            for (int x = 0; x < img.width; ++x) {
                const float alpha = clamp01(src[x * 4 + 3]);
                float px=static_cast<float>(x), py=static_cast<float>(y);
                if (plan.turbulence) {
                    const float dx=turbulence_cache.sample_x(x,y);
                    const float dy=turbulence_cache.sample_y(x,y);
                    px += dx*turbulence_pixels;
                    py += dy*turbulence_pixels;
                }
                const float nx=(px-cx-0.5f)/bw, ny=(py-cy-0.5f)/bh;
                float u=(nx*dir_x+ny*dir_y)*q.cycles+0.5f+q.offset+q.phase_deg/360.0f;
                if (depth_active) {
                    const float depth=depth_map[static_cast<std::size_t>(y)*static_cast<std::size_t>(img.width)+static_cast<std::size_t>(x)];
                    u += depth*q.bulge;
                }
                const Color3f c=adjust_sat_brightness(sample_palette(q.colors,u),q.saturation,q.brightness);
                if (softness_active) {
                    const std::size_t i=(static_cast<std::size_t>(y)*static_cast<std::size_t>(img.width)+static_cast<std::size_t>(x))*3u;
                    softness_a[i+0]=c.r; softness_a[i+1]=c.g; softness_a[i+2]=c.b;
                    dst[x*4+3]=alpha; // preserve source alpha; RGB is finalized after Softness.
                } else {
                    const float r=c.r*alpha,g=c.g*alpha,bl=c.b*alpha;
                    dst[x*4+0]=r;dst[x*4+1]=g;dst[x*4+2]=bl;dst[x*4+3]=alpha;
                    if (plan.grain && !plan.glow && !plan.diffusion) finish_grain(dst+x*4,img.origin_x+x,img.origin_y+y,q,grain_size,seed);
                    if (plan.glow) {
                        const float lum=r*.2126f+g*.7152f+bl*.0722f;
                        const float k=clamp01((lum-q.glow_threshold)*glow_threshold_inv);
                        const std::size_t gi=(static_cast<std::size_t>(y)*static_cast<std::size_t>(img.width)+static_cast<std::size_t>(x))*4u;
                        glow_source[gi+0]=r*k;glow_source[gi+1]=g*k;glow_source[gi+2]=bl*k;glow_source[gi+3]=alpha*k;
                    }
                }
            }
        }
    });
    if (softness_active) {
        // Observable Softness path: three separable box pairs over unpremultiplied
        // color, then the source alpha mask is applied. UI value 40 => pass radius 13.
        for(int pass=0;pass<3;++pass){
            box_blur_rgb_h(softness_a,softness_b,img.width,img.height,softness_radius);
            box_blur_rgb_v(softness_b,softness_a,img.width,img.height,softness_radius);
        }
        parallel_rows(0,img.height,[&](int y0,int y1){
            for(int y=y0;y<y1;++y){
                float* dst=img.dst_rgba+static_cast<std::size_t>(y)*static_cast<std::size_t>(img.stride_floats);
                for(int x=0;x<img.width;++x){
                    const std::size_t i=(static_cast<std::size_t>(y)*static_cast<std::size_t>(img.width)+static_cast<std::size_t>(x))*3u;
                    const float alpha=dst[x*4+3];
                    const float r=softness_a[i+0]*alpha,g=softness_a[i+1]*alpha,bl=softness_a[i+2]*alpha;
                    dst[x*4+0]=r;dst[x*4+1]=g;dst[x*4+2]=bl;
                    if(plan.grain&&!plan.glow&&!plan.diffusion) finish_grain(dst+x*4,img.origin_x+x,img.origin_y+y,q,grain_size,seed);
                    if(plan.glow){
                        const float lum=r*.2126f+g*.7152f+bl*.0722f;
                        const float k=clamp01((lum-q.glow_threshold)*glow_threshold_inv);
                        const std::size_t gi=(static_cast<std::size_t>(y)*static_cast<std::size_t>(img.width)+static_cast<std::size_t>(x))*4u;
                        glow_source[gi+0]=r*k;glow_source[gi+1]=g*k;glow_source[gi+2]=bl*k;glow_source[gi+3]=alpha*k;
                    }
                }
            }
        });
    }

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
                    if (plan.grain && !plan.diffusion) {
                        finish_grain(dst + x * 4, img.origin_x + x, img.origin_y + y, q, grain_size, seed);
                    }
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
                if (plan.grain) {
                    finish_grain(dst + x * 4, img.origin_x + x, img.origin_y + y, q, grain_size, seed);
                }
            }
        }
    });
}

} // namespace stellar
