#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_EffectSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"
#include "../core/Params.h"
#include "../core/RenderPlan.h"
#include "../core/Sanitize.h"
#include "../cpu/ReferenceRenderer.h"
#include "../gpu/StellarGradientMetalSource.generated.h"
#include "../gpu/SharedParams.h"

#ifndef AE_OS_WIN
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#define SG_NAME "Stellar Gradient"
#define SG_MATCH_NAME "StellarLabs.StellarGradient"
#define SG_CATEGORY "Stellar"
#define SG_MAJOR_VERSION 0
#define SG_MINOR_VERSION 6
#define SG_BUG_VERSION 0
#define SG_STAGE_VERSION PF_Stage_BETA
#define SG_BUILD_VERSION 6

#define ERR(FUNC) do { if (!err) err = (FUNC); } while (0)
#define ERR2(FUNC) do { PF_Err _e2 = (FUNC); if (!err && _e2) err = _e2; } while (0)

using stellar::Params;

enum ParamIndex {
    P_INPUT = 0,
    P_PALETTE,
    P_COLOR1, P_COLOR2, P_COLOR3, P_COLOR4, P_COLOR5,
    P_ANGLE, P_CYCLES, P_OFFSET, P_PHASE, P_SATURATION, P_BRIGHTNESS,
    P_DEPTH_TOPIC, P_CONTRAST, P_BULGE, P_ROUNDING, P_DEPTH_END,
    P_TURB_TOPIC, P_TURB_AMOUNT, P_TURB_SIZE_X, P_TURB_SIZE_Y, P_TURB_EVOLUTION, P_TURB_SOFTNESS, P_TURB_END,
    P_LOOK_TOPIC,
    P_GLOW_TOPIC, P_GLOW_RADIUS, P_GLOW_FALLOFF, P_GLOW_THRESHOLD, P_GLOW_INTENSITY, P_GLOW_SOFTCLIP, P_GLOW_END,
    P_GRAIN_TOPIC, P_GRAIN_AMOUNT, P_GRAIN_SIZE, P_GRAIN_COLOR, P_GRAIN_ANIMATE, P_GRAIN_END,
    P_DIFF_TOPIC, P_DIFF_BLUR, P_DIFF_CENTER_X, P_DIFF_CENTER_Y, P_DIFF_FOCUS, P_DIFF_FEATHER, P_DIFF_INVERT, P_DIFF_END,
    P_LOOK_END,
    P_ENGINE,
    P_QUALITY,
    P_COUNT
};

// Persistent project IDs. Never renumber these, even if the UI order changes.
// Values intentionally match v0.5's historical ParamIndex values.
enum ParamDiskID {
    ID_PALETTE = 1,
    ID_COLOR1 = 2, ID_COLOR2 = 3, ID_COLOR3 = 4, ID_COLOR4 = 5, ID_COLOR5 = 6,
    ID_ANGLE = 7, ID_CYCLES = 8, ID_OFFSET = 9, ID_PHASE = 10, ID_SATURATION = 11, ID_BRIGHTNESS = 12,
    ID_DEPTH_TOPIC = 13, ID_CONTRAST = 14, ID_BULGE = 15, ID_ROUNDING = 16, ID_DEPTH_END = 17,
    ID_TURB_TOPIC = 18, ID_TURB_AMOUNT = 19, ID_TURB_SIZE_X = 20, ID_TURB_SIZE_Y = 21, ID_TURB_EVOLUTION = 22, ID_TURB_SOFTNESS = 23, ID_TURB_END = 24,
    ID_LOOK_TOPIC = 25,
    ID_GLOW_TOPIC = 26, ID_GLOW_RADIUS = 27, ID_GLOW_FALLOFF = 28, ID_GLOW_THRESHOLD = 29, ID_GLOW_INTENSITY = 30, ID_GLOW_SOFTCLIP = 31, ID_GLOW_END = 32,
    ID_GRAIN_TOPIC = 33, ID_GRAIN_AMOUNT = 34, ID_GRAIN_SIZE = 35, ID_GRAIN_COLOR = 36, ID_GRAIN_ANIMATE = 37, ID_GRAIN_END = 38,
    ID_DIFF_TOPIC = 39, ID_DIFF_BLUR = 40, ID_DIFF_CENTER_X = 41, ID_DIFF_CENTER_Y = 42, ID_DIFF_FOCUS = 43, ID_DIFF_FEATHER = 44, ID_DIFF_INVERT = 45, ID_DIFF_END = 46,
    ID_LOOK_END = 47, ID_ENGINE = 48, ID_QUALITY = 49
};

struct SGRenderState {
    Params p;
    PF_LRect input_rect{};
    PF_LRect source_max_rect{};
    PF_LRect output_rect{};
    PF_LRect work_rect{};
    float downsample_x = 1.0f;
    float downsample_y = 1.0f;
    double time_seconds = 0.0;
    std::uint32_t frame_index = 0;
    A_long engine_mode = 1; // 1 Auto, 2 GPU, 3 CPU
};

using SGParamsGPU = stellar::gpu::ParamsGPU;

#ifndef AE_OS_WIN
struct MetalGPUData {
    id<MTLComputePipelineState> base;
    id<MTLComputePipelineState> base_out;
    id<MTLComputePipelineState> base_glow;
    id<MTLComputePipelineState> compose_in_place;
    id<MTLComputePipelineState> compose_out;
    id<MTLComputePipelineState> diffusion_out;
    bool supports_f32_filtering;
};
#endif

static void DisposePreRenderData(void* p) { delete static_cast<SGRenderState*>(p); }

static PF_Err CheckoutParam(PF_InData* in, PF_ParamIndex idx, PF_ParamDef* def) {
    return PF_CHECKOUT_PARAM(in, idx, in->current_time, in->time_step, in->time_scale, def);
}
static PF_Err CheckinParam(PF_InData* in, PF_ParamDef* def) {
    return (*(in)->inter.checkin_param)((in)->effect_ref, def);
}
static PF_Err GetFloat(PF_InData* in, PF_ParamIndex idx, float* v) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE; PF_ParamDef d; AEFX_CLR_STRUCT(d);
    err=CheckoutParam(in,idx,&d);
    if(!err){ *v=float(d.u.fs_d.value); err2=CheckinParam(in,&d); }
    return err ? err : err2;
}
static PF_Err GetPopup(PF_InData* in, PF_ParamIndex idx, A_long* v) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE; PF_ParamDef d; AEFX_CLR_STRUCT(d);
    err=CheckoutParam(in,idx,&d);
    if(!err){ *v=d.u.pd.value; err2=CheckinParam(in,&d); }
    return err ? err : err2;
}
static PF_Err GetBool(PF_InData* in, PF_ParamIndex idx, bool* v) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE; PF_ParamDef d; AEFX_CLR_STRUCT(d);
    err=CheckoutParam(in,idx,&d);
    if(!err){ *v=d.u.bd.value!=0; err2=CheckinParam(in,&d); }
    return err ? err : err2;
}
static PF_Err GetColor(PF_InData* in, PF_OutData* out, PF_ParamIndex idx, stellar::Color3f* c) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE; PF_ParamDef d; AEFX_CLR_STRUCT(d);
    err=CheckoutParam(in,idx,&d);
    if(!err){
        PF_PixelFloat fp{};
        AEFX_SuiteScoper<PF_ColorParamSuite1> colorSuite(in,kPFColorParamSuite,kPFColorParamSuiteVersion1,out);
        err=colorSuite->PF_GetFloatingPointColorFromColorDef(in->effect_ref,&d,&fp);
        if(!err){ c->r=fp.red; c->g=fp.green; c->b=fp.blue; }
        err2=CheckinParam(in,&d);
    }
    return err ? err : err2;
}

