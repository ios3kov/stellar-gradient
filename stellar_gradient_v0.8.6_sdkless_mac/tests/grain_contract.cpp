// Independent final-stage grain oracle; reference images are NOT embedded here.
#include "cpu/ReferenceRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
std::uint32_t hash(std::uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
double unit(std::uint32_t n) { return double(hash(n) & 0xffffffu) / 16777215.0; }
std::array<double, 3> noise(int x, int y, float size, float color, bool animate, std::uint32_t frame) {
    const std::uint32_t gx = static_cast<std::uint32_t>(static_cast<int>(std::floor(double(x)/size)));
    const std::uint32_t gy = static_cast<std::uint32_t>(static_cast<int>(std::floor(double(y)/size)));
    const auto seed = animate ? frame*1664525u+1013904223u : 0x12345678u;
    const auto h = hash(seed ^ gx*73856093u ^ gy*19349663u);
    const double mono = unit(h)*2-1;
    std::array<double, 3> n{};
    const std::array<std::uint32_t, 3> salt{{0x68bc21ebu,0x02e5be93u,0x967a889bu}};
    for (unsigned i=0;i<3;i++) n[i] = mono+(unit(h ^ salt[i])*2-1-mono)*color;
    return n;
}
std::vector<float> render(stellar::Params p, int w, int h, int stride, int ox, int oy, std::uint32_t frame) {
    std::vector<float> src(static_cast<std::size_t>(stride*h), -777), dst(src.size(), -777);
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        const auto i=static_cast<std::size_t>(y*stride+x*4);
        // Transparent, almost transparent, partial and opaque pixels.
        src[i]=src[i+1]=src[i+2]=0;
        src[i+3]=x%5==0?0.0f:(x%5==1?1e-8f:(x%5==2?.27f:1.0f));
    }
    stellar::ImageF32 img{w,h,stride,src.data(),dst.data(),{0,0,w-1,h-1},ox,oy};
    stellar::render_reference(img,p,frame/24.0,frame);
    return dst;
}
int emit(const char* file) {
    constexpr int w=512,h=288;
    stellar::Params p;
    const int colors[5][3]={{40,13,140},{55,17,191},{24,14,89},{7,12,38},{242,75,75}};
    for (int i=0;i<5;i++) p.colors[static_cast<std::size_t>(i)]={colors[i][0]/255.f,colors[i][1]/255.f,colors[i][2]/255.f};
    p.bulge=p.turbulence_amount=p.turbulence_softness=p.glow_intensity=p.diffusion_blur_px=0;
    p.grain_amount=.2f; p.grain_size_px=1; p.grain_color=1; p.grain_animate=false;
    std::vector<float> src(w*h*4,1),out(src.size(),0);
    stellar::ImageF32 img{w,h,w*4,src.data(),out.data(),{0,0,w-1,h-1},0,0};
    stellar::render_reference(img,p,0,0);
    FILE* f=std::fopen(file,"wb"); if(!f)return 2;
    const bool ok=std::fwrite(out.data(),sizeof(float),out.size(),f)==out.size();
    const int close=std::fclose(f);return ok&&close==0?0:2;
}
}
int main(int argc,char** argv) {
    if(argc==3&&std::strcmp(argv[1],"--write-grain-f32")==0)return emit(argv[2]);
    if(argc!=1)return 2;
    int failures=0,checks=0;
    constexpr int w=43,h=31,stride=w*4+7;
    for(int mode=0;mode<4;mode++) for(float size:{.5f,1.f,2.5f,11.f})
    for(float color:{0.f,.33f,1.f}) for(float amount:{0.f,1e-7f,.031f,.2f,2.f}) {
        stellar::Params p;
        p.bulge=p.turbulence_amount=0;
        p.glow_intensity=(mode&1)?.86f:0;
        p.glow_radius_px=12; p.diffusion_blur_px=(mode&2)?9.f:0;
        p.diffusion_focus_px=0; p.diffusion_feather_px=10;
        p.grain_size_px=size;p.grain_color=color;p.grain_animate=(mode&1)!=0;
        p.quality=stellar::Quality::Final;
        // Explicit extended-range contract: noise envelope clamps; RGB never does.
        p.colors={{{-.75f,.05f,2.2f},{1.8f,.6f,-.2f},{.15f,.04f,.5f},{.0f,.0f,.0f},{1.f,1.f,1.f}}};
        p.grain_amount=0;
        const auto no_grain=render(p,w,h,stride,-23,19,17);
        p.grain_amount=amount;
        const auto actual=render(p,w,h,stride,-23,19,17);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            const auto i=static_cast<std::size_t>(y*stride+x*4);
            const double alpha=no_grain[i+3];
            const auto n=noise(x-23,y+19,std::clamp(size,.5f,5.f),color,p.grain_animate,17);
            for(unsigned c=0;c<3;c++) {
                double expected=no_grain[i+c];
                if(amount>1e-6f&&alpha>0) {
                    const double straight=expected/alpha;
                    const double envelope=.3*(.15+.85*std::clamp(straight,0.0,1.0));
                    expected+=alpha*n[c]*amount*envelope;
                }
                ++checks;
                if(!std::isfinite(actual[i+c])||std::abs(actual[i+c]-expected)>5e-6)++failures;
            }
            ++checks;if(actual[i+3]!=no_grain[i+3])++failures;
        }
        for(int y=0;y<h;y++)for(int x=w*4;x<stride;x++) {
            ++checks;if(actual[static_cast<std::size_t>(y*stride+x)]!=-777)++failures;
        }
    }
    // Static grain is time-invariant; animated grain changes without changing alpha.
    stellar::Params p;p.bulge=p.turbulence_amount=p.glow_intensity=p.diffusion_blur_px=0;
    p.grain_animate=false;
    const auto a=render(p,w,h,stride,0,0,0), b=render(p,w,h,stride,0,0,7);
    ++checks;if(a!=b)++failures;
    p.grain_animate=true;
    const auto c=render(p,w,h,stride,0,0,0),d=render(p,w,h,stride,0,0,7);
    ++checks;if(c==d)++failures;
    std::printf("%s: grain final-stage contract, %d checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);
    return failures?1:0;
}
