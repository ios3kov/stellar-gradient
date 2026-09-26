#include "cpu/ReferenceRenderer.h"
#include "core/RenderPlan.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

bool nearly_equal(float a, float b, float eps) {
    return std::abs(a - b) <= eps;
}

} // namespace

int main() {
    constexpr int W=64,H=48,S=W*4;
    std::vector<float> src(W*H*4,0.0f), a(W*H*4,0.0f), b(W*H*4,0.0f);
    for(int y=8;y<40;++y) for(int x=10;x<54;++x) src[(y*W+x)*4+3]=1.0f;

    stellar::Params p;
    p.glow_radius_px=24.0f;
    p.diffusion_blur_px=0.0f;
    p.grain_animate=false;
    const stellar::Bounds content{10,8,53,39};
    stellar::ImageF32 ia{W,H,S,src.data(),a.data(),content};
    stellar::ImageF32 ib{W,H,S,src.data(),b.data(),content};
    stellar::render_reference(ia,p,0.0,0);
    stellar::render_reference(ib,p,0.0,0);

    for(std::size_t i=0;i<a.size();++i){
        if(!std::isfinite(a[i])) return fail("non-finite output");
        if(!nearly_equal(a[i],b[i],1e-7f)) return fail("non-deterministic output");
    }

    // Glow expands alpha outside the source silhouette.
    if(!(a[(20*W+8)*4+3] > 0.0f)) return fail("glow did not expand alpha");
    if(!(a[(20*W+20)*4+3] > 0.999f)) return fail("opaque source lost alpha");

    // Phase is a true 360-degree loop.
    std::vector<float> c(W*H*4,0.0f), d(W*H*4,0.0f);
    stellar::Params loop=p;
    loop.glow_intensity=0.0f;
    loop.grain_amount=0.0f;
    loop.phase_deg=0.0f;
    stellar::ImageF32 ic{W,H,S,src.data(),c.data(),content};
    stellar::render_reference(ic,loop,0.0,0);
    loop.phase_deg=360.0f;
    stellar::ImageF32 id{W,H,S,src.data(),d.data(),content};
    stellar::render_reference(id,loop,0.0,0);
    for(std::size_t i=0;i<c.size();++i) if(!nearly_equal(c[i],d[i],1e-6f)) return fail("phase loop mismatch");

    // In-place source/destination is supported and used by the AE CPU wrapper.
    std::vector<float> inplace=src, separate(W*H*4,0.0f);
    stellar::Params simple=loop;
    simple.phase_deg=17.0f;
    stellar::ImageF32 ii{W,H,S,inplace.data(),inplace.data(),content};
    stellar::ImageF32 is{W,H,S,src.data(),separate.data(),content};
    stellar::render_reference(ii,simple,0.0,0);
    stellar::render_reference(is,simple,0.0,0);
    for(std::size_t i=0;i<inplace.size();++i) if(!nearly_equal(inplace[i],separate[i],1e-6f)) return fail("in-place render mismatch");

    auto plan=stellar::make_render_plan(p,W,H,stellar::Backend::CPU);
    if(!plan.glow || plan.diffusion || plan.max_mip_levels<=1) return fail("render plan mismatch");

    std::puts("PASS: core_smoke");
    return 0;
}
