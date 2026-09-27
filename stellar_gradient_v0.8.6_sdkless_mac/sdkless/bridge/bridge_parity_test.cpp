#include "StellarBridge.h"
#include "../../src/cpu/ReferenceRenderer.h"
#include "../../src/core/Sanitize.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

struct PixelF32 { float alpha, red, green, blue; };

static SGParamsC defaults(){
    SGParamsC p{};
    float c[5][3]={{.12f,.05f,.34f},{.12f,.38f,.95f},{.67f,.17f,.95f},{1.f,.31f,.55f},{1.f,.75f,.18f}};
    for(int i=0;i<5;i++)p.colors[i]={c[i][0],c[i][1],c[i][2]};
    p.angle_deg=37;p.cycles=2.2f;p.saturation=1.1f;p.brightness=1.3f;p.depth_angle_deg=25.f;p.depth_contrast=1.4f;p.bulge=.35f;p.rounding=.4f;
    p.turbulence_amount=12.f;p.turbulence_size_x=2.5f;p.turbulence_size_y=4.f;p.turbulence_evolution_deg=21;p.depth_softness_px=.7f;
    p.glow_radius_px=24;p.glow_falloff=1.3f;p.glow_threshold=.15f;p.glow_intensity=.9f;p.glow_soft_clip=.2f;
    p.grain_amount=.02f;p.grain_size_px=1.5f;p.grain_color=.25f;p.grain_animate=1;
    p.diffusion_blur_px=12;p.diffusion_center={.43f,.56f};p.diffusion_focus_px=20;p.diffusion_feather_px=30;p.quality=2;
    return p;
}
int main(){
    const int w=96,h=64;
    std::vector<PixelF32> input((size_t)w*h), output((size_t)w*h);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){float a=(x>10&&x<85&&y>8&&y<55)?1.f:0.f;input[(size_t)y*w+x]={a,.1f*a,.2f*a,.3f*a};}
    SGRenderStateC s{};s.params=defaults();sg_prepare_params(&s.params,1,1);s.input_rect={0,0,w,h};s.source_max_rect={0,0,w,h};s.output_rect={0,0,w,h};s.work_rect={0,0,w,h};s.frame_index=7;s.time_seconds=.25;s.engine_mode=1;
    int rc=sg_cpu_render(&s,input.data(),w,h,w*(int)sizeof(PixelF32),output.data(),w,h,w*(int)sizeof(PixelF32),32);if(rc){std::printf("FAIL rc=%d\n",rc);return 1;}
    std::vector<float> src((size_t)w*h*4),ref((size_t)w*h*4);
    for(int i=0;i<w*h;i++){src[(size_t)i*4]=input[i].red;src[(size_t)i*4+1]=input[i].green;src[(size_t)i*4+2]=input[i].blue;src[(size_t)i*4+3]=input[i].alpha;}
    stellar::Params p;for(int i=0;i<5;i++)p.colors[(size_t)i]={s.params.colors[i].r,s.params.colors[i].g,s.params.colors[i].b};
    p.angle_deg=s.params.angle_deg;p.cycles=s.params.cycles;p.offset=s.params.offset;p.phase_deg=s.params.phase_deg;p.saturation=s.params.saturation;p.brightness=s.params.brightness;
    p.depth_contrast=s.params.depth_contrast;p.bulge=s.params.bulge;p.rounding=s.params.rounding;p.turbulence_amount=s.params.turbulence_amount;p.turbulence_size_x=s.params.turbulence_size_x;p.turbulence_size_y=s.params.turbulence_size_y;p.turbulence_evolution_deg=s.params.turbulence_evolution_deg;p.depth_softness_px=s.params.depth_softness_px;
    p.glow_radius_px=s.params.glow_radius_px;p.glow_falloff=s.params.glow_falloff;p.glow_threshold=s.params.glow_threshold;p.glow_intensity=s.params.glow_intensity;p.glow_soft_clip=s.params.glow_soft_clip;p.grain_amount=s.params.grain_amount;p.grain_size_px=s.params.grain_size_px;p.grain_color=s.params.grain_color;p.grain_animate=s.params.grain_animate!=0;
    p.diffusion_blur_px=s.params.diffusion_blur_px;p.diffusion_center={s.params.diffusion_center.x,s.params.diffusion_center.y};p.diffusion_focus_px=s.params.diffusion_focus_px;p.diffusion_feather_px=s.params.diffusion_feather_px;p.diffusion_invert=s.params.diffusion_invert!=0;p.quality=stellar::Quality::Final;
    ref=src;stellar::ImageF32 img{w,h,w*4,ref.data(),ref.data(),{0,0,w-1,h-1},0,0};stellar::render_reference(img,p,.25,7);
    float maxerr=0;for(int i=0;i<w*h;i++){maxerr=std::max(maxerr,std::abs(output[i].red-ref[(size_t)i*4]));maxerr=std::max(maxerr,std::abs(output[i].green-ref[(size_t)i*4+1]));maxerr=std::max(maxerr,std::abs(output[i].blue-ref[(size_t)i*4+2]));maxerr=std::max(maxerr,std::abs(output[i].alpha-ref[(size_t)i*4+3]));}
    std::printf("bridge parity max_err=%.9g\n",maxerr);return maxerr<=1e-6f?0:2;
}
