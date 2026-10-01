#include "cpu/ReferenceRenderer.h"
#include "core/MipPyramid.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
int fail(const char* msg){ std::fprintf(stderr,"FAIL: %s\n",msg); return 1; }

std::vector<float> render_with_alpha(float alpha, stellar::Params p) {
    constexpr int W=96,H=72,S=W*4;
    std::vector<float> src(static_cast<std::size_t>(W)*H*4u,0.0f), dst(src.size(),0.0f);
    for(int y=18;y<54;++y) for(int x=24;x<72;++x) src[(static_cast<std::size_t>(y)*W+x)*4u+3u]=alpha;
    stellar::ImageF32 img{W,H,S,src.data(),dst.data(),{24,18,71,53},0,0};
    stellar::render_reference(img,p,0.0,3);
    return dst;
}
}

int main(){
    // Corrupt projects/expressions must not feed NaN/Inf into floor/pow/log2 or GPU packing.
    stellar::Params bad;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    bad.angle_deg=nan; bad.cycles=inf; bad.offset=-inf; bad.phase_deg=nan;
    bad.saturation=inf; bad.brightness=nan; bad.depth_contrast=-inf; bad.bulge=inf; bad.rounding=nan;
    bad.turbulence_amount=inf; bad.turbulence_size_x=nan; bad.turbulence_size_y=-inf; bad.turbulence_evolution=inf; bad.turbulence_softness=nan;
    bad.glow_radius_px=inf; bad.glow_falloff=nan; bad.glow_threshold=inf; bad.glow_intensity=nan; bad.glow_soft_clip=-inf;
    bad.grain_amount=inf; bad.grain_size_px=nan; bad.grain_color=-inf;
    bad.diffusion_blur_px=inf; bad.diffusion_center={nan,inf}; bad.diffusion_focus_px=-inf; bad.diffusion_feather_px=nan;
    bad.quality=static_cast<stellar::Quality>(255);
    for(auto& c:bad.colors){ c.r=nan; c.g=inf; c.b=-inf; }
    const auto safe=render_with_alpha(0.5f,bad);
    for(float v:safe) if(!std::isfinite(v)) return fail("NaN/Inf parameters escaped sanitization");

    // F32/HDR path must preserve values above 1.0 rather than clipping RGB.
    stellar::Params hdr;
    hdr.brightness=4.0f; hdr.saturation=1.0f;
    hdr.glow_intensity=0.0f; hdr.grain_amount=0.0f; hdr.turbulence_amount=0.0f; hdr.turbulence_softness=0.0f; hdr.diffusion_blur_px=0.0f; hdr.bulge=0.0f;
    const auto opaque=render_with_alpha(1.0f,hdr);
    const auto half=render_with_alpha(0.5f,hdr);
    float max_rgb=0.0f;
    constexpr int W=96;
    const std::size_t center=(static_cast<std::size_t>(36)*W+48u)*4u;
    for(std::size_t i=0;i<opaque.size();i+=4u) max_rgb=std::max({max_rgb,opaque[i],opaque[i+1],opaque[i+2]});
    if(max_rgb<=1.0f) return fail("HDR RGB was unexpectedly clipped to SDR");
    for(int c=0;c<3;++c){
        const float expected=opaque[center+static_cast<std::size_t>(c)]*0.5f;
        if(std::abs(half[center+static_cast<std::size_t>(c)]-expected)>2.0e-6f) return fail("premultiplied alpha proportionality broken");
    }
    if(std::abs(half[center+3u]-0.5f)>1.0e-6f) return fail("alpha changed in base HDR render");
    if(opaque[0]!=0.0f||opaque[1]!=0.0f||opaque[2]!=0.0f||opaque[3]!=0.0f) return fail("transparent pixels contain unassociated RGB");

    // 8K planning must be overflow-safe. Full FP32 RGBA pyramid is expected to
    // be large; runtime workspace trimming prevents it from remaining pinned.
    std::size_t bytes=0;
    if(!stellar::MipPyramidRGBA::estimate_storage_bytes(7680,4320,0,&bytes)) return fail("8K mip byte estimate overflowed");
    if(bytes < 600u*1024u*1024u || bytes > 800u*1024u*1024u) return fail("8K mip estimate outside expected envelope");

    stellar::MipPyramidRGBA pyramid;
    if(!pyramid.prepare_level0(1024,1024)) return fail("workspace allocation failed");
    pyramid.generate_mips();
    if(pyramid.retained_bytes()==0) return fail("workspace retained-byte accounting broken");
    pyramid.trim_retained_bytes(1u*1024u*1024u);
    if(pyramid.retained_bytes()!=0) return fail("workspace memory trim failed");

    // Preserve legitimate working-space over-range/under-range palette values.
    stellar::Params wide;
    wide.colors[0]={4.0f,-0.25f,2.0f};
    wide.colors[1]={3.0f,0.5f,1.5f};
    wide.glow_intensity=0.0f; wide.grain_amount=0.0f; wide.turbulence_amount=0.0f; wide.turbulence_softness=0.0f; wide.diffusion_blur_px=0.0f; wide.bulge=0.0f;
    const auto wide_out=render_with_alpha(1.0f,wide);
    bool has_over=false,has_under=false;
    for(std::size_t i=0;i<wide_out.size();i+=4u){
        has_over = has_over || wide_out[i]>1.0f || wide_out[i+1]>1.0f || wide_out[i+2]>1.0f;
        has_under = has_under || wide_out[i]<0.0f || wide_out[i+1]<0.0f || wide_out[i+2]<0.0f;
    }
    if(!has_over) return fail("HDR palette over-range value was clipped");
    if(!has_under) return fail("working-space under-range value was clipped");

    // 1x1 and fully transparent sources must remain finite even with maximum blur radii.
    {
        float src[4]={0,0,0,0}, dst[4]={123,123,123,123};
        stellar::Params edge; edge.glow_radius_px=2000.0f; edge.glow_intensity=10.0f; edge.diffusion_blur_px=2000.0f; edge.quality=stellar::Quality::Final;
        stellar::ImageF32 one{1,1,4,src,dst,{0,0,0,0},0,0};
        stellar::render_reference(one,edge,0.0,0);
        for(float v:dst) if(!std::isfinite(v)) return fail("1x1 extreme render produced non-finite value");
        if(dst[0]!=0.0f||dst[1]!=0.0f||dst[2]!=0.0f||dst[3]!=0.0f) return fail("transparent 1x1 extreme render revealed nonzero pixels");
    }

    std::puts("PASS: hardening (NaN/Inf, HDR/premult, HDR palette, 1x1, 8K memory planning)");
    return 0;
}
