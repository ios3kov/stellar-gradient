#pragma once
static const char kStellarGradientMetalSource[] = R"STELLAR_MSL(
#include <metal_stdlib>
using namespace metal;

struct SGColor3 { float r,g,b; };
struct SGParamsGPU {
    SGColor3 colors[5];
    float angle_rad, cycles, offset, phase;
    float saturation, brightness;
    float depth_contrast, bulge, rounding;
    float turbulence_amount, turbulence_scale_x, turbulence_scale_y, turbulence_evolution, turbulence_softness;
    float grain_amount, grain_size, grain_color;
    uint grain_seed;
    float glow_radius, glow_falloff, glow_threshold, glow_intensity, glow_soft_clip;
    float diffusion_blur, center_x, center_y, focus, feather;
    int diffusion_invert;
    int glow_samples;
    int max_mip_level;
    int width, height, src_pitch, dst_pitch;
    int out_width, out_height, crop_x, crop_y;
    int src_width, src_height, src_offset_x, src_offset_y;
    int origin_x, origin_y;
    int min_x,min_y,max_x,max_y;
    float dir_x,dir_y,inv_bw,inv_bh;
    float bound_cx,bound_cy,phase_offset,depth_exp;
    float rounding_clamped,turbulence_inv_x,turbulence_inv_y,turbulence_evo_x;
    float turbulence_evo_y,grain_inv_size,glow_lod,glow_spread;
    float glow_threshold_inv,diffusion_lod,diffusion_cx,diffusion_cy;
    float diffusion_inv_feather;
    uint depth_enabled;
};

inline float clamp01(float x){return clamp(x,0.0f,1.0f);}
inline uint h32(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
inline float h01(uint x){return float(h32(x)&0x00ffffffu)*(1.0f/16777215.0f);}
inline float sm(float x){x=clamp01(x);return x*x*(3.0f-2.0f*x);}
inline float n2(float x,float y,uint seed){int ix=int(floor(x)),iy=int(floor(y));float fx=sm(x-float(ix)),fy=sm(y-float(iy));auto hh=[&](int xx,int yy){return h01(uint(xx)*0x9e3779b9u ^ uint(yy)*0x85ebca6bu ^ seed);};float a=hh(ix,iy),b=hh(ix+1,iy),c=hh(ix,iy+1),d=hh(ix+1,iy+1);return mix(mix(a,b,fx),mix(c,d,fx),fy)*2.0f-1.0f;}
inline float fbm(float x,float y,float soft,uint seed){float sum=0,amp=.5,norm=0;int oct=2+int(clamp01(soft)*3.0f);for(int i=0;i<5;i++){if(i>=oct)break;sum+=n2(x,y,seed+uint(i)*911u)*amp;norm+=amp;x*=2.03f;y*=2.03f;amp*=.5f;}return sum/max(norm,1e-6f);}
inline float3 palette(constant SGParamsGPU& p,float t){t=t-floor(t);float z=t*5.0f;int i0=int(floor(z))%5,i1=(i0+1)%5;float f=sm(z-floor(z));float3 a=float3(p.colors[i0].r,p.colors[i0].g,p.colors[i0].b),b=float3(p.colors[i1].r,p.colors[i1].g,p.colors[i1].b),c=mix(a,b,f);float y=dot(c,float3(.2126,.7152,.0722));return (y+(c-y)*p.saturation)*p.brightness;}
inline float soft_clip(float v,float s){return s>0.0f?v/(1.0f+s*max(0.0f,v-1.0f)):v;}
inline float4 load_bgra(device const float4* src,constant SGParamsGPU& p,uint2 gid){
    int sx=int(gid.x)-p.src_offset_x, sy=int(gid.y)-p.src_offset_y;
    if(sx<0||sy<0||sx>=p.src_width||sy>=p.src_height) return float4(0.0f);
    float4 q=src[uint(sy)*uint(p.src_pitch)+uint(sx)];
    return float4(q.z,q.y,q.x,q.w);
} // AE GPU worlds are BGRA128
inline void store_bgra(device float4* dst,int pitch,uint2 gid,float4 q){dst[gid.y*uint(pitch)+gid.x]=float4(q.z,q.y,q.x,q.w);}

inline float4 shade_base(device const float4* src, constant SGParamsGPU& p, uint2 gid) {
    float4 s=load_bgra(src,p,gid); float alpha=clamp01(s.a);
    float nx=(float(gid.x)-p.bound_cx)*p.inv_bw,ny=(float(gid.y)-p.bound_cy)*p.inv_bh;
    float u=(nx*p.dir_x+ny*p.dir_y)*p.cycles+p.phase_offset;
    float2 layer_pos=float2(int(gid.x)+p.origin_x,int(gid.y)+p.origin_y);
    if(p.turbulence_amount!=0){u+=fbm(layer_pos.x*p.turbulence_inv_x+p.turbulence_evo_x,layer_pos.y*p.turbulence_inv_y+p.turbulence_evo_y,p.turbulence_softness,0x6d2b79f5u)*p.turbulence_amount;}
    if(p.depth_enabled!=0u){ float dome=clamp01(1.0f-(nx*nx+ny*ny)*4.0f); dome=mix(dome,sm(dome),p.rounding_clamped); dome=pow(max(dome,1e-6f),p.depth_exp); u+=(dome-.5f)*p.bulge; }
    float3 c=palette(p,u);
    if(p.grain_amount>0){uint gx=uint(int(floor(layer_pos.x*p.grain_inv_size))),gy=uint(int(floor(layer_pos.y*p.grain_inv_size)));uint h=h32(p.grain_seed ^ gx*73856093u ^ gy*19349663u);float mono=(h01(h)-.5f)*2.0f*p.grain_amount;float3 chr=float3(h01(h^0x68bc21ebu),h01(h^0x02e5be93u),h01(h^0x967a889bu));chr=(chr-.5f)*2.0f*p.grain_amount;c+=mix(float3(mono),chr,p.grain_color);}
    return float4(c*alpha,alpha);
}

kernel void SGBaseKernel(device const float4* src [[buffer(0)]],
                         texture2d<float, access::write> base [[texture(0)]],
                         constant SGParamsGPU& p [[buffer(1)]],
                         uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=uint(p.width)||gid.y>=uint(p.height)) return;
    base.write(shade_base(src,p,gid),gid);
}