static void ApplyPalette(A_long palette, Params& p) {
    using C=stellar::Color3f;
    static const C presets[10][5] = {
        {{1.00f,.13f,.24f},{1.00f,.42f,.10f},{1.00f,.89f,.16f},{.24f,.95f,.74f},{.14f,.37f,1.00f}},
        {{.07f,.15f,.08f},{.18f,.38f,.17f},{.42f,.62f,.20f},{.78f,.79f,.36f},{.93f,.88f,.65f}},
        {{.19f,.08f,.15f},{.46f,.18f,.31f},{.73f,.35f,.48f},{.92f,.57f,.62f},{1.00f,.80f,.73f}},
        {{.015f,.012f,.04f},{.07f,.04f,.16f},{.18f,.07f,.31f},{.36f,.12f,.51f},{.68f,.25f,.76f}},
        {{.02f,.11f,.24f},{.00f,.46f,.82f},{.00f,.92f,.93f},{.52f,1.00f,.46f},{1.00f,.91f,.09f}},
        {{.05f,.01f,.14f},{.28f,.03f,.53f},{.63f,.06f,.89f},{.96f,.20f,.78f},{.36f,.69f,1.00f}},
        {{.00f,.16f,.24f},{.00f,.47f,.61f},{.00f,.78f,.77f},{.37f,.96f,.78f},{.80f,1.00f,.92f}},
        {{.22f,.04f,.01f},{.59f,.13f,.02f},{.92f,.36f,.04f},{1.00f,.66f,.12f},{1.00f,.91f,.48f}},
        {{1.00f,.10f,.27f},{1.00f,.53f,.10f},{.98f,.92f,.12f},{.10f,.83f,.55f},{.08f,.43f,1.00f}},
        {{.42f,.18f,.95f},{.82f,.30f,1.00f},{1.00f,.43f,.73f},{1.00f,.66f,.44f},{.45f,.91f,1.00f}}
    };
    if(palette>=2 && palette<=11) for(int i=0;i<5;++i) p.colors[i]=presets[palette-2][i];
}

static PF_Err GatherParams(PF_InData* in, PF_OutData* out, SGRenderState* s) {
    PF_Err err=PF_Err_NONE; A_long popup=1, quality=2;
    ERR(GetPopup(in,P_PALETTE,&popup));
    ERR(GetColor(in,out,P_COLOR1,&s->p.colors[0])); ERR(GetColor(in,out,P_COLOR2,&s->p.colors[1])); ERR(GetColor(in,out,P_COLOR3,&s->p.colors[2])); ERR(GetColor(in,out,P_COLOR4,&s->p.colors[3])); ERR(GetColor(in,out,P_COLOR5,&s->p.colors[4]));
    ApplyPalette(popup,s->p);
    ERR(GetFloat(in,P_ANGLE,&s->p.angle_deg)); ERR(GetFloat(in,P_CYCLES,&s->p.cycles)); ERR(GetFloat(in,P_OFFSET,&s->p.offset)); ERR(GetFloat(in,P_PHASE,&s->p.phase_deg)); ERR(GetFloat(in,P_SATURATION,&s->p.saturation)); ERR(GetFloat(in,P_BRIGHTNESS,&s->p.brightness));
    ERR(GetFloat(in,P_CONTRAST,&s->p.depth_contrast)); ERR(GetFloat(in,P_BULGE,&s->p.bulge)); ERR(GetFloat(in,P_ROUNDING,&s->p.rounding));
    ERR(GetFloat(in,P_TURB_AMOUNT,&s->p.turbulence_amount)); ERR(GetFloat(in,P_TURB_SIZE_X,&s->p.turbulence_size_x)); ERR(GetFloat(in,P_TURB_SIZE_Y,&s->p.turbulence_size_y)); ERR(GetFloat(in,P_TURB_EVOLUTION,&s->p.turbulence_evolution)); ERR(GetFloat(in,P_TURB_SOFTNESS,&s->p.turbulence_softness));
    ERR(GetFloat(in,P_GLOW_RADIUS,&s->p.glow_radius_px)); ERR(GetFloat(in,P_GLOW_FALLOFF,&s->p.glow_falloff)); ERR(GetFloat(in,P_GLOW_THRESHOLD,&s->p.glow_threshold)); ERR(GetFloat(in,P_GLOW_INTENSITY,&s->p.glow_intensity)); ERR(GetFloat(in,P_GLOW_SOFTCLIP,&s->p.glow_soft_clip));
    ERR(GetFloat(in,P_GRAIN_AMOUNT,&s->p.grain_amount)); ERR(GetFloat(in,P_GRAIN_SIZE,&s->p.grain_size_px)); ERR(GetFloat(in,P_GRAIN_COLOR,&s->p.grain_color)); ERR(GetBool(in,P_GRAIN_ANIMATE,&s->p.grain_animate));
    ERR(GetFloat(in,P_DIFF_BLUR,&s->p.diffusion_blur_px)); float cx=50,cy=50; ERR(GetFloat(in,P_DIFF_CENTER_X,&cx)); ERR(GetFloat(in,P_DIFF_CENTER_Y,&cy)); s->p.diffusion_center={cx*.01f,cy*.01f}; ERR(GetFloat(in,P_DIFF_FOCUS,&s->p.diffusion_focus_px)); ERR(GetFloat(in,P_DIFF_FEATHER,&s->p.diffusion_feather_px)); ERR(GetBool(in,P_DIFF_INVERT,&s->p.diffusion_invert));
    ERR(GetPopup(in,P_ENGINE,&s->engine_mode));
    ERR(GetPopup(in,P_QUALITY,&quality)); s->p.quality = quality<=1?stellar::Quality::Preview:(quality==2?stellar::Quality::Auto:stellar::Quality::Final);
    if(!err) s->p = stellar::sanitized_params(s->p);
    return err;
}

static PF_Err About(PF_InData*, PF_OutData* out) {
    PF_SPRINTF(out->return_msg, "%s v%d.%d\rMetal + CPU SmartFX", SG_NAME, SG_MAJOR_VERSION, SG_MINOR_VERSION); return PF_Err_NONE;
}
static PF_Err GlobalSetup(PF_InData*, PF_OutData* out) {
    out->my_version=PF_VERSION(SG_MAJOR_VERSION,SG_MINOR_VERSION,SG_BUG_VERSION,SG_STAGE_VERSION,SG_BUILD_VERSION);
    // NON_PARAM_VARY is required because animated grain depends on current time.
    // QUERY_DYNAMIC_FLAGS clears it when animated grain is not active so AE can cache stills.
    out->out_flags=PF_OutFlag_DEEP_COLOR_AWARE|PF_OutFlag_NON_PARAM_VARY;
    out->out_flags2=PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS|PF_OutFlag2_SUPPORTS_SMART_RENDER|PF_OutFlag2_FLOAT_COLOR_AWARE|PF_OutFlag2_SUPPORTS_THREADED_RENDERING|PF_OutFlag2_SUPPORTS_GPU_RENDER_F32|PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG|PF_OutFlag2_REVEALS_ZERO_ALPHA;
    return PF_Err_NONE;
}

