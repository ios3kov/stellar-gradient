#!/usr/bin/env python3
import pathlib
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: verify_hot_reload_state.py LIB_RS SHELL_CPP")

lib = pathlib.Path(sys.argv[1]).read_text()
shell = pathlib.Path(sys.argv[2]).read_text()

def require(pattern: str, text: str, label: str) -> str:
    m = re.search(pattern, text, re.MULTILINE | re.DOTALL)
    if not m:
        raise SystemExit(f"missing {label}")
    return m.group(1)


def extract_function(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"missing function {signature}")
    opened = text.find("{", start)
    if opened < 0:
        raise SystemExit(f"missing function body {signature}")
    depth = 0
    in_string = False
    quote = ""
    escaped = False
    line_comment = False
    block_comment = 0
    i = opened
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if c == "\n":
                line_comment = False
            i += 1
            continue
        if block_comment:
            if c == "*" and n == "/":
                block_comment -= 1
                i += 2
                continue
            if c == "/" and n == "*":
                block_comment += 1
                i += 2
                continue
            i += 1
            continue
        if in_string:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                in_string = False
            i += 1
            continue
        if c == "/" and n == "/":
            line_comment = True
            i += 2
            continue
        if c == "/" and n == "*":
            block_comment = 1
            i += 2
            continue
        if c in ('"', "'"):
            in_string = True
            quote = c
            i += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    raise SystemExit(f"unbalanced function body {signature}")


