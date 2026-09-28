#pragma once
#include "StellarBridge.h"
#include <cstdint>
#include <limits>

namespace sgbridge {

// The current MSL kernels address BGRA128 via float4 elements and uint indices.
// This is a shader representation requirement, not an AE/CPU pointer-alignment
// guarantee. Reject unsupported pitches rather than silently truncating them.
struct MetalPlaneLayout {
    std::int32_t pitch_pixels = 0;
    std::uint64_t required_bytes = 0;
};
struct MetalRenderLayout {
    MetalPlaneLayout input, output;
    std::int32_t work_width = 0, work_height = 0;
};

inline bool metal_plane_layout(std::int32_t width, std::int32_t height,
                               std::int32_t rowbytes, MetalPlaneLayout& result) noexcept {
    constexpr std::int32_t pixel_bytes = 4 * sizeof(float);
    static_assert(pixel_bytes == 16, "BGRA128 requires 16 bytes per pixel");
    result = {};
    if (width <= 0 || height <= 0 || rowbytes <= 0 || rowbytes % pixel_bytes != 0)
        return false;
    const std::int32_t pitch = rowbytes / pixel_bytes;
    if (pitch < width) return false;
    // Widen before arithmetic. Kernels use uint32 pixel indexing, even on a
    // 64-bit host. Do not accept a plane whose last index would wrap in MSL.
    const auto last = static_cast<std::uint64_t>(height - 1) *
                      static_cast<std::uint64_t>(pitch) +
                      static_cast<std::uint64_t>(width - 1);
    if (last > std::numeric_limits<std::uint32_t>::max()) return false;
    result.pitch_pixels = pitch;
    // Padding after the last logical pixel is never accessed or required.
    result.required_bytes = (last + 1u) * static_cast<std::uint64_t>(pixel_bytes);
    return true;
}

inline bool metal_i32(std::int64_t value) noexcept {
    return value >= std::numeric_limits<std::int32_t>::min() &&
           value <= std::numeric_limits<std::int32_t>::max();
}

// Check the exact signed operations used by pack_gpu/load_bgra before they
// execute. Output crop must be inside the work texture; input may extend beyond
// it (the shader intentionally zero-fills outside the supplied input plane).
inline bool metal_axis_layout(std::int32_t work_begin, std::int32_t work_end,
                              std::int32_t input_begin, std::int32_t output_begin,
                              std::int32_t output_size, std::int32_t content_begin,
                              std::int32_t content_end) noexcept {
    const std::int64_t work = static_cast<std::int64_t>(work_end) - work_begin;
    const std::int64_t crop = static_cast<std::int64_t>(output_begin) - work_begin;
    const std::int64_t offset = static_cast<std::int64_t>(input_begin) - work_begin;
    const std::int64_t lo = static_cast<std::int64_t>(content_begin) - work_begin;
    const std::int64_t end = static_cast<std::int64_t>(content_end) - work_begin;
    const std::int64_t hi = end - 1;
    if (work <= 0 || !metal_i32(work) || output_size <= 0 || crop < 0 ||
        crop + output_size > work) return false;
    if (!metal_i32(offset) || !metal_i32(-offset) || !metal_i32(work - 1 - offset))
        return false;
    // end is checked as well as hi: the existing expression subtracts first,
    // then decrements. Empty semantic bounds are allowed, inverted bounds not.
    return metal_i32(lo) && metal_i32(end) && metal_i32(hi) &&
           hi - lo >= -1 && metal_i32(hi - lo) && metal_i32(hi - lo + 1) &&
           metal_i32(lo + hi);
}

inline bool metal_render_layout(const SGRenderStateC& state,
                                std::int32_t iw, std::int32_t ih, std::int32_t irb,
                                std::int32_t ow, std::int32_t oh, std::int32_t orb,
                                MetalRenderLayout& result) noexcept {
    result = {};
    MetalRenderLayout checked;
    if (!metal_plane_layout(iw, ih, irb, checked.input) ||
        !metal_plane_layout(ow, oh, orb, checked.output) ||
        !metal_axis_layout(state.work_rect.left, state.work_rect.right,
                           state.input_rect.left, state.output_rect.left, ow,
                           state.source_max_rect.left, state.source_max_rect.right) ||
        !metal_axis_layout(state.work_rect.top, state.work_rect.bottom,
                           state.input_rect.top, state.output_rect.top, oh,
                           state.source_max_rect.top, state.source_max_rect.bottom)) return false;
    // The widened checks above establish that these subtractions are safe.
    checked.work_width = state.work_rect.right - state.work_rect.left;
    checked.work_height = state.work_rect.bottom - state.work_rect.top;
    result = checked;
    return true;
}

inline bool metal_buffers_fit(const MetalRenderLayout& layout,
                              std::uint64_t input_bytes, std::uint64_t output_bytes) noexcept {
    return layout.input.required_bytes != 0 && layout.output.required_bytes != 0 &&
           input_bytes >= layout.input.required_bytes &&
           output_bytes >= layout.output.required_bytes;
}

} // namespace sgbridge
