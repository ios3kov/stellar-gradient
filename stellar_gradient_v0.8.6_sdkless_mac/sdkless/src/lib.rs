use after_effects as ae;

// Hard build contract: PiPL advertises SmartFX/MFR/GPU, so the destination
// crate must actually compile the after-effects macro with the same cfgs.
// FIRST_MAC_BUILD.command passes these explicitly via RUSTFLAGS.
#[cfg(not(threaded_rendering))]
compile_error!("threaded_rendering cfg missing: MFR host would compile with mutable global state");
#[cfg(not(smart_render))]
compile_error!("smart_render cfg missing: SmartFX host contract is not active");
#[cfg(not(gpu_render))]
compile_error!("gpu_render cfg missing: GPU selectors are not active");
#[cfg(not(catch_panics))]
compile_error!("catch_panics cfg missing: release FFI panic boundary is not active");
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct Color3 { r: f32, g: f32, b: f32 }
#[repr(C)]
#[derive(Clone, Copy, Default)]
struct Point2 { x: f32, y: f32 }
#[repr(C)]
#[derive(Clone, Copy, Default)]
struct RectC { left: i32, top: i32, right: i32, bottom: i32 }
#[repr(C)]
#[derive(Clone, Copy)]
struct ParamsC {
    colors: [Color3; 5],
    angle_deg: f32, cycles: f32, offset: f32, phase_deg: f32, saturation: f32, brightness: f32,
    depth_contrast: f32, bulge: f32, rounding: f32,
    turbulence_amount: f32, turbulence_size_x: f32, turbulence_size_y: f32, turbulence_evolution: f32, turbulence_softness: f32,
    glow_radius_px: f32, glow_falloff: f32, glow_threshold: f32, glow_intensity: f32, glow_soft_clip: f32,
    grain_amount: f32, grain_size_px: f32, grain_color: f32, grain_animate: u32,
    diffusion_blur_px: f32, diffusion_center: Point2, diffusion_focus_px: f32, diffusion_feather_px: f32, diffusion_invert: u32,
    quality: u32,
}
impl Default for ParamsC {
    fn default() -> Self {
        Self {
            colors: [Color3::default(); 5], angle_deg: 90.0, cycles: 1.0, offset: 0.0, phase_deg: 0.0,
            saturation: 1.0, brightness: 1.0, depth_contrast: 1.0, bulge: 0.0, rounding: 0.0,
            turbulence_amount: 0.0, turbulence_size_x: 120.0, turbulence_size_y: 120.0, turbulence_evolution: 0.0, turbulence_softness: 0.5,
            glow_radius_px: 60.0, glow_falloff: 1.6, glow_threshold: 0.2, glow_intensity: 0.8, glow_soft_clip: 0.25,
            grain_amount: 0.03, grain_size_px: 1.0, grain_color: 0.0, grain_animate: 1,
            diffusion_blur_px: 0.0, diffusion_center: Point2 { x: 0.5, y: 0.5 }, diffusion_focus_px: 120.0, diffusion_feather_px: 200.0,
            diffusion_invert: 0, quality: 1,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
struct RenderStateC {
    params: ParamsC,
    input_rect: RectC,
    source_max_rect: RectC,
    output_rect: RectC,
    work_rect: RectC,
    time_seconds: f64,
    frame_index: u32,
    engine_mode: i32,
}

unsafe extern "C" {
    fn sg_prepare_params(params: *mut ParamsC, downsample_x: f32, downsample_y: f32);
    fn sg_dependency_rect(params: *const ParamsC, requested: RectC) -> RectC;
    fn sg_finalize_rects(params: *const ParamsC, requested: RectC, input_result: RectC, input_max: RectC,
                         output_rect: *mut RectC, work_rect: *mut RectC, semantic_max_rect: *mut RectC);
    fn sg_rect_empty(rect: RectC) -> i32;
    fn sg_cpu_render(state: *const RenderStateC,
                     input_data: *const c_void, input_width: i32, input_height: i32, input_rowbytes: i32,
                     output_data: *mut c_void, output_width: i32, output_height: i32, output_rowbytes: i32,
                     bitdepth: i32) -> i32;
    #[cfg(target_os = "macos")]
    fn sg_metal_create(mtl_device: *mut c_void, supports_f32_filtering: *mut i32) -> *mut c_void;
    #[cfg(target_os = "macos")]
    fn sg_metal_destroy(context: *mut c_void);
    #[cfg(target_os = "macos")]
    fn sg_metal_render(context: *mut c_void, command_queue: *mut c_void, input_buffer: *mut c_void, output_buffer: *mut c_void,
                       state: *const RenderStateC, input_width: i32, input_height: i32, input_rowbytes: i32,
                       output_width: i32, output_height: i32, output_rowbytes: i32) -> i32;
}

#[repr(i32)]
#[derive(Eq, PartialEq, Hash, Clone, Copy, Debug)]
enum Params {
    Colors = 1, Color1 = 2, Color2 = 3, Color3 = 4, Color4 = 5, Color5 = 6,
    Angle = 7, Cycles = 8, Offset = 9, Phase = 10, Saturation = 11, Brightness = 12,
    DepthTopic = 13, Contrast = 14, Bulge = 15, Rounding = 16, DepthEnd = 17,
    TurbTopic = 18, TurbAmount = 19, TurbSizeX = 20, TurbSizeY = 21, TurbEvolution = 22, TurbSoftness = 23, TurbEnd = 24,
    LookTopic = 25, GlowTopic = 26, GlowRadius = 27, GlowFalloff = 28, GlowThreshold = 29, GlowIntensity = 30, GlowSoftClip = 31, GlowEnd = 32,
    GrainTopic = 33, GrainAmount = 34, GrainSize = 35, GrainColor = 36, GrainAnimate = 37, GrainEnd = 38,
    DiffTopic = 39, DiffBlur = 40, DiffCenterX = 41, DiffCenterY = 42, DiffFocus = 43, DiffFeather = 44, DiffInvert = 45, DiffEnd = 46,
    LookEnd = 47, Engine = 48, Quality = 49,
    PaletteTopic = 50, PaletteEnd = 51,
}

#[derive(Default)]
struct Plugin;
ae::define_effect!(Plugin, (), Params);

// Store the native pointer as an integer so the render-side context is plain,
// immutable Send+Sync data. AE owns the device lifetime between GPU setup and
// setdown; the pointed-to Metal context is read-only during frame dispatch.
struct GpuContext { ptr: usize, supports_f32: bool }

fn rc_to_err(rc: i32) -> Result<(), ae::Error> {
    match rc {
        0 => Ok(()),
        -2 => Err(ae::Error::OutOfMemory),
        -1 => Err(ae::Error::BadCallbackParameter),
        _ => Err(ae::Error::InternalStructDamaged),
    }
}
fn rect_from_raw(r: ae::sys::PF_LRect) -> RectC { RectC { left: r.left, top: r.top, right: r.right, bottom: r.bottom } }
fn rect_to_raw(r: RectC) -> ae::sys::PF_LRect { ae::sys::PF_LRect { left: r.left, top: r.top, right: r.right, bottom: r.bottom } }
fn rect_to_ae(r: RectC) -> ae::Rect { ae::Rect { left: r.left, top: r.top, right: r.right, bottom: r.bottom } }

fn palette_colors(which: i32) -> Option<[[u8; 3]; 5]> {
    const PRESETS: [[[u8; 3]; 5]; 15] = [
        [[255,33,61],[255,107,26],[255,227,41],[61,242,189],[36,94,255]],
        [[18,38,20],[46,97,43],[107,158,51],[199,201,92],[237,224,166]],
        [[48,20,38],[117,46,79],[186,89,122],[235,145,158],[255,204,186]],
        [[4,3,10],[18,10,41],[46,18,79],[92,31,130],[173,64,194]],
        [[5,28,61],[0,117,209],[0,235,237],[133,255,117],[255,232,23]],
        [[13,3,36],[71,8,135],[161,15,227],[245,51,199],[92,176,255]],
        [[0,41,61],[0,120,156],[0,199,196],[94,245,199],[204,255,235]],
        [[56,10,3],[150,33,5],[235,92,10],[255,168,31],[255,232,122]],
        [[255,26,69],[255,135,26],[250,235,31],[26,212,140],[20,110,255]],
        [[107,46,242],[209,77,255],[255,110,186],[255,168,112],[115,232,255]],
        [[8,35,43],[0,115,119],[50,214,184],[122,82,255],[255,75,180]],
        [[42,4,2],[110,14,5],[205,46,10],[255,111,20],[255,204,92]],
        [[67,31,52],[143,71,92],[244,135,119],[255,199,160],[190,235,255]],
        [[5,8,30],[28,20,77],[80,41,130],[182,58,155],[255,115,165]],
        [[14,36,8],[61,112,10],[139,207,16],[218,255,53],[45,255,190]],
    ];
    if (2..=16).contains(&which) {
        Some(PRESETS[(which - 2) as usize])
    } else {
        None
    }
}

fn apply_palette_to_color_params(params: &mut ae::Parameters<Params>, which: i32) -> Result<(), ae::Error> {
    let Some(colors) = palette_colors(which) else { return Ok(()); };
    for (key, rgb) in [Params::Color1, Params::Color2, Params::Color3, Params::Color4, Params::Color5]
        .into_iter()
        .zip(colors.into_iter())
    {
        let mut def = params.get_mut(key)?;
        {
            let mut color = def.as_color_mut()?;
            color.set_value(ae::Pixel8 { alpha: 255, red: rgb[0], green: rgb[1], blue: rgb[2] });
        }
        def.set_value_changed();
    }
    Ok(())
}

fn set_colors_menu_custom(params: &mut ae::Parameters<Params>) -> Result<(), ae::Error> {
    let mut def = params.get_mut(Params::Colors)?;
    let is_custom = def.as_popup()?.value() == 1;
    if !is_custom {
        {
            let mut popup = def.as_popup_mut()?;
            popup.set_value(1);
        }
        def.set_value_changed();
    }
    Ok(())
}

fn gather_params(params: &mut ae::Parameters<Params>) -> Result<(ParamsC,i32), ae::Error> {
    let mut p=ParamsC::default();
    for (i,key) in [Params::Color1,Params::Color2,Params::Color3,Params::Color4,Params::Color5].into_iter().enumerate() {
        let c=params.get(key)?.as_color()?.float_value()?;
        p.colors[i]=Color3{r:c.red,g:c.green,b:c.blue};
    }
    macro_rules! f { ($field:ident,$key:expr) => { p.$field=params.get($key)?.as_float_slider()?.value() as f32; }; }
    f!(angle_deg,Params::Angle); f!(cycles,Params::Cycles); f!(offset,Params::Offset); f!(phase_deg,Params::Phase); f!(saturation,Params::Saturation); f!(brightness,Params::Brightness);
    f!(depth_contrast,Params::Contrast); f!(bulge,Params::Bulge); f!(rounding,Params::Rounding);
    f!(turbulence_amount,Params::TurbAmount); f!(turbulence_size_x,Params::TurbSizeX); f!(turbulence_size_y,Params::TurbSizeY); f!(turbulence_evolution,Params::TurbEvolution); f!(turbulence_softness,Params::TurbSoftness);
    f!(glow_radius_px,Params::GlowRadius); f!(glow_falloff,Params::GlowFalloff); f!(glow_threshold,Params::GlowThreshold); f!(glow_intensity,Params::GlowIntensity); f!(glow_soft_clip,Params::GlowSoftClip);
    f!(grain_amount,Params::GrainAmount); f!(grain_size_px,Params::GrainSize); f!(grain_color,Params::GrainColor); p.grain_animate=params.get(Params::GrainAnimate)?.as_checkbox()?.value() as u32;
    f!(diffusion_blur_px,Params::DiffBlur); let cx=params.get(Params::DiffCenterX)?.as_float_slider()?.value() as f32; let cy=params.get(Params::DiffCenterY)?.as_float_slider()?.value() as f32; p.diffusion_center=Point2{x:cx*0.01,y:cy*0.01};
    f!(diffusion_focus_px,Params::DiffFocus); f!(diffusion_feather_px,Params::DiffFeather); p.diffusion_invert=params.get(Params::DiffInvert)?.as_checkbox()?.value() as u32;
    let engine=params.get(Params::Engine)?.as_popup()?.value();
    let quality=params.get(Params::Quality)?.as_popup()?.value(); p.quality=if quality<=1{0}else if quality==2{1}else{2};
    Ok((p,engine))
}

fn add_id<'a>(params: &mut ae::Parameters<Params>, key: Params, name: &str, def: impl Into<ae::Param<'a>>, id: i32) -> Result<(), ae::Error> {
    if id != key as i32 { return Err(ae::Error::InvalidParms); }
    params.add_customized(key,name,def,move |pd| { pd.set_id(key as i32); -1 })
}
fn add_supervised_id<'a>(params: &mut ae::Parameters<Params>, key: Params, name: &str, def: impl Into<ae::Param<'a>>, id: i32) -> Result<(), ae::Error> {
    if id != key as i32 { return Err(ae::Error::InvalidParms); }
    params.add_customized(key,name,def,move |pd| {
        pd.set_id(key as i32);
        pd.set_flag(ae::ParamFlag::SUPERVISE,true);
        -1
    })
}
fn add_group(params: &mut ae::Parameters<Params>, key: Params, name: &str, id: i32, start: bool, collapsed: bool) -> Result<(), ae::Error> {
    if id != key as i32 { return Err(ae::Error::InvalidParms); }
    params.add_customized(key,name,ae::NullDef::new(),move |pd| {
        pd.set_id(key as i32);
        pd.as_mut().param_type=if start { ae::sys::PF_Param_GROUP_START } else { ae::sys::PF_Param_GROUP_END };
        if start && collapsed { pd.set_flags(ae::ParamFlag::START_COLLAPSED); }
        -1
    })
}

impl AdobePluginGlobal for Plugin {
    fn params_setup(&self, params: &mut ae::Parameters<Params>, _in_data: ae::InData, _: ae::OutData) -> Result<(), ae::Error> {
        macro_rules! slider { ($key:expr,$name:expr,$vmin:expr,$vmax:expr,$smin:expr,$smax:expr,$d:expr,$prec:expr,$id:expr) => {{
            add_id(params,$key,$name,ae::FloatSliderDef::setup(|x|{x.set_valid_min($vmin);x.set_valid_max($vmax);x.set_slider_min($smin);x.set_slider_max($smax);x.set_default($d);x.set_precision($prec);x.set_value(x.default());}),$id)?;
        }}; }
        add_group(params,Params::PaletteTopic,"Palette",50,true,false)?;
        add_supervised_id(params,Params::Colors,"Colors",ae::PopupDef::setup(|x|{x.set_options(&["Custom","Hot Neon","Moss","Rose Dust","Void","Voltage","UV Bloom","Aqua","Golden Hour","Spectrum","Sugar","Aurora","Ember","Peach Ice","Night Bloom","Acid Lime"]);x.set_default(1);x.set_value(1);}),1)?;
        for (key,name,rgba,id) in [
            (Params::Color1,"Color 1",[31,13,87,255],2),(Params::Color2,"Color 2",[31,97,242,255],3),(Params::Color3,"Color 3",[170,43,242,255],4),
            (Params::Color4,"Color 4",[255,79,140,255],5),(Params::Color5,"Color 5",[255,191,46,255],6)
        ] { add_supervised_id(params,key,name,ae::ColorDef::setup(|x|{let c=ae::Pixel8{red:rgba[0],green:rgba[1],blue:rgba[2],alpha:rgba[3]};x.set_default(c);x.set_value(c);}),id)?; }
        add_group(params,Params::PaletteEnd,"",51,false,false)?;
        slider!(Params::Angle,"Angle",-720.0,720.0,-180.0,180.0,90.0,1,7); slider!(Params::Cycles,"Cycles",0.1,20.0,0.1,5.0,1.0,2,8);
        slider!(Params::Offset,"Offset",-100.0,100.0,-2.0,2.0,0.0,2,9); slider!(Params::Phase,"Phase",-100000.0,100000.0,0.0,360.0,0.0,1,10);
        slider!(Params::Saturation,"Saturation",0.0,2.0,0.0,2.0,1.0,2,11); slider!(Params::Brightness,"Brightness",0.0,4.0,0.0,2.0,1.0,2,12);
        add_group(params,Params::DepthTopic,"Depth",13,true,true)?; slider!(Params::Contrast,"Contrast",0.05,8.0,0.05,4.0,1.0,2,14); slider!(Params::Bulge,"Bulge",-2.0,2.0,-1.0,1.0,0.0,2,15); slider!(Params::Rounding,"Rounding",0.0,1.0,0.0,1.0,0.0,2,16); add_group(params,Params::DepthEnd,"",17,false,false)?;
        add_group(params,Params::TurbTopic,"Turbulence",18,true,true)?; slider!(Params::TurbAmount,"Amount",0.0,2.0,0.0,1.0,0.0,2,19); slider!(Params::TurbSizeX,"Size X",1.0,2000.0,1.0,500.0,120.0,1,20); slider!(Params::TurbSizeY,"Size Y",1.0,2000.0,1.0,500.0,120.0,1,21); slider!(Params::TurbEvolution,"Evolution",-100000.0,100000.0,-360.0,360.0,0.0,2,22); slider!(Params::TurbSoftness,"Softness",0.0,1.0,0.0,1.0,0.5,2,23); add_group(params,Params::TurbEnd,"",24,false,false)?;
        add_group(params,Params::LookTopic,"Look",25,true,false)?; add_group(params,Params::GlowTopic,"Glow",26,true,true)?; slider!(Params::GlowRadius,"Radius",0.0,2000.0,0.0,500.0,60.0,1,27); slider!(Params::GlowFalloff,"Falloff",0.25,4.0,0.25,4.0,1.6,2,28); slider!(Params::GlowThreshold,"Threshold",0.0,1.0,0.0,1.0,0.2,2,29); slider!(Params::GlowIntensity,"Intensity",0.0,10.0,0.0,3.0,0.8,2,30); slider!(Params::GlowSoftClip,"Soft Clip",0.0,1.0,0.0,1.0,0.25,2,31); add_group(params,Params::GlowEnd,"",32,false,false)?;
        add_group(params,Params::GrainTopic,"Grain",33,true,true)?; slider!(Params::GrainAmount,"Amount",0.0,0.5,0.0,0.15,0.03,3,34); slider!(Params::GrainSize,"Size",0.5,16.0,0.5,8.0,1.0,2,35); slider!(Params::GrainColor,"Color",0.0,1.0,0.0,1.0,0.0,2,36); add_id(params,Params::GrainAnimate,"Animate",ae::CheckBoxDef::setup(|x|{x.set_default(true);x.set_value(true);}),37)?; add_group(params,Params::GrainEnd,"",38,false,false)?;
        add_group(params,Params::DiffTopic,"Optical Diffusion",39,true,true)?; slider!(Params::DiffBlur,"Blur",0.0,2000.0,0.0,500.0,0.0,1,40); slider!(Params::DiffCenterX,"Center X",0.0,100.0,0.0,100.0,50.0,1,41); slider!(Params::DiffCenterY,"Center Y",0.0,100.0,0.0,100.0,50.0,1,42); slider!(Params::DiffFocus,"Focus",0.0,4000.0,0.0,1000.0,120.0,1,43); slider!(Params::DiffFeather,"Feather",0.0,4000.0,0.0,1000.0,200.0,1,44); add_id(params,Params::DiffInvert,"Invert",ae::CheckBoxDef::setup(|x|{x.set_default(false);x.set_value(false);}),45)?; add_group(params,Params::DiffEnd,"",46,false,false)?; add_group(params,Params::LookEnd,"",47,false,false)?;
        add_id(params,Params::Engine,"Render Engine",ae::PopupDef::setup(|x|{x.set_options(&["Auto","GPU","CPU"]);x.set_default(1);x.set_value(1);}),48)?;
        add_id(params,Params::Quality,"Quality",ae::PopupDef::setup(|x|{x.set_options(&["Preview","Auto","Final"]);x.set_default(2);x.set_value(2);}),49)?;
        Ok(())
    }

