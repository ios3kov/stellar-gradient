#include "StellarBridge.h"
#include "BridgeInternal.hpp"
#include "../../src/core/Sanitize.h"
#include "../../src/cpu/ReferenceRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace {

struct Pixel8 { std::uint8_t alpha, red, green, blue; };
struct Pixel16 { std::uint16_t alpha, red, green, blue; };
struct PixelF32 { float alpha, red, green, blue; };
static_assert(sizeof(Pixel8) == 4);
static_assert(sizeof(Pixel16) == 8);
static_assert(sizeof(PixelF32) == 16);

constexpr float kAE16Max = 32768.0f;

bool empty(SGRectC r) { return r.right <= r.left || r.bottom <= r.top; }
SGRectC expand(SGRectC r, int32_t margin) {
    if (empty(r) || margin <= 0) return r;
    r.left -= margin; r.top -= margin; r.right += margin; r.bottom += margin;
    return r;
}
SGRectC intersect(SGRectC a, SGRectC b) {
    SGRectC r{std::max(a.left,b.left), std::max(a.top,b.top), std::min(a.right,b.right), std::min(a.bottom,b.bottom)};
    return empty(r) ? SGRectC{0,0,0,0} : r;
}
int32_t floor_multiple(int32_t v, int32_t q) {
    if (q <= 1) return v;
    const int32_t rem = v % q;
    return rem >= 0 ? v - rem : v - (rem + q);
}
int32_t ceil_multiple(int32_t v, int32_t q) {
    if (q <= 1) return v;
    const int32_t f = floor_multiple(v,q);
    return f == v ? v : f + q;
}
SGRectC align_rect(SGRectC r, int32_t q) {
    if (empty(r) || q <= 1) return r;
    r.left=floor_multiple(r.left,q); r.top=floor_multiple(r.top,q);
    r.right=ceil_multiple(r.right,q); r.bottom=ceil_multiple(r.bottom,q);
    return r;
}
int32_t mip_alignment(const stellar::Params& p) {
    float max_lod=0.0f;
    if (p.glow_intensity > 1.0e-6f && p.glow_radius_px > 0.5f) {
        const int samples = p.quality==stellar::Quality::Preview?1:(p.quality==stellar::Quality::Auto?3:5);
        const float spread_lod = samples>1 ? std::max(0.35f,0.45f*p.glow_falloff) : 0.0f;
        max_lod=std::max(max_lod,std::log2(std::max(1.0f,p.glow_radius_px))+spread_lod);
    }
    if (p.diffusion_blur_px > 0.5f) max_lod=std::max(max_lod,std::log2(std::max(1.0f,p.diffusion_blur_px)));
    int exponent=static_cast<int>(std::ceil(std::max(0.0f,max_lod)));
    if (p.quality==stellar::Quality::Preview) exponent=std::min(exponent,7);
    else if (p.quality==stellar::Quality::Auto) exponent=std::min(exponent,9);
    else exponent=std::min(exponent,12);
    return static_cast<int32_t>(1u << static_cast<unsigned>(std::max(0,exponent)));
}
int32_t work_margin(const stellar::Params& p) {
    float reach=0.0f;
    if (std::abs(p.bulge) > 1.0e-6f) reach=std::max(reach,0.5f*p.depth_softness_px+p.turbulence_amount_px+3.0f);
    if (p.glow_intensity > 1.0e-6f && p.glow_radius_px > 0.5f) {
        const int samples=p.quality==stellar::Quality::Preview?1:(p.quality==stellar::Quality::Auto?3:5);
        const float spread_lod=samples>1?std::max(0.35f,0.45f*p.glow_falloff):0.0f;
        reach=std::max(reach,p.glow_radius_px*std::exp2(spread_lod)+3.0f);
    }
    if (p.diffusion_blur_px > 0.5f) reach=std::max(reach,p.diffusion_blur_px+3.0f);
    return static_cast<int32_t>(std::ceil(std::max(0.0f,reach)));
}

