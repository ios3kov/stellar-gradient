#include <cuda_runtime.h>
#include <math_constants.h>

struct SGColor3 { float r,g,b; };
struct SGParamsGPU {
    SGColor3 colors[5];
    float angle_rad, cycles, offset, phase;
    float saturation, brightness;
    float depth_dir_x, depth_dir_y, bulge;
    float turbulence_amount, turbulence_scale_x, turbulence_scale_y, turbulence_evolution, depth_softness;
    float depth_contrast, rounding;
    float grain_amount, grain_color;
    unsigned int grain_seed;
    float glow_threshold, glow_intensity, glow_soft_clip;
    float diffusion_blur, center_x, center_y, focus, feather;
    int diffusion_invert;
    int min_x,min_y,max_x,max_y;
};

__device__ inline float sg_clamp01(float x){return fminf(1.f,fmaxf(0.f,x));}
__device__ inline float sg_fract(float x){return x-floorf(x);}
__device__ inline float sg_smooth(float x){x=sg_clamp01(x);return x*x*(3.f-2.f*x);}
__device__ inline unsigned sg_hash(unsigned x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
__device__ inline float sg_h01(unsigned x){return float(sg_hash(x)&0x00ffffffU)*(1.f/16777215.f);}

__device__ inline float sg_noise_hash(int xx,int yy,unsigned seed){
    return sg_h01(unsigned(xx)*0x9e3779b9U ^ unsigned(yy)*0x85ebca6bU ^ seed);
}
__device__ float sg_noise(float x,float y,unsigned seed){
    int ix=(int)floorf(x), iy=(int)floorf(y); float fx=sg_smooth(x-ix), fy=sg_smooth(y-iy);
    float a=sg_noise_hash(ix,iy,seed),b=sg_noise_hash(ix+1,iy,seed),c=sg_noise_hash(ix,iy+1,seed),d=sg_noise_hash(ix+1,iy+1,seed);
    return ((a+(b-a)*fx) + ((c+(d-c)*fx)-(a+(b-a)*fx))*fy)*2.f-1.f;
}

__device__ float sg_fbm(float x,float y,float soft,unsigned seed){
    float sum=0.f,amp=.5f,norm=0.f; int oct=2+(int)(sg_clamp01(soft)*3.f);
    #pragma unroll
    for(int i=0;i<5;++i){ if(i>=oct) break; sum+=sg_noise(x,y,seed+unsigned(i)*911U)*amp; norm+=amp; x*=2.03f;y*=2.03f;amp*=.5f; }
    return sum/fmaxf(norm,1e-6f);
}

__device__ SGColor3 sg_palette(const SGParamsGPU& p,float t){
    t=sg_fract(t); float x=t*5.f; int i0=((int)floorf(x))%5,i1=(i0+1)%5; float f=sg_smooth(x-floorf(x));
    SGColor3 a=p.colors[i0],b=p.colors[i1]; SGColor3 c{a.r+(b.r-a.r)*f,a.g+(b.g-a.g)*f,a.b+(b.b-a.b)*f};
    float y=c.r*.2126f+c.g*.7152f+c.b*.0722f;
    c.r=(y+(c.r-y)*p.saturation)*p.brightness; c.g=(y+(c.g-y)*p.saturation)*p.brightness; c.b=(y+(c.b-y)*p.saturation)*p.brightness;
    return c;
}

// Fused base pass: alpha -> gradient coordinate -> turbulence/depth -> palette -> grain -> premultiply.
extern "C" __global__ void SGBaseKernel(
    const float4* __restrict__ src,
    float4* __restrict__ dst,
    int width,int height,int srcPitchPixels,int dstPitchPixels,
    SGParamsGPU p)
{
    int x=blockIdx.x*blockDim.x+threadIdx.x, y=blockIdx.y*blockDim.y+threadIdx.y;
    if(x>=width||y>=height)return;
    float4 s=src[y*srcPitchPixels+x]; float alpha=sg_clamp01(s.w);
    float bw=fmaxf(1.f,float(p.max_x-p.min_x+1)), bh=fmaxf(1.f,float(p.max_y-p.min_y+1));
    float cx=.5f*float(p.min_x+p.max_x), cy=.5f*float(p.min_y+p.max_y);
    float nx=(float(x)-cx)/bw, ny=(float(y)-cy)/bh;
    float u=(nx*cosf(p.angle_rad)+ny*sinf(p.angle_rad))*p.cycles+p.offset+p.phase;
    // CUDA host parity will be completed with the Windows build path.
    (void)p.depth_dir_x;(void)p.depth_dir_y;(void)p.depth_softness;(void)p.depth_contrast;(void)p.rounding;
    SGColor3 c=sg_palette(p,u);
    if(p.grain_amount>0.f){
        unsigned h=sg_hash(p.grain_seed ^ unsigned(x)*73856093U ^ unsigned(y)*19349663U);
        float mono=(sg_h01(h)-.5f)*2.f*p.grain_amount;
        float rr=(sg_h01(h^0x68bc21ebU)-.5f)*2.f*p.grain_amount;
        float gg=(sg_h01(h^0x02e5be93U)-.5f)*2.f*p.grain_amount;
        float bb=(sg_h01(h^0x967a889bU)-.5f)*2.f*p.grain_amount;
        c.r += mono+(rr-mono)*p.grain_color; c.g += mono+(gg-mono)*p.grain_color; c.b += mono+(bb-mono)*p.grain_color;
    }
    dst[y*dstPitchPixels+x]=make_float4(c.r*alpha,c.g*alpha,c.b*alpha,alpha);
}

// Glow source + 2x downsample fused. Each output pixel consumes a 2x2 tile.
extern "C" __global__ void SGGlowDown2Kernel(
    const float4* __restrict__ src,float4* __restrict__ dst,
    int srcW,int srcH,int srcPitch,int dstPitch,float threshold)
{
    int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;
    int dw=(srcW+1)>>1,dh=(srcH+1)>>1;if(x>=dw||y>=dh)return;
    float4 acc=make_float4(0,0,0,0);int n=0;
    #pragma unroll
    for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox){int sx=x*2+ox,sy=y*2+oy;if(sx<srcW&&sy<srcH){float4 v=src[sy*srcPitch+sx];float lum=v.x*.2126f+v.y*.7152f+v.z*.0722f;float k=sg_clamp01((lum-threshold)/fmaxf(1e-5f,1.f-threshold));acc.x+=v.x*k;acc.y+=v.y*k;acc.z+=v.z*k;acc.w+=v.w*k;++n;}}
    float inv=1.f/fmaxf(1,n);dst[y*dstPitch+x]=make_float4(acc.x*inv,acc.y*inv,acc.z*inv,acc.w*inv);
}

