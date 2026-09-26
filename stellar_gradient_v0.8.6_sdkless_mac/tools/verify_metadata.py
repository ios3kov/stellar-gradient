#!/usr/bin/env python3
from pathlib import Path
import re, sys

root=Path(__file__).resolve().parents[1]
cpp=(root/'src/plugin/StellarGradient.mm').read_text()
pipl=(root/'src/plugin/StellarGradientPiPL.r').read_text()

def macro_int(name):
    m=re.search(rf'^#define\s+{re.escape(name)}\s+(\d+)\s*$', cpp, re.M)
    if not m:
        raise SystemExit(f'missing {name}')
    return int(m.group(1))

major=macro_int('SG_MAJOR_VERSION')
minor=macro_int('SG_MINOR_VERSION')
bug=macro_int('SG_BUG_VERSION')
build=macro_int('SG_BUILD_VERSION')
stage=2 if re.search(r'^#define\s+SG_STAGE_VERSION\s+PF_Stage_BETA\s*$', cpp, re.M) else None
if stage is None:
    raise SystemExit('unsupported SG_STAGE_VERSION')

# Numeric values from AE_Effect.h. Keep this intentionally explicit so PiPL
# drift is caught even though the C++ and Rez sources compile separately.
PF_OutFlag_DEEP_COLOR_AWARE = 1 << 25
PF_OutFlag_NON_PARAM_VARY = 1 << 2
PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS = 1 << 0
PF_OutFlag2_REVEALS_ZERO_ALPHA = 1 << 7
PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG = 1 << 3
PF_OutFlag2_SUPPORTS_SMART_RENDER = 1 << 10
PF_OutFlag2_FLOAT_COLOR_AWARE = 1 << 12
PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 = 1 << 25
PF_OutFlag2_SUPPORTS_THREADED_RENDERING = 1 << 26

required_cpp_flags = [
    'PF_OutFlag_DEEP_COLOR_AWARE',
    'PF_OutFlag_NON_PARAM_VARY',
    'PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS',
    'PF_OutFlag2_SUPPORTS_SMART_RENDER',
    'PF_OutFlag2_FLOAT_COLOR_AWARE',
    'PF_OutFlag2_SUPPORTS_THREADED_RENDERING',
    'PF_OutFlag2_SUPPORTS_GPU_RENDER_F32',
    'PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG',
    'PF_OutFlag2_REVEALS_ZERO_ALPHA',
]
for f in required_cpp_flags:
    if f not in cpp:
        print(f'missing C++ flag {f}', file=sys.stderr)
        sys.exit(2)
if 'PF_OutFlag_PIX_INDEPENDENT' in cpp:
    print('PIX_INDEPENDENT is invalid for Glow/Diffusion', file=sys.stderr)
    sys.exit(3)

out1 = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY
out2 = (PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS |
        PF_OutFlag2_SUPPORTS_SMART_RENDER |
        PF_OutFlag2_FLOAT_COLOR_AWARE |
        PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
        PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 |
        PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG |
        PF_OutFlag2_REVEALS_ZERO_ALPHA)
version = major*524288 + minor*32768 + bug*2048 + stage*512 + build

checks={
    'AE_Effect_Version': version,
    'AE_Effect_Global_OutFlags': out1,
    'AE_Effect_Global_OutFlags_2': out2,
}
for key,val in checks.items():
    m=re.search(rf'{key}\s*\{{\s*(0x[0-9A-Fa-f]+|\d+)\s*\}}',pipl)
    if not m:
        print(f'missing {key}',file=sys.stderr); sys.exit(4)
    got=int(m.group(1),0)
    if got!=val:
        print(f'{key}: got {got:#x}, expected {val:#x}',file=sys.stderr); sys.exit(5)
print(f'metadata OK: v{major}.{minor}.{bug} build {build}')
