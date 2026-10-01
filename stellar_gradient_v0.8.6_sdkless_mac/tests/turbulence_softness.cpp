#include "cpu/ReferenceRenderer.h"
#include "core/Math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int failures = 0;
void near(float got, float want, float eps, const char* label) {
    if (std::abs(got - want) > eps) {
        if (failures < 12) std::fprintf(stderr, "FAIL %s got=%.9g want=%.9g\n", label, double(got), double(want));
        ++failures;
    }
}
void require(bool ok, const char* label) {
    if (!ok) { if (failures < 12) std::fprintf(stderr, "FAIL %s\n", label); ++failures; }
}

struct Rendered { std::vector<float> src, dst; };
Rendered render(stellar::Params p, int w=96, int h=64) {
    Rendered r;
    r.src.assign(static_cast<std::size_t>(w) * h * 4u, 0.0f);
    r.dst.assign(r.src.size(), 0.0f);
    for (int y=0; y<h; ++y) for (int x=0; x<w; ++x) {
        const std::size_t i=(static_cast<std::size_t>(y)*w+static_cast<std::size_t>(x))*4u;
        const float a=0.2f+0.8f*static_cast<float>((x*7+y*11)%31)/30.0f;
        r.src[i+3]=a;
    }
    p.glow_intensity=0.0f; p.grain_amount=0.0f; p.diffusion_blur_px=0.0f;
    p.quality=stellar::Quality::Final;
    stellar::ImageF32 img{w,h,w*4,r.src.data(),r.dst.data(),{0,0,w-1,h-1},17,-23};
    stellar::render_reference(img,p,0.0,0);
    return r;
}

double mean_abs_rgb(const std::vector<float>& a,const std::vector<float>& b) {
    double sum=0.0; std::size_t n=0;
    for(std::size_t i=0;i<a.size();i+=4) for(int c=0;c<3;++c){ sum+=std::abs(double(a[i+c])-double(b[i+c])); ++n; }
    return n?sum/double(n):0.0;
}
}

