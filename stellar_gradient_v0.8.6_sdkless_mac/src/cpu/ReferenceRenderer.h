#pragma once
#include <cstdint>
#include "../core/Params.h"

namespace stellar {

struct ImageF32 {
    int width = 0;
    int height = 0;
    int stride_floats = 0;
    const float* src_rgba = nullptr;
    float* dst_rgba = nullptr;

    // Coordinate system used by the gradient/depth model. This is intentionally
    // explicit so CPU and GPU paths do not disagree when the source is placed
    // inside an expanded SmartFX output buffer. An invalid rect means full image.
    Bounds content_bounds{0, 0, -1, -1};

    // Layer-space origin of pixel (0,0) in this working buffer. SmartFX may
    // render tiles, so procedural coordinates must not restart at every tile.
    int origin_x = 0;
    int origin_y = 0;
};

void render_reference(const ImageF32& img, const Params& p, double time_seconds, std::uint32_t frame_index);

} // namespace stellar