static PF_Err QueryDynamicFlags(PF_InData* in, PF_OutData* out) {
    if(!in||!out) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    PF_Err err=PF_Err_NONE;
    bool animate=false;
    float amount=0.0f;
    ERR(GetBool(in,P_GRAIN_ANIMATE,&animate));
    ERR(GetFloat(in,P_GRAIN_AMOUNT,&amount));
    if(err) return err;
    if(animate && amount>1.0e-6f) out->out_flags|=PF_OutFlag_NON_PARAM_VARY;
    else out->out_flags&=~PF_OutFlag_NON_PARAM_VARY;
    return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out) {
    PF_ParamDef def; AEFX_CLR_STRUCT(def);

    PF_ADD_POPUP("Palette",11,1,"Custom|Hot Neon|Moss|Rose Dust|Void|Voltage|UV Bloom|Aqua|Golden Hour|Spectrum|Sugar",ID_PALETTE);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Color 1",31,13,87,ID_COLOR1);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Color 2",31,97,242,ID_COLOR2);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Color 3",170,43,242,ID_COLOR3);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Color 4",255,79,140,ID_COLOR4);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Color 5",255,191,46,ID_COLOR5);

    PF_ADD_FLOAT_SLIDERX("Angle",-720,720,-180,180,90,PF_Precision_TENTHS,0,0,ID_ANGLE);
    PF_ADD_FLOAT_SLIDERX("Cycles",.1,20,.1,5,1,PF_Precision_HUNDREDTHS,0,0,ID_CYCLES);
    PF_ADD_FLOAT_SLIDERX("Offset",-100,100,-2,2,0,PF_Precision_HUNDREDTHS,0,0,ID_OFFSET);
    PF_ADD_FLOAT_SLIDERX("Phase",-100000,100000,0,360,0,PF_Precision_TENTHS,0,0,ID_PHASE);
    PF_ADD_FLOAT_SLIDERX("Saturation",0,2,0,2,1,PF_Precision_HUNDREDTHS,0,0,ID_SATURATION);
    PF_ADD_FLOAT_SLIDERX("Brightness",0,4,0,2,1,PF_Precision_HUNDREDTHS,0,0,ID_BRIGHTNESS);

    PF_ADD_TOPICX("Depth",PF_ParamFlag_START_COLLAPSED,ID_DEPTH_TOPIC);
    PF_ADD_FLOAT_SLIDERX("Contrast",.05,8,.05,4,1,PF_Precision_HUNDREDTHS,0,0,ID_CONTRAST);
    PF_ADD_FLOAT_SLIDERX("Bulge",-2,2,-1,1,0,PF_Precision_HUNDREDTHS,0,0,ID_BULGE);
    PF_ADD_FLOAT_SLIDERX("Rounding",0,1,0,1,0,PF_Precision_HUNDREDTHS,0,0,ID_ROUNDING);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_DEPTH_END);

    PF_ADD_TOPICX("Turbulence",PF_ParamFlag_START_COLLAPSED,ID_TURB_TOPIC);
    PF_ADD_FLOAT_SLIDERX("Amount",0,2,0,1,0,PF_Precision_HUNDREDTHS,0,0,ID_TURB_AMOUNT);
    PF_ADD_FLOAT_SLIDERX("Size X",1,2000,1,500,120,PF_Precision_TENTHS,0,0,ID_TURB_SIZE_X);
    PF_ADD_FLOAT_SLIDERX("Size Y",1,2000,1,500,120,PF_Precision_TENTHS,0,0,ID_TURB_SIZE_Y);
    PF_ADD_FLOAT_SLIDERX("Evolution",-100000,100000,-360,360,0,PF_Precision_HUNDREDTHS,0,0,ID_TURB_EVOLUTION);
    PF_ADD_FLOAT_SLIDERX("Softness",0,1,0,1,.5,PF_Precision_HUNDREDTHS,0,0,ID_TURB_SOFTNESS);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_TURB_END);

    AEFX_CLR_STRUCT(def); PF_ADD_TOPIC("Look",ID_LOOK_TOPIC);
    PF_ADD_TOPICX("Glow",PF_ParamFlag_START_COLLAPSED,ID_GLOW_TOPIC);
    PF_ADD_FLOAT_SLIDERX("Radius",0,2000,0,500,60,PF_Precision_TENTHS,0,0,ID_GLOW_RADIUS);
    PF_ADD_FLOAT_SLIDERX("Falloff",.25,4,.25,4,1.6,PF_Precision_HUNDREDTHS,0,0,ID_GLOW_FALLOFF);
    PF_ADD_FLOAT_SLIDERX("Threshold",0,1,0,1,.2,PF_Precision_HUNDREDTHS,0,0,ID_GLOW_THRESHOLD);
    PF_ADD_FLOAT_SLIDERX("Intensity",0,10,0,3,.8,PF_Precision_HUNDREDTHS,0,0,ID_GLOW_INTENSITY);
    PF_ADD_FLOAT_SLIDERX("Soft Clip",0,1,0,1,.25,PF_Precision_HUNDREDTHS,0,0,ID_GLOW_SOFTCLIP);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_GLOW_END);

    PF_ADD_TOPICX("Grain",PF_ParamFlag_START_COLLAPSED,ID_GRAIN_TOPIC);
    PF_ADD_FLOAT_SLIDERX("Amount",0,.5,0,.15,.03,PF_Precision_THOUSANDTHS,0,0,ID_GRAIN_AMOUNT);
    PF_ADD_FLOAT_SLIDERX("Size",.5,16,.5,8,1,PF_Precision_HUNDREDTHS,0,0,ID_GRAIN_SIZE);
    PF_ADD_FLOAT_SLIDERX("Color",0,1,0,1,0,PF_Precision_HUNDREDTHS,0,0,ID_GRAIN_COLOR);
    AEFX_CLR_STRUCT(def); PF_ADD_CHECKBOX("Animate","",TRUE,0,ID_GRAIN_ANIMATE);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_GRAIN_END);

    PF_ADD_TOPICX("Optical Diffusion",PF_ParamFlag_START_COLLAPSED,ID_DIFF_TOPIC);
    PF_ADD_FLOAT_SLIDERX("Blur",0,2000,0,500,0,PF_Precision_TENTHS,0,0,ID_DIFF_BLUR);
    PF_ADD_FLOAT_SLIDERX("Center X",0,100,0,100,50,PF_Precision_TENTHS,0,0,ID_DIFF_CENTER_X);
    PF_ADD_FLOAT_SLIDERX("Center Y",0,100,0,100,50,PF_Precision_TENTHS,0,0,ID_DIFF_CENTER_Y);
    PF_ADD_FLOAT_SLIDERX("Focus",0,4000,0,1000,120,PF_Precision_TENTHS,0,0,ID_DIFF_FOCUS);
    PF_ADD_FLOAT_SLIDERX("Feather",0,4000,0,1000,200,PF_Precision_TENTHS,0,0,ID_DIFF_FEATHER);
    AEFX_CLR_STRUCT(def); PF_ADD_CHECKBOX("Invert","",FALSE,0,ID_DIFF_INVERT);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_DIFF_END);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(ID_LOOK_END);

    AEFX_CLR_STRUCT(def); PF_ADD_POPUP("Render Engine",3,1,"Auto|GPU|CPU",ID_ENGINE);
    AEFX_CLR_STRUCT(def); PF_ADD_POPUP("Quality",3,2,"Preview|Auto|Final",ID_QUALITY);
    out->num_params=P_COUNT;
    return PF_Err_NONE;
}

