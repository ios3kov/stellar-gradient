#pragma once
#include "StellarBridge.h"
#include "../../src/core/Params.h"
#include "../../src/core/RenderPlan.h"
#include "../../src/gpu/SharedParams.h"

namespace sgbridge {

stellar::Params to_cpp_params(const SGParamsC& c);
SGParamsC from_cpp_params(const stellar::Params& p);
stellar::gpu::ParamsGPU pack_gpu(const SGRenderStateC& s,
                                 int work_w, int work_h,
                                 int out_w, int out_h,
                                 int src_w, int src_h,
                                 int src_pitch_pixels, int dst_pitch_pixels,
                                 const stellar::RenderPlan& plan);

} // namespace sgbridge
