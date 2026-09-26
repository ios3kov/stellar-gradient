#!/usr/bin/env python3
from pathlib import Path
import sys

cpp=Path('src/plugin/StellarGradient.mm').read_text()
pipl=Path('src/plugin/StellarGradientPiPL.r').read_text()
errors=[]
required=[
    'PF_OutFlag2_SUPPORTS_SMART_RENDER',
    'PF_OutFlag2_SUPPORTS_THREADED_RENDERING',
    'PF_OutFlag2_SUPPORTS_GPU_RENDER_F32',
    'PF_OutFlag2_REVEALS_ZERO_ALPHA',
    'PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS',
    'PF_OutFlag_NON_PARAM_VARY',
    'PF_Cmd_QUERY_DYNAMIC_FLAGS',
    'PF_RenderOutputFlag_GPU_RENDER_POSSIBLE',
    'PF_PixelFormat_ARGB32','PF_PixelFormat_ARGB64','PF_PixelFormat_ARGB128',
    'PF_PixelFormat_GPU_BGRA128',
]
for token in required:
    if token not in cpp: errors.append(f'missing AE contract token: {token}')
if 'PF_OutFlag_PIX_INDEPENDENT' in cpp: errors.append('PIX_INDEPENDENT is invalid for neighbor-dependent Glow/Diffusion')
if 'engine_mode != 3' not in cpp: errors.append('CPU engine does not explicitly suppress GPU render')
if 'extra->input->gpu_data' not in cpp: errors.append('GPU pre-render does not require successful device setup data')
if 'out->out_flags2 &= ~PF_OutFlag2_SUPPORTS_GPU_RENDER_F32' not in cpp: errors.append('GPU setup cannot cleanly reject a Metal device for CPU fallback')
if 'catch(const std::bad_alloc&)' not in cpp: errors.append('allocation failure is not mapped to PF_Err_OUT_OF_MEMORY')
if 'checkout_layer(in->effect_ref,P_INPUT,P_INPUT,&dependency' not in cpp: errors.append('SmartFX dependency checkout missing')
if 'ComputeWorkMargin' not in cpp or 'ComputeMipAlignment' not in cpp: errors.append('SmartFX halo/alignment logic missing')
if '0x02000004' not in pipl or '0x06001489' not in pipl: errors.append('PiPL flags do not include dynamic-time cache contract')
if errors:
    for e in errors: print('FAIL:',e,file=sys.stderr)
    raise SystemExit(1)
print('PASS: After Effects SmartFX/MFR/cache/pixel-format source contract')