static bool RectEmpty(const PF_LRect& r) {
    return r.right <= r.left || r.bottom <= r.top;
}

static PF_LRect ExpandRect(PF_LRect r, A_long margin) {
    if (RectEmpty(r) || margin <= 0) return r;
    r.left -= margin; r.top -= margin; r.right += margin; r.bottom += margin;
    return r;
}

static PF_LRect IntersectRect(const PF_LRect& a, const PF_LRect& b) {
    PF_LRect r{};
    r.left = std::max(a.left, b.left);
    r.top = std::max(a.top, b.top);
    r.right = std::min(a.right, b.right);
    r.bottom = std::min(a.bottom, b.bottom);
    if (RectEmpty(r)) r = PF_LRect{};
    return r;
}

static A_long FloorToMultiple(A_long v, A_long q) {
    if (q <= 1) return v;
    const A_long rem = v % q;
    return rem >= 0 ? v - rem : v - (rem + q);
}

static A_long CeilToMultiple(A_long v, A_long q) {
    if (q <= 1) return v;
    const A_long f = FloorToMultiple(v, q);
    return f == v ? v : f + q;
}

static PF_LRect AlignRect(PF_LRect r, A_long quantum) {
    if (RectEmpty(r) || quantum <= 1) return r;
    r.left = FloorToMultiple(r.left, quantum);
    r.top = FloorToMultiple(r.top, quantum);
    r.right = CeilToMultiple(r.right, quantum);
    r.bottom = CeilToMultiple(r.bottom, quantum);
    return r;
}

static A_long ComputeMipAlignment(const Params& p) {
    float max_lod = 0.0f;
    if (p.glow_intensity > 1.0e-6f && p.glow_radius_px > 0.5f) {
        const int samples = p.quality == stellar::Quality::Preview ? 1 : (p.quality == stellar::Quality::Auto ? 3 : 5);
        const float spread_lod = samples > 1 ? std::max(0.35f, 0.45f * p.glow_falloff) : 0.0f;
        max_lod = std::max(max_lod, std::log2(std::max(1.0f, p.glow_radius_px)) + spread_lod);
    }
    if (p.diffusion_blur_px > 0.5f) {
        max_lod = std::max(max_lod, std::log2(std::max(1.0f, p.diffusion_blur_px)));
    }
    int exponent = static_cast<int>(std::ceil(std::max(0.0f, max_lod)));
    if (p.quality == stellar::Quality::Preview) exponent = std::min(exponent, 7);
    else if (p.quality == stellar::Quality::Auto) exponent = std::min(exponent, 9);
    else exponent = std::min(exponent, 12);
    return static_cast<A_long>(1u << static_cast<unsigned>(std::max(0, exponent)));
}

static A_long ComputeWorkMargin(const Params& p) {
    float reach = 0.0f;
    if (p.glow_intensity > 1.0e-6f && p.glow_radius_px > 0.5f) {
        int samples = p.quality == stellar::Quality::Preview ? 1 : (p.quality == stellar::Quality::Auto ? 3 : 5);
        const float spread_lod = samples > 1 ? std::max(0.35f, 0.45f * p.glow_falloff) : 0.0f;
        // Highest sampled mip controls the support footprint. Keep a small
        // guard band for bilinear sampling and the box footprint of mip texels.
        reach = std::max(reach, p.glow_radius_px * std::exp2(spread_lod) + 3.0f);
    }
    if (p.diffusion_blur_px > 0.5f) {
        reach = std::max(reach, p.diffusion_blur_px + 3.0f);
    }
    return static_cast<A_long>(std::ceil(std::max(0.0f, reach)));
}

static PF_Err PreRender(PF_InData* in, PF_OutData* out, PF_PreRenderExtra* extra) {
    if(!extra||!extra->input||!extra->output) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    PF_Err err=PF_Err_NONE;
    auto* state=new(std::nothrow) SGRenderState();
    if(!state) return PF_Err_OUT_OF_MEMORY;
    err=GatherParams(in,out,state);
    if(err){ delete state; return err; }

    state->downsample_x = in->downsample_x.den ? float(in->downsample_x.num)/float(in->downsample_x.den) : 1.0f;
    state->downsample_y = in->downsample_y.den ? float(in->downsample_y.num)/float(in->downsample_y.den) : 1.0f;
    if(!std::isfinite(state->downsample_x) || state->downsample_x <= 0.0f) state->downsample_x = 1.0f;
    if(!std::isfinite(state->downsample_y) || state->downsample_y <= 0.0f) state->downsample_y = 1.0f;
    const float pixel_scale = std::sqrt(std::max(0.0f, state->downsample_x * state->downsample_y));
    state->p.glow_radius_px*=pixel_scale;
    state->p.diffusion_blur_px*=pixel_scale;
    state->p.diffusion_focus_px*=pixel_scale;
    state->p.diffusion_feather_px*=pixel_scale;
    state->p = stellar::sanitized_params(state->p);
    state->time_seconds = in->time_scale ? double(in->current_time)/double(in->time_scale) : 0.0;
    state->frame_index = in->time_step ? std::uint32_t(in->current_time/in->time_step) : 0;

    const A_long work_margin = ComputeWorkMargin(state->p);
    const A_long mip_alignment = ComputeMipAlignment(state->p);
    const PF_LRect requested = extra->input->output_request.rect;

    // Request exactly the neighborhood needed to compute the requested output.
    // The output itself stays clipped to the host request; the larger working
    // area lives only inside the effect, avoiding duplicated SmartFX tiles.
    PF_RenderRequest dependency = extra->input->output_request;
    dependency.rect = AlignRect(ExpandRect(dependency.rect, work_margin), mip_alignment);

    PF_CheckoutResult cr; AEFX_CLR_STRUCT(cr);
    err=extra->cb->checkout_layer(in->effect_ref,P_INPUT,P_INPUT,&dependency,in->current_time,in->time_step,in->time_scale,&cr);
    if(err){ delete state; return err; }

    state->input_rect=cr.result_rect;
    state->source_max_rect=cr.max_result_rect;

    const PF_LRect semantic_max = ExpandRect(cr.max_result_rect, work_margin);
    state->output_rect = IntersectRect(requested, semantic_max);
    state->work_rect = AlignRect(ExpandRect(state->output_rect, work_margin), mip_alignment);

    extra->output->result_rect = state->output_rect;
    extra->output->max_result_rect = semantic_max;
#ifndef AE_OS_WIN
    if(state->engine_mode != 3 && extra->input->what_gpu==PF_GPU_Framework_METAL && extra->input->gpu_data) {
        bool gpu_ok=true;
        PF_Handle gh=(PF_Handle)extra->input->gpu_data;
        if(!gh||!*gh) gpu_ok=false;
        else {
            const auto* g=(const MetalGPUData*)*gh;
            if(state->p.quality!=stellar::Quality::Preview && !g->supports_f32_filtering) gpu_ok=false;
        }
        if(gpu_ok) extra->output->flags|=PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;
    }
#endif
    extra->output->pre_render_data=state;
    extra->output->delete_pre_render_data_func=DisposePreRenderData;
    return PF_Err_NONE;
}