void copy_to_canvas(const void* input_data, int32_t input_width, int32_t input_height, int32_t rowbytes,
                    int32_t bitdepth, std::vector<float>& canvas, int32_t cw, int32_t ch, int32_t offx, int32_t offy) {
    canvas.assign(static_cast<std::size_t>(cw)*static_cast<std::size_t>(ch)*4u,0.0f);
    const auto* base=static_cast<const std::uint8_t*>(input_data);
    for (int32_t sy=0; sy<input_height; ++sy) {
        const int32_t dy=sy+offy; if (dy<0||dy>=ch) continue;
        const auto* row=base+static_cast<std::ptrdiff_t>(sy)*static_cast<std::ptrdiff_t>(rowbytes);
        for (int32_t sx=0; sx<input_width; ++sx) {
            const int32_t dx=sx+offx; if (dx<0||dx>=cw) continue;
            const std::size_t i=(static_cast<std::size_t>(dy)*static_cast<std::size_t>(cw)+static_cast<std::size_t>(dx))*4u;
            if (bitdepth==8) {
                const auto& q=reinterpret_cast<const Pixel8*>(row)[sx];
                canvas[i]=q.red/255.0f; canvas[i+1]=q.green/255.0f; canvas[i+2]=q.blue/255.0f; canvas[i+3]=q.alpha/255.0f;
            } else if (bitdepth==16) {
                const auto& q=reinterpret_cast<const Pixel16*>(row)[sx];
                canvas[i]=q.red/kAE16Max; canvas[i+1]=q.green/kAE16Max; canvas[i+2]=q.blue/kAE16Max; canvas[i+3]=q.alpha/kAE16Max;
            } else {
                const auto& q=reinterpret_cast<const PixelF32*>(row)[sx];
                canvas[i]=q.red; canvas[i+1]=q.green; canvas[i+2]=q.blue; canvas[i+3]=q.alpha;
            }
        }
    }
}

void copy_from_canvas(const std::vector<float>& src, int32_t sw, int32_t crop_x, int32_t crop_y,
                      void* output_data, int32_t ow, int32_t oh, int32_t rowbytes, int32_t bitdepth) {
    auto* base=static_cast<std::uint8_t*>(output_data);
    for (int32_t y=0; y<oh; ++y) {
        auto* row=base+static_cast<std::ptrdiff_t>(y)*static_cast<std::ptrdiff_t>(rowbytes);
        for (int32_t x=0; x<ow; ++x) {
            const std::size_t i=(static_cast<std::size_t>(crop_y+y)*static_cast<std::size_t>(sw)+static_cast<std::size_t>(crop_x+x))*4u;
            if (bitdepth==8) {
                auto& q=reinterpret_cast<Pixel8*>(row)[x];
                q.red=static_cast<std::uint8_t>(std::lround(std::clamp(src[i],0.0f,1.0f)*255.0f));
                q.green=static_cast<std::uint8_t>(std::lround(std::clamp(src[i+1],0.0f,1.0f)*255.0f));
                q.blue=static_cast<std::uint8_t>(std::lround(std::clamp(src[i+2],0.0f,1.0f)*255.0f));
                q.alpha=static_cast<std::uint8_t>(std::lround(std::clamp(src[i+3],0.0f,1.0f)*255.0f));
            } else if (bitdepth==16) {
                auto& q=reinterpret_cast<Pixel16*>(row)[x];
                q.red=static_cast<std::uint16_t>(std::lround(std::clamp(src[i],0.0f,1.0f)*kAE16Max));
                q.green=static_cast<std::uint16_t>(std::lround(std::clamp(src[i+1],0.0f,1.0f)*kAE16Max));
                q.blue=static_cast<std::uint16_t>(std::lround(std::clamp(src[i+2],0.0f,1.0f)*kAE16Max));
                q.alpha=static_cast<std::uint16_t>(std::lround(std::clamp(src[i+3],0.0f,1.0f)*kAE16Max));
            } else {
                auto& q=reinterpret_cast<PixelF32*>(row)[x];
                q.red=src[i]; q.green=src[i+1]; q.blue=src[i+2]; q.alpha=std::clamp(src[i+3],0.0f,1.0f);
            }
        }
    }
}

} // namespace

