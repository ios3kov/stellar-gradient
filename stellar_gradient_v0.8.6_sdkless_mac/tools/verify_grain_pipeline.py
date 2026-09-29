#!/usr/bin/env python3
"""Static placement/embedding checks only; does not execute a Metal shader."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
s = (root/'src/gpu/StellarGradientMetal.metal').read_text()

def body(text, name):
    start = text.index(name+'(')
    start = text.index('{',start); depth=1;end=start+1
    while depth:
        if text[end]=='{': depth+=1
        if text[end]=='}': depth-=1
        end+=1
    return text[start:end]

def check_shader(text):
    finals=['SGBaseOutKernel','SGComposeOutKernel','SGDiffusionOutKernel']
    intermediate=['shade_color','shade_base','SGBaseKernel','SGBaseUnmaskedKernel','SGSoftMaskKernel','SGGlowSourceKernel','SGBaseGlowKernel','composite_pixel','SGComposeInPlaceKernel']
    for n in finals:
        if body(text,n).count('finish_grain(')!=1: raise ValueError(n+': expected grain exactly once')
    for n in intermediate:
        if 'finish_grain(' in body(text,n) or 'grain_amount' in body(text,n):
            raise ValueError(n+': intermediate shading must remain grain-free')
    helper=body(text,'finish_grain')
    if 'pixel.rgb+=' not in helper or 'clamp(pixel.rgb' not in helper or 'p.grain_amount<=1.0e-6f' not in helper:
        raise ValueError('missing RGB-only envelope or shared activity threshold')
    if 'pixel.a=' in helper or 'pixel.a+=' in helper: raise ValueError('grain must not modify alpha')

check_shader(s)
expected='#pragma once\nstatic const char kStellarGradientMetalSource[] = R"STELLAR_MSL(\n'+s+'\n)STELLAR_MSL";\n'
if (root/'src/gpu/StellarGradientMetalSource.generated.h').read_text()!=expected:
    raise SystemExit('FAIL: regenerate embedded Metal header')
if '--self-test' in sys.argv:
    bad=[s.replace('finish_grain(shade_base(src,depth_map,p,gid),p,gid)','shade_base(src,depth_map,p,gid)'),
         s.replace('return float4(shade_color(depth_map,p,gid)*alpha,alpha);','return finish_grain(float4(shade_color(depth_map,p,gid)*alpha,alpha),p,gid);'),
         s.replace('pixel.rgb+=','pixel.a+=')]
    for t in bad:
        try: check_shader(t)
        except ValueError: pass
        else: raise SystemExit('FAIL: damaged pipeline accepted')
    print('PASS: three negative mutation checks')
print('PASS: grain in three final outputs only; Metal header byte-exact. GPU execution NOT RUN.')