extern "C" __global__ void SGUpsampleAddKernel(
    const float4* __restrict__ low,float4* __restrict__ high,
    int lowW,int lowH,int highW,int highH,int lowPitch,int highPitch,float gain)
{
    int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=highW||y>=highH)return;
    float u=(float(x)+.5f)*float(lowW)/float(highW)-.5f, v=(float(y)+.5f)*float(lowH)/float(highH)-.5f;
    int x0=max(0,min(lowW-1,(int)floorf(u))),y0=max(0,min(lowH-1,(int)floorf(v)));int x1=min(lowW-1,x0+1),y1=min(lowH-1,y0+1);float fx=u-floorf(u),fy=v-floorf(v);
    float4 a=low[y0*lowPitch+x0],b=low[y0*lowPitch+x1],c=low[y1*lowPitch+x0],d=low[y1*lowPitch+x1];
    float4 q;
    q.x=(a.x+(b.x-a.x)*fx)+((c.x+(d.x-c.x)*fx)-(a.x+(b.x-a.x)*fx))*fy;
    q.y=(a.y+(b.y-a.y)*fx)+((c.y+(d.y-c.y)*fx)-(a.y+(b.y-a.y)*fx))*fy;
    q.z=(a.z+(b.z-a.z)*fx)+((c.z+(d.z-c.z)*fx)-(a.z+(b.z-a.z)*fx))*fy;
    q.w=(a.w+(b.w-a.w)*fx)+((c.w+(d.w-c.w)*fx)-(a.w+(b.w-a.w)*fx))*fy;
    float4 h=high[y*highPitch+x];high[y*highPitch+x]=make_float4(h.x+q.x*gain,h.y+q.y*gain,h.z+q.z*gain,h.w);
}

extern "C" __global__ void SGCompositeKernel(
    const float4* __restrict__ base,const float4* __restrict__ glow,float4* __restrict__ out,
    int width,int height,int basePitch,int glowPitch,int outPitch,float glowIntensity,float softClip)
{
    int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=width||y>=height)return;
    float4 b=base[y*basePitch+x],g=glow?glow[y*glowPitch+x]:make_float4(0,0,0,0);float3 c=make_float3(b.x+g.x*glowIntensity,b.y+g.y*glowIntensity,b.z+g.z*glowIntensity);
    if(softClip>0.f){c.x=c.x/(1.f+softClip*fmaxf(0.f,c.x-1.f));c.y=c.y/(1.f+softClip*fmaxf(0.f,c.y-1.f));c.z=c.z/(1.f+softClip*fmaxf(0.f,c.z-1.f));}
    out[y*outPitch+x]=make_float4(c.x,c.y,c.z,b.w);
}
