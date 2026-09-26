#pragma once
#include "Params.h"

namespace stellar {

enum class Backend : unsigned char { CPU = 0, GPU = 1 };

struct RenderPlan {
    bool turbulence = false;
    bool grain = false;
    bool glow = false;
    bool diffusion = false;
    int glow_samples = 1;
    int max_mip_levels = 0; // 0 = full chain
    float glow_lod = 0.0f;
    float diffusion_max_lod = 0.0f;
};

RenderPlan make_render_plan(const Params& p, int width, int height, Backend backend);

} // namespace stellar
