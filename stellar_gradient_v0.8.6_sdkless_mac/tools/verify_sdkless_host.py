#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
host=(root/'sdkless/src/lib.rs').read_text()
build=(root/'sdkless/build.rs').read_text()
first=(root/'FIRST_MAC_BUILD.command').read_text()
runner=(root/'sdkless/tools/build_and_install_macos.command').read_text()
cargo=(root/'sdkless/Cargo.toml').read_text()
UPSTREAM_REV='83dcc93734fd5db1335b6ec83cba7a6505a39dcc'
checks={
 'no Adobe SDK gate': 'AE_SDK_PATH' not in first and 'Adobe After Effects SDK not found' not in first,
 'SDK-less AE crate pinned': 'after-effects = { git = "https://github.com/virtualritz/after-effects"' in cargo and f'rev = "{UPSTREAM_REV}"' in cargo,
 'PiPL crate pinned': 'pipl = { git = "https://github.com/virtualritz/after-effects"' in cargo and cargo.count(f'rev = "{UPSTREAM_REV}"') >= 2,
 'GPU selectors': all(x in host for x in ['GpuDeviceSetup','GpuDeviceSetdown','SmartRenderGpu']),
 'GPU world zero-copy': 'gpu_world_data' in host and 'sg_metal_render' in host,
 'Metal precise math configured': all(x in (root/'sdkless/bridge/MetalBridge.mm').read_text() for x in ['mathMode=MTLMathModeSafe','mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise','fastMathEnabled=NO']),
 'CPU frozen core bridge': 'render_reference' in (root/'sdkless/bridge/StellarBridge.cpp').read_text(),
 'release build': 'cargo build --release' in runner,
 'local signing': 'codesign --force --deep --options runtime --sign -' in runner,
 'GPU PiPL flag': 'SupportsGpuRenderF32' in build,
 'MFR PiPL flag': 'SupportsThreadedRendering' in build,
 'MFR Rust cfg pinned': 'cargo:rustc-cfg=threaded_rendering' in build and 'rustc-check-cfg=cfg({name})' in build and '"threaded_rendering"' in build,
 'GPU Rust cfg pinned': all(x in build for x in ['cargo:rustc-cfg=smart_render','cargo:rustc-cfg=gpu_render']),
 'macro cfg names registered': 'rustc-check-cfg=cfg({name})' in build and all(f'"{x}"' in build for x in ['catch_panics','does_dialog','with_premiere','threaded_rendering']),
 'release panic boundary': 'cargo:rustc-cfg=catch_panics' in build and 'panic = "unwind"' in cargo,
 'direct GPU context ownership': 'Box::into_raw(context)' in host and 'Box::from_raw(context_ptr)' in host and 'gpu_data::<GpuContext>()' not in host,
 'Rust ABI tests': 'ffi_layout_matches_cpp_contract' in host and 'size_of::<RenderStateC>()' in host,
 'C++ ABI static asserts': 'static_assert(sizeof(SGRenderStateC) == 264' in (root/'sdkless/bridge/StellarBridge.h').read_text(),
 'Rust host quality gates': all(x in runner for x in ['cargo fmt --all -- --check','cargo check --release','cargo test --release','cargo clippy --release']),
 'external Rust flags replaced with pinned cfgs': ('unset RUSTFLAGS CARGO_ENCODED_RUSTFLAGS' in runner and 'export RUSTFLAGS="--cfg threaded_rendering --cfg smart_render --cfg gpu_render --cfg catch_panics"' in runner),
 'destination cfg compile guards': all(x in host for x in ['cfg missing: MFR host','smart_render cfg missing','gpu_render cfg missing','catch_panics cfg missing']),
}

# Public parameter IDs are a saved-project compatibility contract. v0.8 pins
# them directly on the Params enum and the add helpers reject a mismatched ID.
import re
enum_match=re.search(r'enum Params \{(.*?)\n\}',host,re.S)
ids=[] if not enum_match else [int(x) for x in re.findall(r'=\s*(\d+)',enum_match.group(1))]
checks['parameter IDs 1..49 stable + palette 50..51 + center 52 + depth angle 53']=(len(ids)==53 and sorted(ids)==list(range(1,54)) and len(set(ids))==53 and 'Presets = 1' in host and 'PaletteTopic = 50' in host and 'PaletteEnd = 51' in host and 'DiffCenter = 52' in host and 'DepthAngle = 53' in host and 'id != key as i32' in host)
# Rust requires a leading zero on fractional literals (0.13, not .13).
checks['no leading-dot Rust floats']=(re.search(r'(?<![A-Za-z0-9_.])\.\d', host) is None)
checks['palette preset UX']=all(x in host for x in ['"Palette",50,true,false','Params::Presets,"Presets"','UserChangedParam','apply_cosmic_preset','set_presets_menu_custom'])
checks['palette group contains full top controls']=(
    host.index('add_group(params,Params::PaletteTopic') <
    host.index('Params::Presets,"Presets"') <
    host.index('Params::Brightness,"Brightness"') <
    host.index('add_group(params,Params::PaletteEnd') <
    host.index('add_group(params,Params::DepthTopic')
)
checks['original-style angle/percent controls']=all(x in host for x in ['Params::Angle,"Angle",ae::AngleDef::setup','Params::Phase,"Phase",ae::AngleDef::setup','percent_slider!(Params::Offset','percent_slider!(Params::Contrast','percent_slider!(Params::Bulge','percent_slider!(Params::Rounding'])
checks['ephemeral rustfmt build copy']=('HOST_BUILD="$ROOT/.sdkless-build"' in runner and 'cargo fmt --all' in runner and 'cp -R "$HOST_SRC" "$HOST_BUILD"' in runner)
checks['native optical center'] = all(x in host for x in [
    'Params::DiffCenter,"Center",ae::PointDef::setup',
    'Params::DiffCenter)?.as_point()?.value()',
]) and 'add_hidden_id' not in host
checks['native depth angle'] = all(x in host for x in [
    'Params::DepthAngle,"Angle",ae::AngleDef::setup',
    'p.depth_angle_deg=params.get(Params::DepthAngle)?.as_angle()?.float_value()',
])

failed=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(('PASS' if v else 'FAIL')+': '+k)
if failed: raise SystemExit(1)
