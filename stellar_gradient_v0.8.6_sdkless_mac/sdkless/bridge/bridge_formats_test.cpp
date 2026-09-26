#include "StellarBridge.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

struct P8 { std::uint8_t a,r,g,b; };
struct P16 { std::uint16_t a,r,g,b; };
struct PF { float a,r,g,b; };

static SGParamsC simple() {
    SGParamsC p{};
    for(auto& c:p.colors) c={.8f,.7f,.6f};
    p.angle_deg=0; p.cycles=1; p.saturation=1; p.brightness=2.5f;
    p.depth_contrast=1; p.turbulence_size_x=120; p.turbulence_size_y=120; p.turbulence_softness=.5f;
    p.glow_falloff=1.6f; p.glow_threshold=.2f; p.glow_soft_clip=0;
    p.grain_size_px=1; p.diffusion_center={.5f,.5f}; p.diffusion_focus_px=10; p.diffusion_feather_px=10; p.quality=2;
    return p;
}

int main() {
    constexpr int w=12,h=8;
    SGRenderStateC s{}; s.params=simple(); sg_prepare_params(&s.params,1,1);
    s.input_rect={0,0,w,h}; s.source_max_rect={0,0,w,h}; s.output_rect={0,0,w,h}; s.work_rect={0,0,w,h};

    std::vector<PF> f((size_t)w*h), fo((size_t)w*h);
    for(auto& q:f) q={1.f,.1f,.2f,.3f};
    if(sg_cpu_render(&s,f.data(),w,h,w*(int)sizeof(PF),fo.data(),w,h,w*(int)sizeof(PF),32)!=0) return 1;
    float peak=0.f; for(const auto& q:fo) peak=std::max({peak,q.r,q.g,q.b});
    if(!(peak>1.f)) { std::printf("FAIL HDR peak=%g\n",peak); return 2; }

    std::vector<P8> a((size_t)w*h), ao((size_t)w*h);
    for(auto& q:a) q={255,25,51,77};
    if(sg_cpu_render(&s,a.data(),w,h,w*(int)sizeof(P8),ao.data(),w,h,w*(int)sizeof(P8),8)!=0) return 3;
    std::uint8_t peak8=0; for(const auto& q:ao) peak8=std::max({peak8,q.r,q.g,q.b});
    if(peak8!=255) return 4;

    std::vector<P16> b((size_t)w*h), bo((size_t)w*h);
    for(auto& q:b) q={32768,3277,6554,9830};
    if(sg_cpu_render(&s,b.data(),w,h,w*(int)sizeof(P16),bo.data(),w,h,w*(int)sizeof(P16),16)!=0) return 5;
    std::uint16_t peak16=0; for(const auto& q:bo) peak16=std::max({peak16,q.r,q.g,q.b});
    if(peak16!=32768) return 6;

    std::printf("bridge formats PASS HDR_peak=%g\n",peak);
    return 0;
}
