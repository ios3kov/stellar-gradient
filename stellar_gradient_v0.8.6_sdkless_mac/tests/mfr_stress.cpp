#include "cpu/ReferenceRenderer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

namespace { int fail(const char* m,float e=0){std::fprintf(stderr,"FAIL: %s max_err=%g\n",m,e);return 1;} }

int main(){
    constexpr int W=320,H=192,S=W*4,THREADS=8;
    std::vector<float> src(static_cast<std::size_t>(W)*H*4u,0.0f);
    for(int y=20;y<H-20;++y) for(int x=24;x<W-24;++x){
        const float dx=(x-W*.5f)/(W*.39f),dy=(y-H*.5f)/(H*.34f);
        if(dx*dx+dy*dy<1.0f) src[(static_cast<std::size_t>(y)*W+x)*4u+3u]=((x+y)%7)?1.0f:0.4f;
    }
    stellar::Params p;
    p.angle_deg=43.0f;p.cycles=3.2f;p.phase_deg=127.0f;p.bulge=.55f;p.depth_contrast=1.9f;p.rounding=.7f;
    p.turbulence_amount=.23f;p.turbulence_size_x=63.0f;p.turbulence_size_y=91.0f;p.turbulence_evolution=88.0f;p.turbulence_softness=.8f;
    p.glow_radius_px=42.0f;p.glow_intensity=1.1f;p.glow_falloff=1.7f;p.grain_amount=.04f;p.grain_color=.4f;
    p.diffusion_blur_px=30.0f;p.diffusion_focus_px=40.0f;p.diffusion_feather_px=75.0f;p.quality=stellar::Quality::Final;
    const stellar::Bounds bounds{0,0,W-1,H-1};

    std::vector<float> reference(src.size(),0.0f);
    stellar::ImageF32 ri{W,H,S,src.data(),reference.data(),bounds,0,0};
    stellar::render_reference(ri,p,17.0/30.0,17);

    std::vector<std::vector<float>> outputs(THREADS,std::vector<float>(src.size(),0.0f));
    std::atomic<int> ready{0}; std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for(int t=0;t<THREADS;++t){
        threads.emplace_back([&,t]{
            ready.fetch_add(1,std::memory_order_release);
            while(!go.load(std::memory_order_acquire)) std::this_thread::yield();
            stellar::ImageF32 img{W,H,S,src.data(),outputs[static_cast<std::size_t>(t)].data(),bounds,0,0};
            stellar::render_reference(img,p,17.0/30.0,17);
        });
    }
    while(ready.load(std::memory_order_acquire)!=THREADS) std::this_thread::yield();
    go.store(true,std::memory_order_release);
    for(auto& t:threads)t.join();

    float max_err=0.0f;
    for(const auto& out:outputs) for(std::size_t i=0;i<out.size();++i) max_err=std::max(max_err,std::abs(out[i]-reference[i]));
    if(max_err>1.0e-6f) return fail("concurrent MFR render is non-deterministic",max_err);
    std::printf("PASS: mfr_stress %d concurrent renders max_err=%g\n",THREADS,max_err);
    return 0;
}