static void CopyWorldToCanvas(PF_EffectWorld* w, PF_PixelFormat fmt, std::vector<float>& canvas,
                              int canvas_w, int canvas_h, int offx, int offy) {
    canvas.assign(static_cast<std::size_t>(canvas_w) * static_cast<std::size_t>(canvas_h) * 4u, 0.0f);
    for (A_long sy = 0; sy < w->height; ++sy) {
        const int dy = static_cast<int>(sy) + offy;
        if (dy < 0 || dy >= canvas_h) continue;
        const char* row = static_cast<const char*>(w->data) + static_cast<std::size_t>(sy) * static_cast<std::size_t>(w->rowbytes);
        for (A_long sx = 0; sx < w->width; ++sx) {
            const int dx = static_cast<int>(sx) + offx;
            if (dx < 0 || dx >= canvas_w) continue;
            const std::size_t i = (static_cast<std::size_t>(dy) * static_cast<std::size_t>(canvas_w) + static_cast<std::size_t>(dx)) * 4u;
            if (fmt == PF_PixelFormat_ARGB32) {
                const PF_Pixel& q = reinterpret_cast<const PF_Pixel*>(row)[sx];
                canvas[i + 0] = q.red / 255.0f; canvas[i + 1] = q.green / 255.0f; canvas[i + 2] = q.blue / 255.0f; canvas[i + 3] = q.alpha / 255.0f;
            } else if (fmt == PF_PixelFormat_ARGB64) {
                const PF_Pixel16& q = reinterpret_cast<const PF_Pixel16*>(row)[sx];
                canvas[i + 0] = q.red / float(PF_MAX_CHAN16); canvas[i + 1] = q.green / float(PF_MAX_CHAN16); canvas[i + 2] = q.blue / float(PF_MAX_CHAN16); canvas[i + 3] = q.alpha / float(PF_MAX_CHAN16);
            } else {
                const PF_PixelFloat& q = reinterpret_cast<const PF_PixelFloat*>(row)[sx];
                canvas[i + 0] = q.red; canvas[i + 1] = q.green; canvas[i + 2] = q.blue; canvas[i + 3] = q.alpha;
            }
        }
    }
}

static void FromFloatRGBARegion(const std::vector<float>& src, int src_w, int crop_x, int crop_y,
                                PF_EffectWorld* w, PF_PixelFormat fmt) {
    for (A_long y = 0; y < w->height; ++y) {
        char* row = static_cast<char*>(w->data) + static_cast<std::size_t>(y) * static_cast<std::size_t>(w->rowbytes);
        for (A_long x = 0; x < w->width; ++x) {
            const int sx = crop_x + static_cast<int>(x);
            const int sy = crop_y + static_cast<int>(y);
            const std::size_t i = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(src_w) + static_cast<std::size_t>(sx)) * 4u;
            if (fmt == PF_PixelFormat_ARGB32) {
                auto& q = reinterpret_cast<PF_Pixel*>(row)[x];
                q.red=A_u_char(std::lround(std::clamp(src[i],0.f,1.f)*255)); q.green=A_u_char(std::lround(std::clamp(src[i+1],0.f,1.f)*255)); q.blue=A_u_char(std::lround(std::clamp(src[i+2],0.f,1.f)*255)); q.alpha=A_u_char(std::lround(std::clamp(src[i+3],0.f,1.f)*255));
            } else if (fmt == PF_PixelFormat_ARGB64) {
                auto& q = reinterpret_cast<PF_Pixel16*>(row)[x];
                q.red=A_u_short(std::lround(std::clamp(src[i],0.f,1.f)*PF_MAX_CHAN16)); q.green=A_u_short(std::lround(std::clamp(src[i+1],0.f,1.f)*PF_MAX_CHAN16)); q.blue=A_u_short(std::lround(std::clamp(src[i+2],0.f,1.f)*PF_MAX_CHAN16)); q.alpha=A_u_short(std::lround(std::clamp(src[i+3],0.f,1.f)*PF_MAX_CHAN16));
            } else {
                auto& q = reinterpret_cast<PF_PixelFloat*>(row)[x];
                q.red=src[i]; q.green=src[i+1]; q.blue=src[i+2]; q.alpha=std::clamp(src[i+3],0.f,1.f);
            }
        }
    }
}

static PF_Err RenderCPU(PF_InData* in, PF_OutData* out, PF_SmartRenderExtra* extra, SGRenderState* state) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE;
    PF_EffectWorld *input=nullptr,*output=nullptr;
    bool checked_input=false;
    err=extra->cb->checkout_layer_pixels(in->effect_ref,P_INPUT,&input);
    if(!err && input) checked_input=true;
    if(!err) err=extra->cb->checkout_output(in->effect_ref,&output);
    if(err||!input||!output){
        if(checked_input) err2=extra->cb->checkin_layer_pixels(in->effect_ref,P_INPUT);
        return err ? err : (err2 ? err2 : PF_Err_INTERNAL_STRUCT_DAMAGED);
    }

    AEFX_SuiteScoper<PF_WorldSuite2> ws(in,kPFWorldSuite,kPFWorldSuiteVersion2,out);
    PF_PixelFormat inf=PF_PixelFormat_INVALID,outf=PF_PixelFormat_INVALID;
    err=ws->PF_GetPixelFormat(input,&inf);
    if(!err) err=ws->PF_GetPixelFormat(output,&outf);
    if(!err && inf!=outf) err=PF_Err_UNRECOGNIZED_PARAM_TYPE;
    if(!err && inf!=PF_PixelFormat_ARGB32 && inf!=PF_PixelFormat_ARGB64 && inf!=PF_PixelFormat_ARGB128) err=PF_Err_UNRECOGNIZED_PARAM_TYPE;

    if(!err){
        const int work_w=static_cast<int>(state->work_rect.right-state->work_rect.left);
        const int work_h=static_cast<int>(state->work_rect.bottom-state->work_rect.top);
        const int crop_x=static_cast<int>(state->output_rect.left-state->work_rect.left);
        const int crop_y=static_cast<int>(state->output_rect.top-state->work_rect.top);
        if(work_w<=0||work_h<=0||crop_x<0||crop_y<0||crop_x+output->width>work_w||crop_y+output->height>work_h){
            err=PF_Err_INTERNAL_STRUCT_DAMAGED;
        } else {
            const int input_offx=static_cast<int>(state->input_rect.left-state->work_rect.left);
            const int input_offy=static_cast<int>(state->input_rect.top-state->work_rect.top);
            std::vector<float> canvas;
            CopyWorldToCanvas(input,inf,canvas,work_w,work_h,input_offx,input_offy);
            const stellar::Bounds content{
                static_cast<int>(state->source_max_rect.left-state->work_rect.left),
                static_cast<int>(state->source_max_rect.top-state->work_rect.top),
                static_cast<int>(state->source_max_rect.right-state->work_rect.left)-1,
                static_cast<int>(state->source_max_rect.bottom-state->work_rect.top)-1
            };
            stellar::ImageF32 img{work_w,work_h,work_w*4,canvas.data(),canvas.data(),content,
                                  static_cast<int>(state->work_rect.left),static_cast<int>(state->work_rect.top)};
            stellar::render_reference(img,state->p,state->time_seconds,state->frame_index);
            FromFloatRGBARegion(canvas,work_w,crop_x,crop_y,output,outf);
        }
    }

    if(checked_input) err2=extra->cb->checkin_layer_pixels(in->effect_ref,P_INPUT);
    return err ? err : err2;
}

