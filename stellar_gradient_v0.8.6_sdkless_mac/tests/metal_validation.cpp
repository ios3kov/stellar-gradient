#include "../sdkless/bridge/MetalValidation.hpp"
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
std::uint64_t checks = 0;
void check(bool ok, const char* name) {
    ++checks;
    if (!ok && ++failures <= 12) std::fprintf(stderr, "FAIL: %s\n", name);
}
SGRenderStateC fixture() {
    SGRenderStateC s{};
    s.work_rect = {-4,-6,20,18}; s.output_rect = {0,0,12,8};
    s.input_rect = {-2,-3,14,12}; s.source_max_rect = {0,0,12,8};
    return s;
}
bool layout(const SGRenderStateC& s, sgbridge::MetalRenderLayout& out) {
    return sgbridge::metal_render_layout(s,16,15,272,12,8,208,out);
}
}
int main() {
    using namespace sgbridge;
    MetalPlaneLayout p;
    // Exhaustive small-plane oracle uses byte arithmetic, independent of the
    // production pixel-stride calculation. Includes every non-pixel remainder.
    for (int w=-1; w<=32; ++w) for (int h=-1; h<=12; ++h)
    for (int rb=-1; rb<=600; ++rb) {
        const bool expected = w>0 && h>0 && rb>0 && rb%16==0 && rb>=w*16;
        const bool actual = metal_plane_layout(w,h,rb,p);
        check(actual==expected,"small plane validation");
        if (actual) {
            check(p.required_bytes==static_cast<std::uint64_t>((h-1)*rb+w*16),"last pixel span");
            check(p.pitch_pixels*16==rb,"no pitch truncation");
        } else check(p.pitch_pixels==0 && p.required_bytes==0,"invalid result cleared");
    }
    constexpr auto imax = std::numeric_limits<std::int32_t>::max();
    constexpr auto imin = std::numeric_limits<std::int32_t>::min();
    constexpr std::uint64_t umax = std::numeric_limits<std::uint32_t>::max();
    check(metal_plane_layout(2,imax,32,p),"large valid index without allocation");
    check(p.required_bytes==(static_cast<std::uint64_t>(imax)*32),"64-bit span");
    check(metal_plane_layout(2,3,2147483632,p),"largest aligned signed rowbytes");
    check(!metal_plane_layout(imax,1,2147483632,p),"minimum width overflow avoided");
    check(!metal_plane_layout(2,imax,48,p),"shader uint indexing overflow");
    check(!metal_plane_layout(2,2,imin,p),"INT_MIN pitch");
    check(metal_plane_layout(65536,65536,1048576,p),"last UINT_MAX index permitted");
    check(p.required_bytes==(umax+1)*16,"UINT_MAX boundary span");
    check(!metal_plane_layout(65536,65537,1048576,p),"one row past UINT_MAX rejected");

    MetalRenderLayout r;
    const auto good = fixture();
    check(layout(good,r),"translated work and padded unequal pitches");
    check(r.work_width==24 && r.work_height==24,"work dimensions");
    check(r.input.required_bytes==14*272+16*16 && r.output.required_bytes==7*208+12*16,"independent spans");
    check(metal_buffers_fit(r,r.input.required_bytes,r.output.required_bytes),"exact final-pixel capacity");
    check(!metal_buffers_fit(r,r.input.required_bytes-1,r.output.required_bytes),"input one byte short");
    check(!metal_buffers_fit(r,r.input.required_bytes,r.output.required_bytes-1),"output one byte short");
    check(metal_buffers_fit(r,std::numeric_limits<std::uint64_t>::max(),std::numeric_limits<std::uint64_t>::max()),"larger buffers");
    check(!metal_buffers_fit({},1024,1024),"uninitialized layout");
    auto s=good; s.output_rect.left=-5; check(!layout(s,r),"negative crop");
    s=good; s.output_rect.left=9; check(!layout(s,r),"crop right overflow");
    s=good; s.output_rect.top=11; check(!layout(s,r),"crop bottom overflow");
    s=good; s.work_rect.right=-4; check(!layout(s,r),"empty work");
    s=good; s.work_rect.left=imin; s.work_rect.right=imax; check(!layout(s,r),"signed work overflow");
    s=good; s.input_rect.left=imin; check(!layout(s,r),"signed sample subtraction overflow");
    s=good; s.source_max_rect.right=imax; check(!layout(s,r),"relative bounds overflow");
    s=good; s.source_max_rect={1100000000,0,1100000012,8}; check(!layout(s,r),"bound center sum overflow");
    s=good; s.source_max_rect={5,5,5,5}; check(layout(s,r),"empty semantic bounds preserved");
    s=good; s.source_max_rect={6,5,5,5}; check(!layout(s,r),"inverted semantic bounds");
    s=good; s.input_rect={-100,-100,-84,-85}; check(layout(s,r),"zero-filled input outside work is valid");
    // Wide random scalar values exercise all arithmetic under UBSan without
    // allocating memory. Encoding is separate; no GPU/AE is claimed here.
    std::uint32_t seed=0x98ab;
    auto next=[&]() { seed=seed*1664525u+1013904223u;
        return static_cast<std::int32_t>(static_cast<std::int64_t>(seed)+imin); };
    for (int i=0;i<100000;++i) {
        s.work_rect={next(),next(),next(),next()};
        s.input_rect={next(),next(),next(),next()};
        s.source_max_rect={next(),next(),next(),next()};
        s.output_rect={next(),next(),next(),next()};
        const bool valid=layout(s,r);
        if (!valid) check(r.work_width==0 && r.input.required_bytes==0,"failed request clears output");
    }
    std::printf("Metal scalar validation: %llu checks, %d failures; GPU NOT RUN\n",
                static_cast<unsigned long long>(checks),failures);
    return failures ? 1 : 0;
}
