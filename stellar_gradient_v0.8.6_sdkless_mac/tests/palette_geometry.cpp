#include "cpu/ReferenceRenderer.h"
#include "core/Math.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
stellar::Params base_params() {
    stellar::Params p;
    p.colors = {{{40.0f/255,13.0f/255,140.0f/255}, {55.0f/255,17.0f/255,191.0f/255},
                 {24.0f/255,14.0f/255,89.0f/255}, {7.0f/255,12.0f/255,38.0f/255},
                 {242.0f/255,75.0f/255,75.0f/255}}};
    p.bulge = p.turbulence_amount = p.turbulence_softness = p.glow_intensity = p.grain_amount = p.diffusion_blur_px = 0;
    p.quality = stellar::Quality::Final;
    return p;
}
std::array<double,3> expected_palette(const stellar::Params& p, double t) {
    const double phase = t - std::floor(t);
    const double segment = phase * 5.0;
    const auto index = static_cast<std::size_t>(std::floor(segment));
    const double f = segment - std::floor(segment);
    const auto a = p.colors[index], b = p.colors[(index+1)%5];
    return {{a.r*(1-f)+b.r*f, a.g*(1-f)+b.g*f, a.b*(1-f)+b.b*f}};
}
int errors = 0;
void near(float got, double expected, const char* label) {
    if (std::abs(static_cast<double>(got)-expected)>2e-5) {
        if (errors<6) std::fprintf(stderr,"FAIL %s: got=%.9g expected=%.9g\n",label,static_cast<double>(got),expected);
        ++errors;
    }
}
void render_case(stellar::Params p, int w, int h, stellar::Bounds bounds) {
    const int stride=w*4+12; // retain non-pixel row padding
    std::vector<float> src(static_cast<std::size_t>(stride*h),-777.0f), out(src.size(),-777.0f);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        auto i=static_cast<std::size_t>(y*stride+x*4);
        src[i]=src[i+1]=src[i+2]=0; src[i+3]=static_cast<float>((x+y)%5)/4;
    }
    stellar::ImageF32 image{w,h,stride,src.data(),out.data(),bounds,91,-43};
    stellar::render_reference(image,p,0,0);
    const double bw=bounds.max_x-bounds.min_x+1, bh=bounds.max_y-bounds.min_y+1;
    const double angle=p.angle_deg*3.14159265358979323846/180;
    for(int y=0;y<h;++y) {
        for(int x=0;x<w;++x) {
            const double nx=(x-bounds.min_x)/bw-0.5;
            const double ny=(y-bounds.min_y)/bh-0.5;
            const double t=(nx*std::cos(angle)+ny*std::sin(angle))*p.cycles+0.5+p.offset+p.phase_deg/360;
            const auto expected=expected_palette(p,t);
            const auto i=static_cast<std::size_t>(y*stride+x*4);
            for(std::size_t c=0;c<3;++c) near(out[i+c],expected[c]*src[i+3],"render coordinates/premultiply");
            near(out[i+3],src[i+3],"alpha");
        }
        for(int i=w*4;i<stride;++i) near(out[static_cast<std::size_t>(y*stride+i)],-777,"row padding");
    }
}
} // namespace

int main(int argc,char** argv) {
    auto p=base_params();
    // Optional owned replay output: raw native-endian RGBA float32, 512x288.
    // This is a local CPU replay, never an After Effects/GPU runtime claim.
    if(argc==3 && std::strcmp(argv[1],"--write-base-f32")==0) {
        constexpr int w=512,h=288;
        std::vector<float> input(w*h*4,1), output(input.size());
        stellar::ImageF32 image{w,h,w*4,input.data(),output.data(),{0,0,w-1,h-1},0,0};
        stellar::render_reference(image,p,0,0);
        std::ofstream stream(argv[2],std::ios::binary);
        stream.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()*sizeof(float)));
        return stream.good()?0:2;
    }
    if(argc!=1) { std::fputs("usage: stellar_palette_geometry [--write-base-f32 FILE]\n",stderr); return 2; }
    // Unequal fractions distinguish true linear interpolation from smoothstep.
    for(double t : {-3.93,-0.73,-0.07,0.0,0.03,0.07,0.2,0.47,0.8,0.97,1.0,1.03,11.47}) {
        const auto got=stellar::sample_palette(p.colors,static_cast<float>(t));
        const auto want=expected_palette(p,t);
        near(got.r,want[0],"linear palette r"); near(got.g,want[1],"linear palette g"); near(got.b,want[2],"linear palette b");
    }
    for(float angle : {0.0f,90.0f,180.0f,270.0f,37.0f,-53.0f}) {
        p.angle_deg=angle;
        render_case(p,65,41,{0,0,64,40});
        render_case(p,19,13,{-17,-9,47,31}); // translated/cropped semantic bounds
    }
    p.angle_deg=90;
    render_case(p,1,1,{0,0,0,0});
    p.cycles=2.35f;p.offset=-0.13f;p.phase_deg=123;
    render_case(p,65,41,{0,0,64,40});
    p=base_params();
    p.colors={{{-1,2,0},{3,-2,1},{2,3,-1},{-2,1,4},{4,0,2}}};
    render_case(p,65,41,{0,0,64,40}); // no unintended SDR clamp
    if(errors) {std::fprintf(stderr,"%d palette/geometry assertions failed\n",errors);return 1;}
    std::puts("PASS: linear cyclic palette, phase origin, rotations, translated bounds, stride, alpha and HDR");
}
