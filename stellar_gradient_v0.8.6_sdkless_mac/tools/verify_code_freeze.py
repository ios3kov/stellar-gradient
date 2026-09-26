#!/usr/bin/env python3
from pathlib import Path
import re, sys
cpp=Path('src/plugin/StellarGradient.mm').read_text()
sanitize=Path('src/core/Sanitize.h').read_text()
build=Path('tools/build_mac.sh').read_text()
errors=[]

# Persistent parameter IDs must be explicit and independent from ParamIndex.
required_ids={
    'ID_PALETTE':1,'ID_COLOR1':2,'ID_COLOR5':6,'ID_ANGLE':7,'ID_BRIGHTNESS':12,
    'ID_DEPTH_TOPIC':13,'ID_DEPTH_END':17,'ID_TURB_TOPIC':18,'ID_TURB_END':24,
    'ID_LOOK_TOPIC':25,'ID_GLOW_TOPIC':26,'ID_GLOW_END':32,'ID_GRAIN_TOPIC':33,
    'ID_GRAIN_END':38,'ID_DIFF_TOPIC':39,'ID_DIFF_END':46,'ID_LOOK_END':47,
    'ID_ENGINE':48,'ID_QUALITY':49,
}
for name,val in required_ids.items():
    if not re.search(rf'\b{name}\s*=\s*{val}\b',cpp): errors.append(f'{name} is not pinned to historical id {val}')
for line in cpp.splitlines():
    if ('PF_ADD_' in line or 'PF_END_TOPIC' in line) and re.search(r',\s*P_[A-Z0-9_]+\s*\)?\s*;',line):
        errors.append('parameter setup still uses ParamIndex as persistent disk id: '+line.strip())

if 'PF_GetFloatingPointColorFromColorDef' not in cpp:
    errors.append('HDR color parameters are not retrieved through PF_ColorParamSuite1')
if 'kColorLimit = 65504.0f' not in sanitize:
    errors.append('HDR/under-range finite color preservation guard missing')
if 'supports32BitFloatFiltering' not in cpp:
    errors.append('Metal FP32 filtering capability is not checked')
if 'state->p.quality!=stellar::Quality::Preview && !g->supports_f32_filtering' not in cpp:
    errors.append('Auto/Final GPU gate does not require FP32 filtering support')
if 'if(RectEmpty(s->output_rect)) return PF_Err_NONE;' not in cpp:
    errors.append('empty SmartFX output is not an explicit no-op')
if 'if(err){ [pool drain]; return PF_Err_NONE; }' not in cpp:
    errors.append('GPU device probing failure does not cleanly reject GPU')
if 'fastMathEnabled=NO' not in cpp or '-fno-fast-math' not in build or '-fno-unsafe-math-optimizations' not in build:
    errors.append('precise Release math contract is incomplete')
if errors:
    for e in errors: print('FAIL:',e,file=sys.stderr)
    raise SystemExit(1)
print('PASS: code-freeze source contract')
