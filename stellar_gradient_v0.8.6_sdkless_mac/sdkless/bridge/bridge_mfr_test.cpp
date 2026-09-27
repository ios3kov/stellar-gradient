#include "StellarBridge.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

struct PixelF32 { float alpha, red, green, blue; };

static SGParamsC params() {
    SGParamsC p{};
    const float c[5][3]={{.12f,.05f,.34f},{.12f,.38f,.95f},{.67f,.17f,.95f},{1.f,.31f,.55f},{1.f,.75f,.18f}};
    for(int i=0;i<5;i++) p.colors[i]={c[i][0],c[i][1],c[i][2]};
    p.angle_deg=27.f; p.cycles=2.7f; p.saturation=1.1f; p.brightness=1.2f;
    p.depth_angle_deg=18.f; p.depth_contrast=1.3f; p.bulge=.4f; p.rounding=.35f;
    p.turbulence_amount=10.f; p.turbulence_size_x=2.4f; p.turbulence_size_y=3.8f; p.turbulence_evolution_deg=18.f; p.depth_softness_px=20.f;
    p.glow_radius_px=18.f; p.glow_falloff=1.4f; p.glow_threshold=.18f; p.glow_intensity=.7f; p.glow_soft_clip=.15f;
    p.grain_amount=.015f; p.grain_size_px=1.25f; p.grain_color=.2f; p.grain_animate=1;
    p.diffusion_blur_px=9.f; p.diffusion_center={.48f,.52f}; p.diffusion_focus_px=16.f; p.diffusion_feather_px=24.f; p.quality=2;
    return p;
}

int main() {
    constexpr int w=80,h=52,threads=8,iterations=8;
    std::vector<PixelF32> input((size_t)w*h);
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        const float a=(x>7&&x<73&&y>5&&y<47)?1.f:0.f;
        input[(size_t)y*w+x]={a,.11f*a,.22f*a,.33f*a};
    }
    SGRenderStateC s{}; s.params=params(); sg_prepare_params(&s.params,1,1);
    s.input_rect={0,0,w,h}; s.source_max_rect={0,0,w,h}; s.output_rect={0,0,w,h}; s.work_rect={0,0,w,h};
    s.frame_index=13; s.time_seconds=.5; s.engine_mode=1;

    std::vector<PixelF32> reference((size_t)w*h);
    if(sg_cpu_render(&s,input.data(),w,h,w*(int)sizeof(PixelF32),reference.data(),w,h,w*(int)sizeof(PixelF32),32)!=0) return 1;

    std::atomic<int> failed{0};
    std::atomic<float> max_err{0.f};
    std::vector<std::thread> workers;
    workers.reserve(threads);
    for(int t=0;t<threads;t++) workers.emplace_back([&] {
        for(int n=0;n<iterations;n++) {
            std::vector<PixelF32> out((size_t)w*h);
            if(sg_cpu_render(&s,input.data(),w,h,w*(int)sizeof(PixelF32),out.data(),w,h,w*(int)sizeof(PixelF32),32)!=0) { failed.fetch_add(1); return; }
            float local=0.f;
            for(size_t i=0;i<out.size();i++) {
                local=std::max(local,std::abs(out[i].red-reference[i].red));
                local=std::max(local,std::abs(out[i].green-reference[i].green));
                local=std::max(local,std::abs(out[i].blue-reference[i].blue));
                local=std::max(local,std::abs(out[i].alpha-reference[i].alpha));
            }
            float cur=max_err.load();
            while(cur<local && !max_err.compare_exchange_weak(cur,local)) {}
        }
    });
    for(auto& wkr:workers) wkr.join();
    std::printf("bridge MFR max_err=%.9g failures=%d\n",max_err.load(),failed.load());
    return failed.load()==0 && max_err.load()==0.f ? 0 : 2;
}