#ifndef AE_OS_WIN
static id<MTLComputePipelineState> MakePipeline(id<MTLDevice> d,id<MTLLibrary> l,const char* name,NSError** e){NSString* n=[NSString stringWithUTF8String:name];id<MTLFunction> f=[l newFunctionWithName:n];if(!f)return nil;id<MTLComputePipelineState> p=[d newComputePipelineStateWithFunction:f error:e];[f release];return p;}
static PF_Err GPUDeviceSetup(PF_InData* in, PF_OutData* out, PF_GPUDeviceSetupExtra* extra) {
    if(!extra||!extra->input||!extra->output) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    // Device setup is the clean fallback boundary: unsupported/broken GPU
    // setup simply declines GPU support so AE can call the CPU SmartRender.
    out->out_flags2 &= ~PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    extra->output->gpu_data = nullptr;
    if(extra->input->what_gpu!=PF_GPU_Framework_METAL) return PF_Err_NONE;

    NSAutoreleasePool* pool=[[NSAutoreleasePool alloc] init];
    PF_Err err=PF_Err_NONE;
    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gs(in,kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1,out);
    PF_GPUDeviceInfo di; AEFX_CLR_STRUCT(di);
    err=gs->GetDeviceInfo(in->effect_ref,extra->input->device_index,&di);
    if(err){ [pool drain]; return PF_Err_NONE; }
    if(!di.devicePV){ [pool drain]; return PF_Err_NONE; }

    AEFX_SuiteScoper<PF_HandleSuite1> hs(in,kPFHandleSuite,kPFHandleSuiteVersion1,out);
    PF_Handle h=hs->host_new_handle(sizeof(MetalGPUData));
    if(!h){ [pool drain]; return PF_Err_NONE; }
    auto* g=(MetalGPUData*)*h;
    std::memset(g,0,sizeof(*g));

    id<MTLDevice> dev=(id<MTLDevice>)di.devicePV;
    g->supports_f32_filtering = dev.supports32BitFloatFiltering;
    NSError* e=nil;
    NSString* src=[NSString stringWithUTF8String:kStellarGradientMetalSource];
    MTLCompileOptions* compileOptions=[[MTLCompileOptions alloc] init];
    compileOptions.fastMathEnabled=NO;
    id<MTLLibrary> lib=[dev newLibraryWithSource:src options:compileOptions error:&e];
    [compileOptions release];
    if(!lib){ hs->host_dispose_handle(h); [pool drain]; return PF_Err_NONE; }

    g->base=MakePipeline(dev,lib,"SGBaseKernel",&e);
    g->base_out=MakePipeline(dev,lib,"SGBaseOutKernel",&e);
    g->base_glow=MakePipeline(dev,lib,"SGBaseGlowKernel",&e);
    g->compose_in_place=MakePipeline(dev,lib,"SGComposeInPlaceKernel",&e);
    g->compose_out=MakePipeline(dev,lib,"SGComposeOutKernel",&e);
    g->diffusion_out=MakePipeline(dev,lib,"SGDiffusionOutKernel",&e);
    [lib release];

    if(!g->base||!g->base_out||!g->base_glow||!g->compose_in_place||!g->compose_out||!g->diffusion_out){
        if(g->base)[g->base release]; if(g->base_out)[g->base_out release];
        if(g->base_glow)[g->base_glow release]; if(g->compose_in_place)[g->compose_in_place release];
        if(g->compose_out)[g->compose_out release]; if(g->diffusion_out)[g->diffusion_out release];
        hs->host_dispose_handle(h);
        [pool drain];
        return PF_Err_NONE;
    }

    extra->output->gpu_data=h;
    out->out_flags2|=PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    [pool drain];
    return PF_Err_NONE;
}

static PF_Err GPUDeviceSetdown(PF_InData* in, PF_OutData* out, PF_GPUDeviceSetdownExtra* extra) {
    if(!extra||!extra->input||!extra->input->gpu_data) return PF_Err_NONE;
    AEFX_SuiteScoper<PF_HandleSuite1> hs(in,kPFHandleSuite,kPFHandleSuiteVersion1,out);
    PF_Handle h=(PF_Handle)extra->input->gpu_data;
    if(!h||!*h) return PF_Err_NONE;
    auto* g=(MetalGPUData*)*h;
    if(g->base)[g->base release]; if(g->base_out)[g->base_out release];
    if(g->base_glow)[g->base_glow release]; if(g->compose_in_place)[g->compose_in_place release];
    if(g->compose_out)[g->compose_out release]; if(g->diffusion_out)[g->diffusion_out release];
    hs->host_dispose_handle(h);
    return PF_Err_NONE;
}

static void Encode2D(id<MTLComputeCommandEncoder> enc,id<MTLComputePipelineState> pipe,NSUInteger w,NSUInteger h) {
    NSUInteger tw=pipe.threadExecutionWidth?pipe.threadExecutionWidth:16;
    NSUInteger th=std::max<NSUInteger>(1,std::min<NSUInteger>(8,pipe.maxTotalThreadsPerThreadgroup/tw));
    [enc dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(tw,th,1)];
}

static id<MTLTexture> MakeTexture(id<MTLDevice> dev,int w,int h,int levels,MTLPixelFormat format) {
    MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:levels>1];
    d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
    d.storageMode=MTLStorageModePrivate;
    if(levels>1) d.mipmapLevelCount=levels;
    return [dev newTextureWithDescriptor:d];
}

