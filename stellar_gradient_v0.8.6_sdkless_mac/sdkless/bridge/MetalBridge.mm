#include "StellarBridge.h"
#include "BridgeInternal.hpp"
#include "MetalValidation.hpp"
#include "../../src/core/Sanitize.h"
#include "../../src/gpu/StellarGradientMetalSource.generated.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <new>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

namespace {

struct SGMetalContext {
    id<MTLComputePipelineState> depth = nil;
    id<MTLComputePipelineState> base = nil;
    id<MTLComputePipelineState> base_out = nil;
    id<MTLComputePipelineState> base_glow = nil;
    id<MTLComputePipelineState> compose_in_place = nil;
    id<MTLComputePipelineState> compose_out = nil;
    id<MTLComputePipelineState> diffusion_out = nil;
    bool supports_f32_filtering = false;
};

id<MTLComputePipelineState> make_pipeline(id<MTLDevice> d, id<MTLLibrary> l, const char* name, NSError** error) {
    NSString* n=[NSString stringWithUTF8String:name];
    id<MTLFunction> f=[l newFunctionWithName:n];
    if(!f) return nil;
    id<MTLComputePipelineState> p=[d newComputePipelineStateWithFunction:f error:error];
    [f release];
    return p;
}

void release_context(SGMetalContext* g) {
    if(!g) return;
    if(g->depth)[g->depth release];
    if(g->base)[g->base release];
    if(g->base_out)[g->base_out release];
    if(g->base_glow)[g->base_glow release];
    if(g->compose_in_place)[g->compose_in_place release];
    if(g->compose_out)[g->compose_out release];
    if(g->diffusion_out)[g->diffusion_out release];
    delete g;
}

void encode_2d(id<MTLComputeCommandEncoder> enc,id<MTLComputePipelineState> pipe,NSUInteger w,NSUInteger h) {
    NSUInteger tw=pipe.threadExecutionWidth?pipe.threadExecutionWidth:16;
    NSUInteger th=std::max<NSUInteger>(1,std::min<NSUInteger>(8,pipe.maxTotalThreadsPerThreadgroup/tw));
    [enc dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(tw,th,1)];
}

id<MTLTexture> make_texture(id<MTLDevice> dev,int w,int h,int levels,MTLPixelFormat format) {
    MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:(NSUInteger)w height:(NSUInteger)h mipmapped:levels>1];
    d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    d.storageMode=MTLStorageModePrivate;
    if(levels>1) d.mipmapLevelCount=(NSUInteger)levels;
    return [dev newTextureWithDescriptor:d];
}

} // namespace