kernel void SGBaseGlowKernel(device const float4* src [[buffer(0)]],
                             texture2d<float, access::write> base [[texture(0)]],
                             texture2d<float, access::write> glow0 [[texture(1)]],
                             constant SGParamsGPU& p [[buffer(1)]],
                             uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=uint(p.width)||gid.y>=uint(p.height)) return;
    float4 v=shade_base(src,p,gid);
    base.write(v,gid);
    float lum=dot(v.rgb,float3(.2126,.7152,.0722));
    float k=clamp01((lum-p.glow_threshold)*p.glow_threshold_inv);
    glow0.write(v*k,gid);
}

kernel void SGBaseOutKernel(device const float4* src [[buffer(0)]],
                            device float4* dst [[buffer(1)]],
                            constant SGParamsGPU& p [[buffer(2)]],
                            uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=uint(p.width)||gid.y>=uint(p.height)) return;
    store_bgra(dst,p.dst_pitch,gid,shade_base(src,p,gid));
}

inline float4 composite_pixel(texture2d<float, access::read> base,
                              texture2d<float, access::sample> glowMip,
                              constant SGParamsGPU& p,uint2 gid) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
    float2 uv=(float2(gid)+.5f)/float2(p.width,p.height); float4 b=base.read(gid); float3 rgb=b.rgb; float alpha=b.a;
    if(p.glow_intensity>0.0f && p.glow_radius>0.5f){
        float lod=p.glow_lod; float spread=p.glow_spread; int n=max(1,p.glow_samples); float4 g=0.0f; float wsum=0.0f;
        for(int i=0;i<5;i++){if(i>=n) break; float centered=n==1?0.0f:(float(i)/float(n-1)-.5f)*2.0f; float w=1.0f-.22f*abs(centered); g+=glowMip.sample(s,uv,level(clamp(lod+centered*spread,0.0f,float(p.max_mip_level))))*w; wsum+=w;}
        g/=max(wsum,1e-6f); rgb+=g.rgb*p.glow_intensity; alpha=clamp01(alpha+g.a*p.glow_intensity);
        rgb=float3(soft_clip(rgb.r,p.glow_soft_clip),soft_clip(rgb.g,p.glow_soft_clip),soft_clip(rgb.b,p.glow_soft_clip));
    }
    return float4(rgb,alpha);
}

