#!/usr/bin/env python3
from pathlib import Path
import sys

metal = Path('src/gpu/StellarGradientMetal.metal').read_text()
wrapper = Path('src/plugin/StellarGradient.mm').read_text()
build_mac = Path('tools/build_mac.sh').read_text()
errors=[]
for needle in ('cos(', 'sin(', 'log2('):
    if needle in metal:
        errors.append(f'per-pixel Metal still contains {needle}')
if 'SGBaseGlowKernel' not in metal or 'SGGlowSeedKernel' in metal:
    errors.append('base/glow fusion contract broken')
if 'newBufferWithBytes' in wrapper:
    errors.append('per-frame parameter MTLBuffer allocation returned')
if 'waitUntilCompleted' in wrapper:
    errors.append('CPU/GPU synchronization stall found')
if 'fastMathEnabled=NO' not in wrapper:
    errors.append('precise Metal math contract not explicit')
if '-ffast-math' in build_mac or '-Ofast' in build_mac:
    errors.append('Mac C++ build enables unsafe fast-math')
if '-fno-fast-math' not in build_mac:
    errors.append('Mac C++ precise-math flag is not explicit')
if 'quality==stellar::Quality::Preview ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA32Float' not in wrapper:
    errors.append('Auto/Final are not locked to FP32 intermediates')
if wrapper.count('[queue commandBuffer]') != 1:
    errors.append('expected exactly one render command-buffer creation site')
if errors:
    for e in errors: print('FAIL:',e,file=sys.stderr)
    raise SystemExit(1)
print('PASS: Metal performance/quality source contract')