static SGParamsGPU PackGPU(const SGRenderState* s,int work_w,int work_h,int out_w,int out_h,
                           int sw,int sh,int sp,int dp,const stellar::RenderPlan& plan) {
    SGParamsGPU q{};
    for(int i=0;i<5;++i) q.colors[i]={s->p.colors[i].r,s->p.colors[i].g,s->p.colors[i].b};
    q.angle_rad=s->p.angle_deg*3.14159265358979323846f/180.f; q.cycles=s->p.cycles; q.offset=s->p.offset; q.phase=s->p.phase_deg/360.f;
    q.saturation=s->p.saturation; q.brightness=s->p.brightness;
    q.depth_contrast=s->p.depth_contrast; q.bulge=s->p.bulge; q.rounding=s->p.rounding;
    q.turbulence_amount=s->p.turbulence_amount; q.turbulence_scale_x=s->p.turbulence_size_x; q.turbulence_scale_y=s->p.turbulence_size_y; q.turbulence_evolution=s->p.turbulence_evolution; q.turbulence_softness=s->p.turbulence_softness;
    q.grain_amount=s->p.grain_amount; q.grain_size=s->p.grain_size_px; q.grain_color=s->p.grain_color; q.grain_seed=s->p.grain_animate?s->frame_index*1664525u+1013904223u:0x12345678u;
    q.glow_radius=s->p.glow_radius_px; q.glow_falloff=s->p.glow_falloff; q.glow_threshold=s->p.glow_threshold; q.glow_intensity=s->p.glow_intensity; q.glow_soft_clip=s->p.glow_soft_clip;
    q.diffusion_blur=s->p.diffusion_blur_px; q.center_x=s->p.diffusion_center.x; q.center_y=s->p.diffusion_center.y; q.focus=s->p.diffusion_focus_px; q.feather=s->p.diffusion_feather_px; q.diffusion_invert=s->p.diffusion_invert?1:0;
    q.glow_samples=plan.glow_samples; q.max_mip_level=std::max(0,plan.max_mip_levels-1);
    q.width=work_w; q.height=work_h; q.src_pitch=sp; q.dst_pitch=dp;
    q.out_width=out_w; q.out_height=out_h;
    q.crop_x=int(s->output_rect.left-s->work_rect.left); q.crop_y=int(s->output_rect.top-s->work_rect.top);
    q.src_width=sw; q.src_height=sh;
    q.src_offset_x=int(s->input_rect.left-s->work_rect.left); q.src_offset_y=int(s->input_rect.top-s->work_rect.top);
    q.origin_x=int(s->work_rect.left); q.origin_y=int(s->work_rect.top);
    q.min_x=int(s->source_max_rect.left-s->work_rect.left); q.min_y=int(s->source_max_rect.top-s->work_rect.top);
    q.max_x=int(s->source_max_rect.right-s->work_rect.left)-1; q.max_y=int(s->source_max_rect.bottom-s->work_rect.top)-1;

    const float bw=static_cast<float>(std::max(1,q.max_x-q.min_x+1));
    const float bh=static_cast<float>(std::max(1,q.max_y-q.min_y+1));
    q.dir_x=std::cos(q.angle_rad); q.dir_y=std::sin(q.angle_rad);
    q.inv_bw=1.0f/bw; q.inv_bh=1.0f/bh;
    q.bound_cx=0.5f*static_cast<float>(q.min_x+q.max_x); q.bound_cy=0.5f*static_cast<float>(q.min_y+q.max_y);
    q.phase_offset=q.offset+q.phase; q.depth_exp=std::max(0.05f,q.depth_contrast);
    q.rounding_clamped=std::clamp(q.rounding,0.0f,1.0f);
    q.turbulence_inv_x=1.0f/std::max(1.0f,q.turbulence_scale_x);
    q.turbulence_inv_y=1.0f/std::max(1.0f,q.turbulence_scale_y);
    q.turbulence_evo_x=q.turbulence_evolution*0.013f; q.turbulence_evo_y=q.turbulence_evolution*0.017f;
    q.grain_inv_size=1.0f/std::max(0.5f,q.grain_size);
    q.glow_lod=plan.glow_lod; q.glow_spread=std::max(0.35f,0.45f*q.glow_falloff);
    q.glow_threshold_inv=1.0f/std::max(1.0e-5f,1.0f-q.glow_threshold);
    q.diffusion_lod=plan.diffusion_max_lod;
    q.diffusion_cx=static_cast<float>(q.min_x)+q.center_x*static_cast<float>(std::max(0,q.max_x-q.min_x));
    q.diffusion_cy=static_cast<float>(q.min_y)+q.center_y*static_cast<float>(std::max(0,q.max_y-q.min_y));
    q.diffusion_inv_feather=1.0f/std::max(1.0f,q.feather);
    q.depth_enabled=std::abs(q.bulge)>1.0e-6f?1u:0u;
    return q;
}