kernel void SGComposeInPlaceKernel(texture2d<float, access::read_write> base [[texture(0)]],
                                    texture2d<float, access::sample> glowMip [[texture(1)]],
                                    constant SGParamsGPU& p [[buffer(0)]],
                                    uint2 gid [[thread_position_in_grid]]) {
    if(gid.x>=uint(p.width)||gid.y>=uint(p.height)) return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
    float2 uv=(float2(gid)+.5f)/float2(p.width,p.height);
    float4 b=base.read(gid);
    float3 rgb=b.rgb;
    float alpha=b.a;
    if(p.glow_intensity>0.0f && p.glow_radius>0.5f){
        float lod=p.glow_lod;
        float spread=p.glow_spread;
        int n=max(1,p.glow_samples);
        float4 g=0.0f;
        float wsum=0.0f;
        for(int i=0;i<5;i++){
            if(i>=n) break;
            float centered=n==1?0.0f:(float(i)/float(n-1)-.5f)*2.0f;
            float w=1.0f-.22f*abs(centered);
            g+=glowMip.sample(s,uv,level(clamp(lod+centered*spread,0.0f,float(p.max_mip_level))))*w;
            wsum+=w;
        }
        g/=max(wsum,1e-6f);
        rgb+=g.rgb*p.glow_intensity;
        alpha=clamp01(alpha+g.a*p.glow_intensity);
        rgb=float3(soft_clip(rgb.r,p.glow_soft_clip),soft_clip(rgb.g,p.glow_soft_clip),soft_clip(rgb.b,p.glow_soft_clip));
    }
    base.write(float4(rgb,alpha),gid);
}

kernel void SGComposeOutKernel(texture2d<float, access::read> base [[texture(0)]],
                               texture2d<float, access::sample> glowMip [[texture(1)]],
                               device float4* dst [[buffer(0)]],constant SGParamsGPU& p [[buffer(1)]],uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=uint(p.out_width)||gid.y>=uint(p.out_height)) return;
    uint2 work_gid=uint2(gid.x+uint(p.crop_x),gid.y+uint(p.crop_y));
    store_bgra(dst,p.dst_pitch,gid,composite_pixel(base,glowMip,p,work_gid));
}

kernel void SGDiffusionOutKernel(texture2d<float, access::sample> composedMip [[texture(0)]],
                                 device float4* dst [[buffer(0)]],constant SGParamsGPU& p [[buffer(1)]],uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=uint(p.out_width)||gid.y>=uint(p.out_height)) return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
    uint2 work_gid=uint2(gid.x+uint(p.crop_x),gid.y+uint(p.crop_y));
    float2 uv=(float2(work_gid)+.5f)/float2(p.width,p.height);
    float4 sharp=composedMip.sample(s,uv,level(0.0f));
    float2 center=float2(p.diffusion_cx,p.diffusion_cy);
    float amount=sm((distance(float2(work_gid),center)-p.focus)*p.diffusion_inv_feather);
    if(p.diffusion_invert!=0) amount=1.0f-amount;
    float lod=amount*p.diffusion_lod;
    float4 q=composedMip.sample(s,uv,level(lod));
    store_bgra(dst,p.dst_pitch,gid,mix(sharp,q,amount));
}

)STELLAR_MSL";