int main() {
    // Lock exact recovered 4-D turbulence field samples. These values are from
    // the independently decoded Cosmic PTX contract, not renderer goldens.
    struct Sample { float x,y,turns,dx,dy; };
    const std::array<Sample,4> samples{{
        {0.0f,0.0f,0.0f,-0.142857149f,-0.0658483058f},
        {0.5208333333f,0.3125f,0.0f,-0.440735430f,0.259574175f},
        {2.71875f,-1.234375f,0.1583333333f,0.127715349f,-0.118329056f},
        {-3.125f,7.75f,-0.2f,0.281344324f,0.0949763432f},
    }};
    constexpr float tau=6.28318530717958647692f;
    for(const auto& q:samples){
        const float ax=q.turns*tau, ay=(q.turns+43.7f)*tau;
        const float dx=stellar::cosmic_fbm4(q.x,q.y,0.5f*std::cos(ax),0.5f*std::sin(ax));
        const float dy=stellar::cosmic_fbm4(q.x+137.5f,q.y+91.3f,0.5f*std::cos(ay),0.5f*std::sin(ay));
        near(dx,q.dx,3.0e-6f,"PTX turbulence X sample");
        near(dy,q.dy,3.0e-6f,"PTX turbulence Y sample");
        const float ax2=(q.turns+1.0f)*tau, ay2=(q.turns+44.7f)*tau;
        near(dx,stellar::cosmic_fbm4(q.x,q.y,0.5f*std::cos(ax2),0.5f*std::sin(ax2)),2.0e-5f,"evolution +360 X");
        near(dy,stellar::cosmic_fbm4(q.x+137.5f,q.y+91.3f,0.5f*std::cos(ay2),0.5f*std::sin(ay2)),2.0e-5f,"evolution +360 Y");
    }

    stellar::Params p;
    p.colors={{{-2.0f,0.15f,3.0f},{-1.4f,2.2f,4.0f},{-0.8f,1.5f,2.8f},{-2.2f,3.0f,4.2f},{-1.1f,1.3f,3.6f}}};
    p.angle_deg=33.0f; p.cycles=2.1f; p.depth_angle_deg=19.0f;
    p.bulge=0.6f; p.rounding=1.0f; p.turbulence_amount=0.0f;
    p.turbulence_softness=0.0f;
    const auto sharp=render(p);
    p.turbulence_softness=0.4f;
    const auto soft=render(p);
    require(mean_abs_rgb(sharp.dst,soft.dst)>0.01,"Softness changes Depth even when Turbulence Amount is zero");
    bool saw_negative=false,saw_hdr=false;
    for(std::size_t i=0;i<soft.dst.size();i+=4){
        near(soft.dst[i+3],soft.src[i+3],1.0e-6f,"Softness preserves source alpha");
        for(int c=0;c<3;++c){ saw_negative|=soft.dst[i+c]<0.0f; saw_hdr|=soft.dst[i+c]>soft.dst[i+3]; }
    }
    require(saw_negative,"Softness preserves negative working-space RGB");
    require(saw_hdr,"Softness preserves extended-range RGB");

    // Turbulence is independent of Softness and uses UI Amount as pixel displacement.
    stellar::Params t;
    t.bulge=0.0f; t.rounding=0.0f; t.turbulence_softness=0.0f;
    t.turbulence_amount=0.0f; const auto t0=render(t);
    t.turbulence_amount=0.4f; t.turbulence_size_x=3.0f; t.turbulence_size_y=3.0f; const auto t3=render(t);
    t.turbulence_size_x=6.0f; t.turbulence_size_y=6.0f; const auto t6=render(t);
    require(mean_abs_rgb(t0.dst,t3.dst)>0.01,"Amount 40 changes output");
    require(mean_abs_rgb(t3.dst,t6.dst)>0.001,"Size 3 and Size 6 are distinct");

    // Precomputation may change where coordinate state is evaluated, but never
    // the field itself. Compare the cached renderer with direct cosmic_fbm4.
    constexpr int W=96,H=64;
    constexpr float tau2=6.28318530717958647692f;
    const float bw=static_cast<float>(W), bh=static_cast<float>(H);
    const float cx=0.5f*static_cast<float>(W-1), cy=0.5f*static_cast<float>(H-1);
    const float angle=t.angle_deg*3.14159265358979323846f/180.0f;
    const float dir_x=std::cos(angle), dir_y=std::sin(angle);
    const float inv_x=1.0f/(32.0f*std::max(0.1f,t.turbulence_size_x));
    const float inv_y=1.0f/(32.0f*std::max(0.1f,t.turbulence_size_y));
    const float turns=t.turbulence_evolution/360.0f;
    const float ax=turns*tau2, ay=(turns+43.7f)*tau2;
    const float zx=0.5f*std::cos(ax), wx=0.5f*std::sin(ax);
    const float zy=0.5f*std::cos(ay), wy=0.5f*std::sin(ay);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x){
        const std::size_t i=(static_cast<std::size_t>(y)*static_cast<std::size_t>(W)+static_cast<std::size_t>(x))*4u;
        const float layer_x=static_cast<float>(17+x), layer_y=static_cast<float>(-23+y);
        const float nx4=layer_x*inv_x, ny4=layer_y*inv_y;
        const float dx=stellar::cosmic_fbm4(nx4,ny4,zx,wx);
        const float dy=stellar::cosmic_fbm4(nx4+137.5f,ny4+91.3f,zy,wy);
        const float px=static_cast<float>(x)+dx*t.turbulence_amount*100.0f;
        const float py=static_cast<float>(y)+dy*t.turbulence_amount*100.0f;
        const float nx=(px-cx-0.5f)/bw, ny=(py-cy-0.5f)/bh;
        const float u=(nx*dir_x+ny*dir_y)*t.cycles+0.5f+t.offset+t.phase_deg/360.0f;
        const stellar::Color3f c=stellar::adjust_sat_brightness(stellar::sample_palette(t.colors,u),t.saturation,t.brightness);
        const float alpha=t6.src[i+3];
        near(t6.dst[i+0],c.r*alpha,2.0e-6f,"cached turbulence renderer R");
        near(t6.dst[i+1],c.g*alpha,2.0e-6f,"cached turbulence renderer G");
        near(t6.dst[i+2],c.b*alpha,2.0e-6f,"cached turbulence renderer B");
        near(t6.dst[i+3],alpha,1.0e-6f,"cached turbulence renderer alpha");
    }

    if(failures){ std::fprintf(stderr,"%d turbulence/softness checks failed\n",failures); return 1; }
    std::puts("PASS: recovered 4-D turbulence, 360-degree evolution, independent Softness blur, alpha/HDR contracts");
    return 0;
}