static PF_Err RenderMetal(PF_InData* in,PF_OutData* out,PF_SmartRenderExtra* extra,SGRenderState* state) {
    PF_Err err=PF_Err_NONE, err2=PF_Err_NONE;
    PF_EffectWorld *input=nullptr,*output=nullptr;
    bool checked_input=false;
    NSAutoreleasePool* pool=[[NSAutoreleasePool alloc] init];

    err=extra->cb->checkout_layer_pixels(in->effect_ref,P_INPUT,&input);
    if(!err && input) checked_input=true;
    if(!err) err=extra->cb->checkout_output(in->effect_ref,&output);

    id<MTLTexture> base=nil, glow=nil;
    id<MTLCommandBuffer> cb=nil;

    do {
        if(err||!input||!output){ if(!err) err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }

        AEFX_SuiteScoper<PF_WorldSuite2> ws(in,kPFWorldSuite,kPFWorldSuiteVersion2,out);
        PF_PixelFormat inf=PF_PixelFormat_INVALID,outf=PF_PixelFormat_INVALID;
        err=ws->PF_GetPixelFormat(input,&inf); if(err) break;
        err=ws->PF_GetPixelFormat(output,&outf); if(err) break;
        if(inf!=PF_PixelFormat_GPU_BGRA128||outf!=PF_PixelFormat_GPU_BGRA128){ err=PF_Err_UNRECOGNIZED_PARAM_TYPE; break; }

        AEFX_SuiteScoper<PF_GPUDeviceSuite1> gs(in,kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1,out);
        PF_GPUDeviceInfo di; AEFX_CLR_STRUCT(di);
        err=gs->GetDeviceInfo(in->effect_ref,extra->input->device_index,&di); if(err) break;
        void *smem=nullptr,*dmem=nullptr;
        err=gs->GetGPUWorldData(in->effect_ref,input,&smem); if(err) break;
        err=gs->GetGPUWorldData(in->effect_ref,output,&dmem); if(err) break;
        if(!smem||!dmem||!di.devicePV||!di.command_queuePV){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }

        PF_Handle gh=(PF_Handle)extra->input->gpu_data;
        if(!gh||!*gh){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
        auto* g=(MetalGPUData*)*gh;
        if(!g||!g->base||!g->base_out||!g->base_glow||!g->compose_in_place||!g->compose_out||!g->diffusion_out){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }

        id<MTLDevice> dev=(id<MTLDevice>)di.devicePV;
        id<MTLCommandQueue> queue=(id<MTLCommandQueue>)di.command_queuePV;
        const int out_w=output->width,out_h=output->height;
        const int work_w=int(state->work_rect.right-state->work_rect.left);
        const int work_h=int(state->work_rect.bottom-state->work_rect.top);
        if(work_w<=0||work_h<=0){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
        const int sp=input->rowbytes/int(sizeof(PF_PixelFloat));
        const int dp=output->rowbytes/int(sizeof(PF_PixelFloat));
        auto plan=stellar::make_render_plan(state->p,work_w,work_h,stellar::Backend::GPU);
        SGParamsGPU p=PackGPU(state,work_w,work_h,out_w,out_h,input->width,input->height,sp,dp,plan);
        id<MTLBuffer> src=(id<MTLBuffer>)smem,dst=(id<MTLBuffer>)dmem;
        cb=[queue commandBuffer];
        if(!cb){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }

        if(!plan.glow&&!plan.diffusion && work_w==out_w && work_h==out_h && p.crop_x==0 && p.crop_y==0){
            id<MTLComputeCommandEncoder> e=[cb computeCommandEncoder];
            if(!e){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
            [e setComputePipelineState:g->base_out]; [e setBuffer:src offset:0 atIndex:0]; [e setBuffer:dst offset:0 atIndex:1]; [e setBytes:&p length:sizeof(p) atIndex:2];
            Encode2D(e,g->base_out,out_w,out_h); [e endEncoding]; [cb commit];
            break;
        }

        const int levels=std::max(1,plan.max_mip_levels);
        // Only explicit Preview may use FP16. Auto and Final preserve the F32 quality contract.
        const MTLPixelFormat intermediate_format = state->p.quality==stellar::Quality::Preview ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA32Float;
        base=MakeTexture(dev,work_w,work_h,plan.diffusion?levels:1,intermediate_format);
        if(!base){ err=PF_Err_OUT_OF_MEMORY; break; }
        if(plan.glow){
            glow=MakeTexture(dev,work_w,work_h,levels,intermediate_format);
            if(!glow){ err=PF_Err_OUT_OF_MEMORY; break; }
        }
        id<MTLComputeCommandEncoder> e=[cb computeCommandEncoder];
        if(!e){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
        if(plan.glow){
            [e setComputePipelineState:g->base_glow]; [e setBuffer:src offset:0 atIndex:0];
            [e setTexture:base atIndex:0]; [e setTexture:glow atIndex:1]; [e setBytes:&p length:sizeof(p) atIndex:1];
            Encode2D(e,g->base_glow,work_w,work_h);
        }else{
            [e setComputePipelineState:g->base]; [e setBuffer:src offset:0 atIndex:0]; [e setTexture:base atIndex:0]; [e setBytes:&p length:sizeof(p) atIndex:1];
            Encode2D(e,g->base,work_w,work_h);
        }
        [e endEncoding];

        if(plan.glow){
            id<MTLBlitCommandEncoder> bl=[cb blitCommandEncoder]; if(!bl){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
            [bl generateMipmapsForTexture:glow]; [bl endEncoding];
        }

        if(plan.diffusion){
            if(plan.glow){
                // Compose into base level 0 in place, then reuse the same texture as
                // the diffusion pyramid. This removes a third full mip-chain.
                e=[cb computeCommandEncoder]; if(!e){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
                [e setComputePipelineState:g->compose_in_place]; [e setTexture:base atIndex:0]; [e setTexture:glow atIndex:1]; [e setBytes:&p length:sizeof(p) atIndex:0];
                Encode2D(e,g->compose_in_place,work_w,work_h); [e endEncoding];
            }
            id<MTLBlitCommandEncoder> bl=[cb blitCommandEncoder]; if(!bl){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
            [bl generateMipmapsForTexture:base]; [bl endEncoding];
            e=[cb computeCommandEncoder]; if(!e){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
            [e setComputePipelineState:g->diffusion_out]; [e setTexture:base atIndex:0]; [e setBuffer:dst offset:0 atIndex:0]; [e setBytes:&p length:sizeof(p) atIndex:1];
            Encode2D(e,g->diffusion_out,out_w,out_h); [e endEncoding];
        } else {
            e=[cb computeCommandEncoder]; if(!e){ err=PF_Err_INTERNAL_STRUCT_DAMAGED; break; }
            [e setComputePipelineState:g->compose_out]; [e setTexture:base atIndex:0]; [e setTexture:(glow?glow:base) atIndex:1]; [e setBuffer:dst offset:0 atIndex:0]; [e setBytes:&p length:sizeof(p) atIndex:1];
            Encode2D(e,g->compose_out,out_w,out_h); [e endEncoding];
        }
        [cb commit];
    } while(false);

    if(glow)[glow release]; if(base)[base release];
    if(checked_input) err2=extra->cb->checkin_layer_pixels(in->effect_ref,P_INPUT);
    [pool drain];
    return err ? err : err2;
}

#else
static PF_Err GPUDeviceSetup(PF_InData*,PF_OutData*,PF_GPUDeviceSetupExtra*){return PF_Err_UNRECOGNIZED_PARAM_TYPE;} static PF_Err GPUDeviceSetdown(PF_InData*,PF_OutData*,PF_GPUDeviceSetdownExtra*){return PF_Err_NONE;}
#endif

static PF_Err SmartRender(PF_InData* in, PF_OutData* out, PF_SmartRenderExtra* extra, bool gpu) {
    if(!extra||!extra->input) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    auto* s=(SGRenderState*)extra->input->pre_render_data;
    if(!s) return PF_Err_INTERNAL_STRUCT_DAMAGED;
    if(RectEmpty(s->output_rect)) return PF_Err_NONE;
#ifndef AE_OS_WIN
    if(gpu) return RenderMetal(in,out,extra,s);
#else
    (void)gpu;
#endif
    return RenderCPU(in,out,extra,s);
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,PF_PluginDataCB2 cb,SPBasicSuite*,const char*,const char*){return PF_REGISTER_EFFECT_EXT2(inPtr,cb,SG_NAME,SG_MATCH_NAME,SG_CATEGORY,AE_RESERVED_INFO,"EffectMain","https://github.com/ios3kov");}
extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd,PF_InData* in,PF_OutData* out,PF_ParamDef*[],PF_LayerDef*,void* extra){try{switch(cmd){case PF_Cmd_ABOUT:return About(in,out);case PF_Cmd_GLOBAL_SETUP:return GlobalSetup(in,out);case PF_Cmd_QUERY_DYNAMIC_FLAGS:return QueryDynamicFlags(in,out);case PF_Cmd_PARAMS_SETUP:return ParamsSetup(in,out);case PF_Cmd_GPU_DEVICE_SETUP:return GPUDeviceSetup(in,out,(PF_GPUDeviceSetupExtra*)extra);case PF_Cmd_GPU_DEVICE_SETDOWN:return GPUDeviceSetdown(in,out,(PF_GPUDeviceSetdownExtra*)extra);case PF_Cmd_SMART_PRE_RENDER:return PreRender(in,out,(PF_PreRenderExtra*)extra);case PF_Cmd_SMART_RENDER:return SmartRender(in,out,(PF_SmartRenderExtra*)extra,false);case PF_Cmd_SMART_RENDER_GPU:return SmartRender(in,out,(PF_SmartRenderExtra*)extra,true);case PF_Cmd_RENDER:return PF_Err_UNRECOGNIZED_PARAM_TYPE;default:return PF_Err_NONE;}}catch(const std::bad_alloc&){return PF_Err_OUT_OF_MEMORY;}catch(...){return PF_Err_INTERNAL_STRUCT_DAMAGED;}}
