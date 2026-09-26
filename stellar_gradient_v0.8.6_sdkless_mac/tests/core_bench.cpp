#include "cpu/ReferenceRenderer.h"
#include <chrono>
#include <cstdio>
#include <vector>

static double measure(int w,int h,int frames,const stellar::Params& p){
    std::vector<float> src(static_cast<std::size_t>(w)*h*4,0.0f), dst(src.size());
    for(int y=h/8;y<h-h/8;++y) for(int x=w/8;x<w-w/8;++x){
        auto i=(static_cast<std::size_t>(y)*w+x)*4; src[i+3]=1.0f;
    }
    stellar::ImageF32 img{w,h,w*4,src.data(),dst.data()};
    stellar::render_reference(img,p,0.0,0);
    const auto t0=std::chrono::steady_clock::now();
    for(int f=0;f<frames;++f) stellar::render_reference(img,p,double(f)/30.0,static_cast<unsigned>(f));
    const auto t1=std::chrono::steady_clock::now();
    return std::chrono::duration<double,std::milli>(t1-t0).count()/frames;
}

static void run_case(int w,int h,int frames){
    stellar::Params base;
    base.glow_intensity=0.0f;
    base.diffusion_blur_px=0.0f;
    base.grain_amount=0.0f;
    base.turbulence_amount=0.0f;

    stellar::Params procedural=base;
    procedural.turbulence_amount=.22f;
    procedural.grain_amount=.02f;

    stellar::Params full=procedural;
    full.glow_radius_px=96.0f;
    full.glow_intensity=1.0f;
    full.diffusion_blur_px=72.0f;
    full.quality=stellar::Quality::Auto;

    std::printf("%dx%d base: %.3f ms/frame\n",w,h,measure(w,h,frames,base));
    std::printf("%dx%d procedural: %.3f ms/frame\n",w,h,measure(w,h,frames,procedural));
    std::printf("%dx%d full: %.3f ms/frame\n",w,h,measure(w,h,frames,full));
}
int main(){ run_case(640,360,3); run_case(1280,720,2); return 0; }