namespace sgbridge {

stellar::Params to_cpp_params(const SGParamsC& c) {
    stellar::Params p;
    for (int i=0;i<5;++i) p.colors[static_cast<std::size_t>(i)]={c.colors[i].r,c.colors[i].g,c.colors[i].b};
    p.angle_deg=c.angle_deg; p.cycles=c.cycles; p.offset=c.offset; p.phase_deg=c.phase_deg;
    p.saturation=c.saturation; p.brightness=c.brightness;
    p.depth_angle_deg=c.depth_angle_deg; p.depth_contrast=c.depth_contrast; p.bulge=c.bulge; p.rounding=c.rounding;
    p.turbulence_amount_px=c.turbulence_amount; p.turbulence_size_x=c.turbulence_size_x; p.turbulence_size_y=c.turbulence_size_y;
    p.turbulence_evolution_deg=c.turbulence_evolution_deg; p.depth_softness_px=c.depth_softness_px;
    p.glow_radius_px=c.glow_radius_px; p.glow_falloff=c.glow_falloff; p.glow_threshold=c.glow_threshold;
    p.glow_intensity=c.glow_intensity; p.glow_soft_clip=c.glow_soft_clip;
    p.grain_amount=c.grain_amount; p.grain_size_px=c.grain_size_px; p.grain_color=c.grain_color; p.grain_animate=c.grain_animate!=0;
    p.diffusion_blur_px=c.diffusion_blur_px; p.diffusion_center={c.diffusion_center.x,c.diffusion_center.y};
    p.diffusion_focus_px=c.diffusion_focus_px; p.diffusion_feather_px=c.diffusion_feather_px; p.diffusion_invert=c.diffusion_invert!=0;
    p.quality=c.quality==0?stellar::Quality::Preview:(c.quality==2?stellar::Quality::Final:stellar::Quality::Auto);
    return p;
}

SGParamsC from_cpp_params(const stellar::Params& p) {
    SGParamsC c{};
    for (int i=0;i<5;++i) c.colors[i]={p.colors[static_cast<std::size_t>(i)].r,p.colors[static_cast<std::size_t>(i)].g,p.colors[static_cast<std::size_t>(i)].b};
    c.angle_deg=p.angle_deg; c.cycles=p.cycles; c.offset=p.offset; c.phase_deg=p.phase_deg;
    c.saturation=p.saturation; c.brightness=p.brightness;
    c.depth_angle_deg=p.depth_angle_deg; c.depth_contrast=p.depth_contrast; c.bulge=p.bulge; c.rounding=p.rounding;
    c.turbulence_amount=p.turbulence_amount_px; c.turbulence_size_x=p.turbulence_size_x; c.turbulence_size_y=p.turbulence_size_y;
    c.turbulence_evolution_deg=p.turbulence_evolution_deg; c.depth_softness_px=p.depth_softness_px;
    c.glow_radius_px=p.glow_radius_px; c.glow_falloff=p.glow_falloff; c.glow_threshold=p.glow_threshold; c.glow_intensity=p.glow_intensity; c.glow_soft_clip=p.glow_soft_clip;
    c.grain_amount=p.grain_amount; c.grain_size_px=p.grain_size_px; c.grain_color=p.grain_color; c.grain_animate=p.grain_animate?1u:0u;
    c.diffusion_blur_px=p.diffusion_blur_px; c.diffusion_center={p.diffusion_center.x,p.diffusion_center.y}; c.diffusion_focus_px=p.diffusion_focus_px; c.diffusion_feather_px=p.diffusion_feather_px; c.diffusion_invert=p.diffusion_invert?1u:0u;
    c.quality=p.quality==stellar::Quality::Preview?0u:(p.quality==stellar::Quality::Final?2u:1u);
    return c;
}

stellar::gpu::ParamsGPU pack_gpu(const SGRenderStateC& s, int work_w, int work_h, int out_w, int out_h,
                                 int src_w, int src_h, int sp, int dp, const stellar::RenderPlan& plan) {
    const stellar::Params p=to_cpp_params(s.params);
    stellar::gpu::ParamsGPU q{};
    for(int i=0;i<5;++i) q.colors[i]={p.colors[static_cast<std::size_t>(i)].r,p.colors[static_cast<std::size_t>(i)].g,p.colors[static_cast<std::size_t>(i)].b};
    q.angle_rad=p.angle_deg*3.14159265358979323846f/180.0f; q.cycles=p.cycles; q.offset=p.offset; q.phase=p.phase_deg/360.0f;
    q.saturation=p.saturation; q.brightness=p.brightness;
    const float da=p.depth_angle_deg*3.14159265358979323846f/180.0f;
    q.depth_dir_x=std::cos(da); q.depth_dir_y=std::sin(da); q.bulge=p.bulge;
    q.turbulence_amount=p.turbulence_amount_px; q.turbulence_scale_x=p.turbulence_size_x; q.turbulence_scale_y=p.turbulence_size_y; q.turbulence_evolution=p.turbulence_evolution_deg; q.depth_softness=p.depth_softness_px;
    q.grain_amount=p.grain_amount; q.grain_size=p.grain_size_px; q.grain_color=p.grain_color; q.grain_seed=p.grain_animate?s.frame_index*1664525u+1013904223u:0x12345678u;
    q.glow_radius=p.glow_radius_px; q.glow_falloff=p.glow_falloff; q.glow_threshold=p.glow_threshold; q.glow_intensity=p.glow_intensity; q.glow_soft_clip=p.glow_soft_clip;
    q.diffusion_blur=p.diffusion_blur_px; q.center_x=p.diffusion_center.x; q.center_y=p.diffusion_center.y; q.focus=p.diffusion_focus_px; q.feather=p.diffusion_feather_px; q.diffusion_invert=p.diffusion_invert?1:0;
    q.glow_samples=plan.glow_samples; q.max_mip_level=std::max(0,plan.max_mip_levels-1);
    q.width=work_w; q.height=work_h; q.src_pitch=sp; q.dst_pitch=dp; q.out_width=out_w; q.out_height=out_h;
    q.crop_x=s.output_rect.left-s.work_rect.left; q.crop_y=s.output_rect.top-s.work_rect.top;
    q.src_width=src_w; q.src_height=src_h; q.src_offset_x=s.input_rect.left-s.work_rect.left; q.src_offset_y=s.input_rect.top-s.work_rect.top;
    q.origin_x=s.work_rect.left; q.origin_y=s.work_rect.top;
    q.min_x=s.source_max_rect.left-s.work_rect.left; q.min_y=s.source_max_rect.top-s.work_rect.top;
    q.max_x=s.source_max_rect.right-s.work_rect.left-1; q.max_y=s.source_max_rect.bottom-s.work_rect.top-1;
    const float bw=static_cast<float>(std::max(1,q.max_x-q.min_x+1)); const float bh=static_cast<float>(std::max(1,q.max_y-q.min_y+1));
    q.dir_x=std::cos(q.angle_rad); q.dir_y=std::sin(q.angle_rad); q.inv_bw=1.0f/bw; q.inv_bh=1.0f/bh;
    q.bound_cx=0.5f*static_cast<float>(q.min_x+q.max_x); q.bound_cy=0.5f*static_cast<float>(q.min_y+q.max_y);
    q.phase_offset=q.offset+q.phase; q.depth_contrast=std::max(0.0f,p.depth_contrast); q.rounding_clamped=std::clamp(p.rounding,0.0f,1.0f);
    q.turbulence_inv_x=1.0f/(std::max(0.1f,q.turbulence_scale_x)*64.0f); q.turbulence_inv_y=1.0f/(std::max(0.1f,q.turbulence_scale_y)*64.0f);
    q.turbulence_evo_x=q.turbulence_evolution*0.013f; q.turbulence_evo_y=q.turbulence_evolution*0.017f;
    q.grain_inv_size=1.0f/std::max(0.5f,q.grain_size); q.glow_lod=plan.glow_lod; q.glow_spread=std::max(0.35f,0.45f*q.glow_falloff);
    q.glow_threshold_inv=1.0f/std::max(1.0e-5f,1.0f-q.glow_threshold); q.diffusion_lod=plan.diffusion_max_lod;
    q.diffusion_cx=static_cast<float>(q.min_x)+q.center_x*static_cast<float>(std::max(0,q.max_x-q.min_x));
    q.diffusion_cy=static_cast<float>(q.min_y)+q.center_y*static_cast<float>(std::max(0,q.max_y-q.min_y));
    q.diffusion_inv_feather=1.0f/std::max(1.0f,q.feather); q.depth_enabled=std::abs(q.bulge)>1.0e-6f?1u:0u;
    return q;
}

} // namespace sgbridge

