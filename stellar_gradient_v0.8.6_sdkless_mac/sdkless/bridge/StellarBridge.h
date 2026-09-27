#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SGColor3C { float r, g, b; } SGColor3C;
typedef struct SGPoint2C { float x, y; } SGPoint2C;
typedef struct SGRectC { int32_t left, top, right, bottom; } SGRectC;

typedef struct SGParamsC {
    SGColor3C colors[5];
    float angle_deg;
    float cycles;
    float offset;
    float phase_deg;
    float saturation;
    float brightness;
    float depth_angle_deg;
    float depth_contrast;
    float bulge;
    float rounding;
    float turbulence_amount;
    float turbulence_size_x;
    float turbulence_size_y;
    float turbulence_evolution_deg;
    float depth_softness_px;
    float glow_radius_px;
    float glow_falloff;
    float glow_threshold;
    float glow_intensity;
    float glow_soft_clip;
    float grain_amount;
    float grain_size_px;
    float grain_color;
    uint32_t grain_animate;
    float diffusion_blur_px;
    SGPoint2C diffusion_center;
    float diffusion_focus_px;
    float diffusion_feather_px;
    uint32_t diffusion_invert;
    uint32_t quality; /* 0 Preview, 1 Auto, 2 Final */
} SGParamsC;

typedef struct SGRenderStateC {
    SGParamsC params;
    SGRectC input_rect;
    SGRectC source_max_rect;
    SGRectC output_rect;
    SGRectC work_rect;
    double time_seconds;
    uint32_t frame_index;
    int32_t engine_mode; /* 1 Auto, 2 GPU, 3 CPU */
} SGRenderStateC;

#ifdef __cplusplus
static_assert(sizeof(SGColor3C) == 12, "SGColor3C ABI changed");
static_assert(sizeof(SGPoint2C) == 8, "SGPoint2C ABI changed");
static_assert(sizeof(SGRectC) == 16, "SGRectC ABI changed");
static_assert(sizeof(SGParamsC) == 184, "SGParamsC ABI changed");
static_assert(alignof(SGParamsC) == 4, "SGParamsC alignment changed");
static_assert(sizeof(SGRenderStateC) == 264, "SGRenderStateC ABI changed");
static_assert(alignof(SGRenderStateC) == 8, "SGRenderStateC alignment changed");
static_assert(offsetof(SGParamsC, diffusion_center) == 160, "SGParamsC ABI changed");
static_assert(offsetof(SGParamsC, quality) == 180, "SGParamsC ABI changed");
static_assert(offsetof(SGRenderStateC, input_rect) == 184, "SGRenderStateC ABI changed");
static_assert(offsetof(SGRenderStateC, time_seconds) == 248, "SGRenderStateC ABI changed");
static_assert(offsetof(SGRenderStateC, engine_mode) == 260, "SGRenderStateC ABI changed");
#endif

/* Sanitizes values and applies AE downsample scaling to pixel-distance params. */
void sg_prepare_params(SGParamsC* params, float downsample_x, float downsample_y);

/* SmartFX dependency/output geometry. */
SGRectC sg_dependency_rect(const SGParamsC* params, SGRectC requested);
void sg_finalize_rects(const SGParamsC* params,
                       SGRectC requested,
                       SGRectC input_result,
                       SGRectC input_max,
                       SGRectC* output_rect,
                       SGRectC* work_rect,
                       SGRectC* semantic_max_rect);
int32_t sg_rect_empty(SGRectC r);

/* CPU render. AE bitdepth must be 8, 16, or 32. Rowbytes are signed. */
int32_t sg_cpu_render(const SGRenderStateC* state,
                      const void* input_data, int32_t input_width, int32_t input_height, int32_t input_rowbytes,
                      void* output_data, int32_t output_width, int32_t output_height, int32_t output_rowbytes,
                      int32_t bitdepth);

/* Metal is implemented without Adobe headers; AE supplies native MTLDevice/MTLCommandQueue/MTLBuffer pointers. */
void* sg_metal_create(void* mtl_device, int32_t* supports_f32_filtering);
void sg_metal_destroy(void* context);
int32_t sg_metal_render(void* context,
                        void* command_queue,
                        void* input_buffer,
                        void* output_buffer,
                        const SGRenderStateC* state,
                        int32_t input_width, int32_t input_height, int32_t input_rowbytes,
                        int32_t output_width, int32_t output_height, int32_t output_rowbytes);

#ifdef __cplusplus
}
#endif