extern "C" {

void* sg_metal_create(void* mtl_device, int32_t* supports_f32_filtering) {
    if(supports_f32_filtering) *supports_f32_filtering=0;
    if(!mtl_device) return nullptr;
    NSAutoreleasePool* pool=[[NSAutoreleasePool alloc] init];
    id<MTLDevice> dev=(id<MTLDevice>)mtl_device;
    auto* g=new(std::nothrow) SGMetalContext();
    if(!g){ [pool drain]; return nullptr; }

    if([dev respondsToSelector:@selector(supports32BitFloatFiltering)]) {
        g->supports_f32_filtering=dev.supports32BitFloatFiltering;
    }

    NSError* error=nil;
    NSString* source=[NSString stringWithUTF8String:kStellarGradientMetalSource];
    MTLCompileOptions* options=[[MTLCompileOptions alloc] init];
    // Preserve the quality contract: never allow unsafe/fast floating-point math.
    // macOS 15 SDK deprecated fastMathEnabled in favor of mathMode +
    // mathFloatingPointFunctions. Keep an old-SDK/runtime fallback without
    // turning deprecation warnings into build failures under -Werror.
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
    if (@available(macOS 15.0, *)) {
        options.mathMode=MTLMathModeSafe;
        options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled=NO;
#pragma clang diagnostic pop
    }
#else
    options.fastMathEnabled=NO;
#endif
    id<MTLLibrary> lib=[dev newLibraryWithSource:source options:options error:&error];
    [options release];
    if(!lib){ release_context(g); [pool drain]; return nullptr; }

    g->depth=make_pipeline(dev,lib,"SGDepthKernel",&error);
    g->base=make_pipeline(dev,lib,"SGBaseKernel",&error);
    g->base_out=make_pipeline(dev,lib,"SGBaseOutKernel",&error);
    g->base_glow=make_pipeline(dev,lib,"SGBaseGlowKernel",&error);
    g->compose_in_place=make_pipeline(dev,lib,"SGComposeInPlaceKernel",&error);
    g->compose_out=make_pipeline(dev,lib,"SGComposeOutKernel",&error);
    g->diffusion_out=make_pipeline(dev,lib,"SGDiffusionOutKernel",&error);
    [lib release];

    if(!g->depth||!g->base||!g->base_out||!g->base_glow||!g->compose_in_place||!g->compose_out||!g->diffusion_out) {
        release_context(g); [pool drain]; return nullptr;
    }
    if(supports_f32_filtering) *supports_f32_filtering=g->supports_f32_filtering?1:0;
    [pool drain];
    return g;
}

void sg_metal_destroy(void* context) {
    release_context(static_cast<SGMetalContext*>(context));
}

int32_t sg_metal_render(void* context, void* command_queue, void* input_buffer, void* output_buffer,
                        const SGRenderStateC* state, int32_t iw, int32_t ih, int32_t irb,
                        int32_t ow, int32_t oh, int32_t orb) {
    if(!context||!command_queue||!input_buffer||!output_buffer||!state) return -1;
    sgbridge::MetalRenderLayout layout;
    if(!sgbridge::metal_render_layout(*state,iw,ih,irb,ow,oh,orb,layout)) return -1;
    // MTLBuffer.length is a byte count. Validate both complete logical planes
    // before touching pipelines, allocating textures or creating a command buffer.
    id<MTLBuffer> src=(id<MTLBuffer>)input_buffer;
    id<MTLBuffer> dst=(id<MTLBuffer>)output_buffer;
    if(!sgbridge::metal_buffers_fit(layout,static_cast<uint64_t>(src.length),
                                   static_cast<uint64_t>(dst.length))) return -1;
    NSAutoreleasePool* pool=[[NSAutoreleasePool alloc] init];
    SGMetalContext* g=static_cast<SGMetalContext*>(context);
    id<MTLCommandQueue> queue=(id<MTLCommandQueue>)command_queue;
    id<MTLDevice> dev=queue.device;
    id<MTLTexture> base=nil,glow=nil,depth=nil,depth_tmp=nil;
    int32_t rc=0;

    do {
        const int work_w=layout.work_width;
        const int work_h=layout.work_height;
        const int sp=layout.input.pitch_pixels;
        const int dp=layout.output.pitch_pixels;
        stellar::Params p=stellar::sanitized_params(sgbridge::to_cpp_params(state->params));
        const auto plan=stellar::make_render_plan(p,work_w,work_h,stellar::Backend::GPU);
        const auto gp=sgbridge::pack_gpu(*state,work_w,work_h,ow,oh,iw,ih,sp,dp,plan);

        id<MTLCommandBuffer> cb=[queue commandBuffer];
        if(!cb){ rc=-3; break; }

        depth=make_texture(dev,work_w,work_h,1,MTLPixelFormatR32Float); if(!depth){ rc=-2; break; }
        depth_tmp=make_texture(dev,work_w,work_h,1,MTLPixelFormatR32Float); if(!depth_tmp){ rc=-2; break; }

        id<MTLComputeCommandEncoder> depth_encoder=[cb computeCommandEncoder]; if(!depth_encoder){ rc=-3; break; }
        [depth_encoder setComputePipelineState:g->depth];
        [depth_encoder setBuffer:src offset:0 atIndex:0];
        [depth_encoder setTexture:depth atIndex:0];
        [depth_encoder setBytes:&gp length:sizeof(gp) atIndex:1];
        encode_2d(depth_encoder,g->depth,(NSUInteger)work_w,(NSUInteger)work_h);
        [depth_encoder endEncoding];

        if(gp.depth_enabled!=0u) {
            const float rounding_radius=static_cast<float>(std::min(work_w,work_h))*0.0875f*std::clamp(p.rounding,0.0f,1.0f);
            if(rounding_radius>=0.5f) {
                const int pass_radius=std::max(1,(int)std::lround(rounding_radius/3.0f));
                const NSUInteger kernel=(NSUInteger)(pass_radius*2+1);
                MPSImageBox* box=[[MPSImageBox alloc] initWithDevice:dev kernelWidth:kernel kernelHeight:kernel];
                box.edgeMode=MPSImageEdgeModeClamp;
                for(int pass=0;pass<3;++pass) {
                    [box encodeToCommandBuffer:cb sourceTexture:depth destinationTexture:depth_tmp];
                    id<MTLTexture> swap=depth; depth=depth_tmp; depth_tmp=swap;
                }
                [box release];
            }
        }

        if(!plan.glow&&!plan.diffusion&&work_w==ow&&work_h==oh&&gp.crop_x==0&&gp.crop_y==0) {
            id<MTLComputeCommandEncoder> e=[cb computeCommandEncoder]; if(!e){ rc=-3; break; }
            [e setComputePipelineState:g->base_out]; [e setBuffer:src offset:0 atIndex:0]; [e setBuffer:dst offset:0 atIndex:1]; [e setTexture:depth atIndex:0]; [e setBytes:&gp length:sizeof(gp) atIndex:2];
            encode_2d(e,g->base_out,(NSUInteger)ow,(NSUInteger)oh); [e endEncoding]; [cb commit];
            break;
        }

        const int levels=std::max(1,plan.max_mip_levels);
        const MTLPixelFormat fmt=p.quality==stellar::Quality::Preview?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA32Float;
        base=make_texture(dev,work_w,work_h,plan.diffusion?levels:1,fmt); if(!base){ rc=-2; break; }
        if(plan.glow){ glow=make_texture(dev,work_w,work_h,levels,fmt); if(!glow){ rc=-2; break; } }

        id<MTLComputeCommandEncoder> e=[cb computeCommandEncoder]; if(!e){ rc=-3; break; }
        if(plan.glow) {
            [e setComputePipelineState:g->base_glow]; [e setBuffer:src offset:0 atIndex:0]; [e setTexture:base atIndex:0]; [e setTexture:glow atIndex:1]; [e setTexture:depth atIndex:2]; [e setBytes:&gp length:sizeof(gp) atIndex:1];
            encode_2d(e,g->base_glow,(NSUInteger)work_w,(NSUInteger)work_h);
        } else {
            [e setComputePipelineState:g->base]; [e setBuffer:src offset:0 atIndex:0]; [e setTexture:base atIndex:0]; [e setTexture:depth atIndex:1]; [e setBytes:&gp length:sizeof(gp) atIndex:1];
            encode_2d(e,g->base,(NSUInteger)work_w,(NSUInteger)work_h);
        }
        [e endEncoding];

        if(plan.glow){ id<MTLBlitCommandEncoder> bl=[cb blitCommandEncoder]; if(!bl){ rc=-3; break; } [bl generateMipmapsForTexture:glow]; [bl endEncoding]; }
        if(plan.diffusion) {
            if(plan.glow) {
                e=[cb computeCommandEncoder]; if(!e){ rc=-3; break; }
                [e setComputePipelineState:g->compose_in_place]; [e setTexture:base atIndex:0]; [e setTexture:glow atIndex:1]; [e setBytes:&gp length:sizeof(gp) atIndex:0];
                encode_2d(e,g->compose_in_place,(NSUInteger)work_w,(NSUInteger)work_h); [e endEncoding];
            }
            id<MTLBlitCommandEncoder> bl=[cb blitCommandEncoder]; if(!bl){ rc=-3; break; } [bl generateMipmapsForTexture:base]; [bl endEncoding];
            e=[cb computeCommandEncoder]; if(!e){ rc=-3; break; }
            [e setComputePipelineState:g->diffusion_out]; [e setTexture:base atIndex:0]; [e setBuffer:dst offset:0 atIndex:0]; [e setBytes:&gp length:sizeof(gp) atIndex:1];
            encode_2d(e,g->diffusion_out,(NSUInteger)ow,(NSUInteger)oh); [e endEncoding];
        } else {
            e=[cb computeCommandEncoder]; if(!e){ rc=-3; break; }
            [e setComputePipelineState:g->compose_out]; [e setTexture:base atIndex:0]; [e setTexture:(glow?glow:base) atIndex:1]; [e setBuffer:dst offset:0 atIndex:0]; [e setBytes:&gp length:sizeof(gp) atIndex:1];
            encode_2d(e,g->compose_out,(NSUInteger)ow,(NSUInteger)oh); [e endEncoding];
        }
        [cb commit];
    } while(false);

    if(depth_tmp)[depth_tmp release]; if(depth)[depth release];
    if(glow)[glow release]; if(base)[base release];
    [pool drain];
    return rc;
}

} // extern C