extern "C" {

void sg_prepare_params(SGParamsC* params, float downsample_x, float downsample_y) {
    if (!params) return;
    stellar::Params p=sgbridge::to_cpp_params(*params);
    if (!std::isfinite(downsample_x)||downsample_x<=0.0f) downsample_x=1.0f;
    if (!std::isfinite(downsample_y)||downsample_y<=0.0f) downsample_y=1.0f;
    const float pixel_scale=std::sqrt(std::max(0.0f,downsample_x*downsample_y));
    p.turbulence_amount_px*=pixel_scale; p.depth_softness_px*=pixel_scale;
    p.glow_radius_px*=pixel_scale; p.diffusion_blur_px*=pixel_scale; p.diffusion_focus_px*=pixel_scale; p.diffusion_feather_px*=pixel_scale;
    p=stellar::sanitized_params(p);
    *params=sgbridge::from_cpp_params(p);
}

SGRectC sg_dependency_rect(const SGParamsC* params, SGRectC requested) {
    if (!params) return requested;
    const stellar::Params p=stellar::sanitized_params(sgbridge::to_cpp_params(*params));
    return align_rect(expand(requested,work_margin(p)),mip_alignment(p));
}

void sg_finalize_rects(const SGParamsC* params, SGRectC requested, SGRectC input_result, SGRectC input_max,
                       SGRectC* output_rect, SGRectC* work_rect, SGRectC* semantic_max_rect) {
    if (!params||!output_rect||!work_rect||!semantic_max_rect) return;
    const stellar::Params p=stellar::sanitized_params(sgbridge::to_cpp_params(*params));
    const int32_t margin=work_margin(p); const int32_t align=mip_alignment(p);
    *semantic_max_rect=expand(input_max,margin);
    *output_rect=intersect(requested,*semantic_max_rect);
    *work_rect=align_rect(expand(*output_rect,margin),align);
    (void)input_result;
}

int32_t sg_rect_empty(SGRectC r) { return empty(r)?1:0; }

int32_t sg_cpu_render(const SGRenderStateC* s, const void* input_data, int32_t iw, int32_t ih, int32_t irb,
                      void* output_data, int32_t ow, int32_t oh, int32_t orb, int32_t bitdepth) {
    if (!s||!input_data||!output_data||iw<=0||ih<=0||ow<=0||oh<=0||(bitdepth!=8&&bitdepth!=16&&bitdepth!=32)) return -1;
    try {
        const int32_t ww=s->work_rect.right-s->work_rect.left, wh=s->work_rect.bottom-s->work_rect.top;
        const int32_t crop_x=s->output_rect.left-s->work_rect.left, crop_y=s->output_rect.top-s->work_rect.top;
        if (ww<=0||wh<=0||crop_x<0||crop_y<0||crop_x+ow>ww||crop_y+oh>wh) return -1;
        const int32_t offx=s->input_rect.left-s->work_rect.left, offy=s->input_rect.top-s->work_rect.top;
        std::vector<float> canvas;
        copy_to_canvas(input_data,iw,ih,irb,bitdepth,canvas,ww,wh,offx,offy);
        const stellar::Bounds content{s->source_max_rect.left-s->work_rect.left,s->source_max_rect.top-s->work_rect.top,
            s->source_max_rect.right-s->work_rect.left-1,s->source_max_rect.bottom-s->work_rect.top-1};
        stellar::Params p=stellar::sanitized_params(sgbridge::to_cpp_params(s->params));
        stellar::ImageF32 img{ww,wh,ww*4,canvas.data(),canvas.data(),content,s->work_rect.left,s->work_rect.top};
        stellar::render_reference(img,p,s->time_seconds,s->frame_index);
        copy_from_canvas(canvas,ww,crop_x,crop_y,output_data,ow,oh,orb,bitdepth);
        return 0;
    } catch (const std::bad_alloc&) { return -2; }
      catch (...) { return -3; }
}

} // extern C