    fn handle_command(&self, cmd: ae::Command, in_data: ae::InData, mut out_data: ae::OutData, params: &mut ae::Parameters<Params>) -> Result<(), ae::Error> {
        match cmd {
            ae::Command::About => out_data.set_return_msg("Stellar Gradient v0.8.7\rCurated palette presets + Metal + CPU SmartFX"),
            ae::Command::UserChangedParam { param_index } => {
                match params.type_at(param_index) {
                    Params::Colors => {
                        let which=params.get(Params::Colors)?.as_popup()?.value();
                        apply_palette_to_color_params(params,which)?;
                        out_data.set_out_flag(ae::OutFlags::RefreshUi,true);
                    }
                    Params::Color1 | Params::Color2 | Params::Color3 | Params::Color4 | Params::Color5 => {
                        set_colors_menu_custom(params)?;
                        out_data.set_out_flag(ae::OutFlags::RefreshUi,true);
                    }
                    _ => {}
                }
            }
            ae::Command::QueryDynamicFlags => {
                let animate=params.get(Params::GrainAnimate)?.as_checkbox()?.value();
                let amount=params.get(Params::GrainAmount)?.as_float_slider()?.value();
                out_data.set_out_flag(ae::OutFlags::NonParamVary, animate && amount>1.0e-6);
            }
            ae::Command::SmartPreRender { mut extra } => {
                let (mut p,engine)=gather_params(params)?;
                unsafe { sg_prepare_params(&mut p, f32::from(in_data.downsample_x()), f32::from(in_data.downsample_y())); }
                let requested=rect_from_raw(extra.output_request().rect);
                let dep=unsafe { sg_dependency_rect(&p,requested) };
                let mut req=extra.output_request(); req.rect=rect_to_raw(dep);
                let cr=extra.callbacks().checkout_layer(0,0,&req,in_data.current_time(),in_data.time_step(),in_data.time_scale())?;
                let mut state=RenderStateC { params:p, input_rect:rect_from_raw(cr.result_rect), source_max_rect:rect_from_raw(cr.max_result_rect),
                    output_rect:RectC::default(), work_rect:RectC::default(), time_seconds:if in_data.time_scale()!=0 {in_data.current_time() as f64/in_data.time_scale() as f64}else{0.0}, frame_index:if in_data.time_step()!=0 {(in_data.current_time()/in_data.time_step()) as u32}else{0}, engine_mode:engine };
                let mut semantic=RectC::default();
                unsafe { sg_finalize_rects(&state.params,requested,state.input_rect,state.source_max_rect,&mut state.output_rect,&mut state.work_rect,&mut semantic); }
                extra.set_result_rect(rect_to_ae(state.output_rect)); extra.set_max_result_rect(rect_to_ae(semantic));
                let mut gpu_possible=false;
                #[cfg(target_os="macos")]
                if engine!=3 && extra.what_gpu()==ae::GpuFramework::Metal {
                    unsafe {
                        let raw=extra.as_ptr();
                        if !raw.is_null() && !(*raw).input.is_null() && !(*(*raw).input).gpu_data.is_null() {
                            let g=&*((*(*raw).input).gpu_data as *const GpuContext);
                            gpu_possible=state.params.quality==0 || g.supports_f32;
                        }
                    }
                }
                extra.set_gpu_render_possible(gpu_possible);
                extra.set_pre_render_data(state);
            }
            ae::Command::SmartRender { extra } => {
                let Some(state)=extra.pre_render_data::<RenderStateC>() else { return Err(ae::Error::InternalStructDamaged); };
                if unsafe { sg_rect_empty(state.output_rect) } != 0 { return Ok(()); }
                let cb=extra.callbacks(); let Some(input)=cb.checkout_layer_pixels(0)? else { return Ok(()); };
                let result=(|| {
                    let Some(output)=cb.checkout_output()? else { return Ok(()); };
                    if input.bit_depth()!=output.bit_depth() { return Err(ae::Error::BadCallbackParameter); }
                    let rc=unsafe { sg_cpu_render(state, input.data_ptr() as *const c_void,input.width() as i32,input.height() as i32,input.row_bytes() as i32,
                        output.data_ptr_mut() as *mut c_void,output.width() as i32,output.height() as i32,output.row_bytes() as i32,output.bit_depth() as i32) };
                    rc_to_err(rc)
                })(); cb.checkin_layer_pixels(0)?; result?;
            }
            #[cfg(target_os="macos")]
            ae::Command::GpuDeviceSetup { extra } => {
                out_data.set_out_flag2(ae::OutFlags2::SupportsGpuRenderF32,false);
                if extra.what_gpu()!=ae::GpuFramework::Metal { return Ok(()); }
                let suite=ae::pf::suites::GPUDevice::new()?; let info=suite.device_info(in_data.effect_ref(),extra.device_index())?;
                if info.devicePV.is_null() { return Ok(()); }
                let mut f32_ok=0; let ptr=unsafe { sg_metal_create(info.devicePV,&mut f32_ok) };
                if !ptr.is_null() {
                    let raw=extra.as_ptr();
                    if raw.is_null() || unsafe { (*raw).output.is_null() } {
                        unsafe { sg_metal_destroy(ptr); }
                        return Err(ae::Error::InternalStructDamaged);
                    }
                    let context=Box::new(GpuContext{ptr:ptr as usize,supports_f32:f32_ok!=0});
                    unsafe { (*(*raw).output).gpu_data=Box::into_raw(context) as *mut c_void; }
                    out_data.set_out_flag2(ae::OutFlags2::SupportsGpuRenderF32,true);
                }
            }
            #[cfg(target_os="macos")]
            ae::Command::GpuDeviceSetdown { extra } => {
                if extra.what_gpu()==ae::GpuFramework::Metal {
                    let raw=extra.as_ptr();
                    let has_data=unsafe { !raw.is_null() && !(*raw).input.is_null() && !(*(*raw).input).gpu_data.is_null() };
                    if has_data {
                        unsafe {
                            let input=(*raw).input;
                            let context_ptr=(*input).gpu_data as *mut GpuContext;
                            (*input).gpu_data=std::ptr::null_mut();
                            let g=Box::from_raw(context_ptr);
                            sg_metal_destroy(g.ptr as *mut c_void);
                        }
                    }
                }
            }
            #[cfg(target_os="macos")]
            ae::Command::SmartRenderGpu { extra } => {
                let Some(state)=extra.pre_render_data::<RenderStateC>() else { return Err(ae::Error::InternalStructDamaged); };
                if unsafe { sg_rect_empty(state.output_rect) } != 0 { return Ok(()); }
                let raw=extra.as_ptr();
                let g=unsafe {
                    if raw.is_null() || (*raw).input.is_null() || (*(*raw).input).gpu_data.is_null() {
                        return Err(ae::Error::InternalStructDamaged);
                    }
                    &*((*(*raw).input).gpu_data as *const GpuContext)
                };
                let cb=extra.callbacks(); let Some(mut input)=cb.checkout_layer_pixels(0)? else { return Ok(()); };
                let result=(|| {
                    let Some(mut output)=cb.checkout_output()? else { return Ok(()); };
                    if input.pixel_format()?!=ae::PixelFormat::GpuBgra128 || output.pixel_format()?!=ae::PixelFormat::GpuBgra128 { return Err(ae::Error::UnrecogizedParameterType); }
                    let suite=ae::pf::suites::GPUDevice::new()?; let info=suite.device_info(in_data.effect_ref(),extra.device_index())?;
                    if info.command_queuePV.is_null() { return Err(ae::Error::InternalStructDamaged); }
                    let src=suite.gpu_world_data(in_data.effect_ref(),&mut input)?; let dst=suite.gpu_world_data(in_data.effect_ref(),&mut output)?;
                    let rc=unsafe { sg_metal_render(g.ptr as *mut c_void,info.command_queuePV,src,dst,state,input.width() as i32,input.height() as i32,input.row_bytes() as i32,output.width() as i32,output.height() as i32,output.row_bytes() as i32) };
                    rc_to_err(rc)
                })(); cb.checkin_layer_pixels(0)?; result?;
            }
            _ => {}
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ffi_layout_matches_cpp_contract() {
        assert_eq!(std::mem::size_of::<Color3>(), 12);
        assert_eq!(std::mem::size_of::<Point2>(), 8);
        assert_eq!(std::mem::size_of::<RectC>(), 16);
        assert_eq!(std::mem::size_of::<ParamsC>(), 180);
        assert_eq!(std::mem::align_of::<ParamsC>(), 4);
        assert_eq!(std::mem::offset_of!(ParamsC, diffusion_center), 156);
        assert_eq!(std::mem::offset_of!(ParamsC, quality), 176);
        assert_eq!(std::mem::size_of::<RenderStateC>(), 264);
        assert_eq!(std::mem::align_of::<RenderStateC>(), 8);
        assert_eq!(std::mem::offset_of!(RenderStateC, input_rect), 180);
        assert_eq!(std::mem::offset_of!(RenderStateC, time_seconds), 248);
        assert_eq!(std::mem::offset_of!(RenderStateC, engine_mode), 260);
    }
}
