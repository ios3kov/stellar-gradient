#include "cpu/ReferenceRenderer.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {
int fail(const char* msg){ std::fprintf(stderr,"FAIL: %s\n",msg); return 1; }
}

int main(){
    constexpr int W=96,H=64,S=W*4;
    std::vector<float> src(static_cast<std::size_t>(W)*H*4,0.0f), dst(src.size(),0.0f);
    for(int y=12;y<52;++y) for(int x=18;x<78;++x){
        const auto i=(static_cast<std::size_t>(y)*W+x)*4u;
        src[i+3]=((x+y)&3)?1.0f:0.35f;
    }
    const stellar::Bounds content{18,12,77,51};

    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> u01(0.0f,1.0f);
    for(int n=0;n<40;++n){
        stellar::Params p;
        p.angle_deg=-720.0f+1440.0f*u01(rng);
        p.cycles=0.1f+8.0f*u01(rng);
        p.offset=-2.0f+4.0f*u01(rng);
        p.phase_deg=-720.0f+1440.0f*u01(rng);
        p.saturation=2.0f*u01(rng);
        p.brightness=2.0f*u01(rng);
        p.depth_contrast=0.05f+4.0f*u01(rng);
        p.bulge=-1.0f+2.0f*u01(rng);
        p.rounding=u01(rng);
        p.depth_angle_deg=-360.0f+720.0f*u01(rng);
        p.turbulence_amount_px=120.0f*u01(rng);
        p.turbulence_size_x=0.1f+9.9f*u01(rng);
        p.turbulence_size_y=0.1f+9.9f*u01(rng);
        p.turbulence_evolution_deg=-360.0f+720.0f*u01(rng);
        p.depth_softness_px=120.0f*u01(rng);
        p.glow_radius_px=120.0f*u01(rng);
        p.glow_falloff=0.25f+3.75f*u01(rng);
        p.glow_threshold=u01(rng);
        p.glow_intensity=3.0f*u01(rng);
        p.glow_soft_clip=u01(rng);
        p.grain_amount=0.15f*u01(rng);
        p.grain_size_px=0.5f+7.5f*u01(rng);
        p.grain_color=u01(rng);
        p.diffusion_blur_px=96.0f*u01(rng);
        p.diffusion_center={u01(rng),u01(rng)};
        p.diffusion_focus_px=200.0f*u01(rng);
        p.diffusion_feather_px=250.0f*u01(rng);
        p.diffusion_invert=(n&1)!=0;
        p.quality=static_cast<stellar::Quality>(n%3);

        std::fill(dst.begin(),dst.end(),0.0f);
        stellar::ImageF32 img{W,H,S,src.data(),dst.data(),content};
        stellar::render_reference(img,p,double(n)/30.0,static_cast<std::uint32_t>(n));
        for(float v:dst) if(!std::isfinite(v)) return fail("randomized render produced non-finite value");
    }

    // Invalid input is a no-op, not a crash.
    stellar::Params p;
    stellar::ImageF32 invalid{W,H,W*4-1,src.data(),dst.data(),content};
    stellar::render_reference(invalid,p,0.0,0);

    std::puts("PASS: core_edge_cases");
    return 0;
}