def normalized_code(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', '', text)
    return re.sub(r'\s+', '', text)


def fnv1a64(text: str) -> int:
    value = 1469598103934665603
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


EXPECTED_PARAMS_SETUP_FNV64 = 0x1eca99aaffa95737
params_setup_hash = fnv1a64(normalized_code(extract_function(lib, "fn params_setup(")))
if params_setup_hash != EXPECTED_PARAMS_SETUP_FNV64:
    raise SystemExit(
        "params_setup host contract changed; this cannot be live-reloaded safely. "
        "Bump StateABI, rebuild/install the shell, and update the verifier intentionally: "
        f"expected=0x{EXPECTED_PARAMS_SETUP_FNV64:016x} actual=0x{params_setup_hash:016x}"
    )

impl_protocol = int(require(
    r'AEHotLoader_ImplementationABI\(\)\s*->\s*u32\s*\{\s*(\d+)\s*\}',
    lib,
    "implementation protocol ABI",
))
shell_protocol = int(require(
    r'kImplementationAbi\s*=\s*(\d+)\s*;',
    shell,
    "shell implementation ABI",
))
if impl_protocol != 2 or shell_protocol != 2 or impl_protocol != shell_protocol:
    raise SystemExit(
        f"Protocol ABI drift: implementation={impl_protocol} shell={shell_protocol}; expected=2"
    )

def fields(struct_name: str):
    body = require(rf'struct\s+{struct_name}\s*\{{(.*?)\}}', lib, struct_name)
    out = []
    for item in body.split(','):
        item = item.strip()
        if not item or ':' not in item:
            continue
        name, typ = item.split(':', 1)
        out.append((name.strip(), typ.strip()))
    return out

impl_abi = int(require(r'const\s+HOT_RELOAD_STATE_ABI:\s*u64\s*=\s*(\d+)\s*;', lib, "implementation StateABI"))
shell_abi = int(require(r'kImplementationStateAbi\s*=\s*(\d+)\s*;', shell, "shell StateABI"))
if impl_abi != shell_abi:
    raise SystemExit(f"StateABI drift: implementation={impl_abi} shell={shell_abi}")
if impl_abi != 4:
    raise SystemExit(f"unexpected Stellar StateABI {impl_abi}; update verifier intentionally when schema changes")

if "ae::define_effect!(Plugin, (), Params);" not in lib:
    raise SystemExit("effect global/sequence contract changed; review StateABI")

params_body = require(r'enum\s+Params\s*\{(.*?)\}', lib, "Params enum")
actual_params = [(name, int(value)) for name, value in re.findall(r'([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\d+)', params_body)]
expected_names = [
    "Presets","Color1","Color2","Color3","Color4","Color5",
    "Angle","Cycles","Offset","Phase","Saturation","Brightness",
    "DepthTopic","Contrast","Bulge","Rounding","DepthEnd",
    "TurbTopic","TurbAmount","TurbSizeX","TurbSizeY","TurbEvolution","TurbSoftness","TurbEnd",
    "LookTopic","GlowTopic","GlowRadius","GlowFalloff","GlowThreshold","GlowIntensity","GlowSoftClip","GlowEnd",
    "GrainTopic","GrainAmount","GrainSize","GrainColor","GrainAnimate","GrainEnd",
    "DiffTopic","DiffBlur","DiffCenterX","DiffCenterY","DiffFocus","DiffFeather","DiffInvert","DiffEnd",
    "LookEnd","Engine","Quality","PaletteTopic","PaletteEnd","DiffCenter","DepthAngle",
]
expected_params = [(name, i + 1) for i, name in enumerate(expected_names)]
if actual_params != expected_params:
    raise SystemExit(
        "Stellar parameter IDs changed; bump StateABI and update verifier intentionally:\n"
        f"  expected={expected_params}\n  actual={actual_params}"
    )

expected = {
    "Color3": [("r","f32"),("g","f32"),("b","f32")],
    "Point2": [("x","f32"),("y","f32")],
    "RectC": [("left","i32"),("top","i32"),("right","i32"),("bottom","i32")],
    "ParamsC": [
        ("colors","[Color3; 5]"),
        ("angle_deg","f32"),("cycles","f32"),("offset","f32"),("phase_deg","f32"),
        ("saturation","f32"),("brightness","f32"),
        ("depth_angle_deg","f32"),("depth_contrast","f32"),("bulge","f32"),("rounding","f32"),
        ("turbulence_amount","f32"),("turbulence_size_x","f32"),("turbulence_size_y","f32"),
        ("turbulence_evolution","f32"),("turbulence_softness","f32"),
        ("glow_radius_px","f32"),("glow_falloff","f32"),("glow_threshold","f32"),
        ("glow_intensity","f32"),("glow_soft_clip","f32"),
        ("grain_amount","f32"),("grain_size_px","f32"),("grain_color","f32"),("grain_animate","u32"),
        ("diffusion_blur_px","f32"),("diffusion_center","Point2"),("diffusion_focus_px","f32"),
        ("diffusion_feather_px","f32"),("diffusion_invert","u32"),
        ("quality","u32"),
    ],
    "RenderStateC": [
        ("params","ParamsC"),("input_rect","RectC"),("source_max_rect","RectC"),
        ("output_rect","RectC"),("work_rect","RectC"),("time_seconds","f64"),
        ("frame_index","u32"),("engine_mode","i32"),("generation","u64"),
    ],
    "GpuContext": [
        ("ptr","usize"),("generation","u64"),
        ("destroy_fn","MetalDestroyFn"),("supports_f32","bool")
    ],
}
for name, wanted in expected.items():
    actual = fields(name)
    if actual != wanted:
        raise SystemExit(
            f"{name} schema changed; bump StateABI and update verifier intentionally:\n"
            f"  expected={wanted}\n  actual={actual}"
        )

for evidence in [
    "size_of::<RenderStateC>()",
    "offset_of!(RenderStateC, engine_mode)",
    "offset_of!(RenderStateC, generation)",
    "size_of::<GpuContext>()",
    "offset_of!(GpuContext, generation)",
    "offset_of!(GpuContext, destroy_fn)",
    "AEHotLoader_ImplementationRuntimeABI",
    "AEHotLoader_SetGeneration",
]:
    if evidence not in lib:
        raise SystemExit(f"missing hot-reload state/layout evidence: {evidence}")


build_rs = pathlib.Path(sys.argv[1]).parent.parent / "build.rs"
build_text = build_rs.read_text()
global_flags = require(
    r'Property::AE_Effect_Global_OutFlags\((.*?)\),\s*Property::AE_Effect_Global_OutFlags_2',
    build_text,
    "PiPL Global OutFlags",
)
normalized_global = re.sub(r'\s+', '', global_flags)
expected_global = "OutFlags::DeepColorAware|OutFlags::NonParamVary"
if normalized_global != expected_global:
    raise SystemExit(
        f"PiPL Global OutFlags changed; reinstall/restart required: expected={expected_global} actual={normalized_global}"
    )

out2_body = require(
    r'Property::AE_Effect_Global_OutFlags_2\((.*?)\)\s*,\s*Property::AE_Effect_Match_Name',
    build_text,
    "PiPL Global OutFlags2",
)
actual_out2 = re.findall(r'OutFlags2::([A-Za-z0-9_]+)', out2_body)
expected_out2 = ["SupportsQueryDynamicFlags","ParamGroupStartCollapsedFlag","RevealsZeroAlpha","SupportsSmartRender","FloatColorAware","SupportsThreadedRendering","SupportsGetFlattenedSequenceData","SupportsGpuRenderF32"]
if actual_out2 != expected_out2:
    raise SystemExit(
        f"PiPL OutFlags2 changed; reinstall/restart required: expected={expected_out2} actual={actual_out2}"
    )

print(
    f"hot-reload state: PASS StateABI={impl_abi} "
    f"params={len(actual_params)} pre_render=RenderStateC gpu_generation=present"
)
