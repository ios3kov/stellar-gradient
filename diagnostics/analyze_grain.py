#!/usr/bin/env python3
"""Compare isolated RGB8 grain statistics; never modify reference images.
Requires numpy/Pillow. Original random fields need not coincide; no parity PASS.
"""
import argparse
import io
import json
from pathlib import Path
import zipfile
import numpy as np
from PIL import Image
from analyze_isolation import sha, delta, output_projection


def grain_statistics(image, base, mask):
    residual = image.astype(float) - base.astype(float)
    values = residual[mask]
    result = {"mean_absolute_change_codes": float(np.abs(values).mean()),
              "rms_change_codes": float(np.sqrt(np.mean(values**2))),
              "signed_mean_change_codes": float(values.mean())}
    # Independent channel-value bins in the rendered base, not fitted to noise.
    result['base_brightness_bins'] = []
    for low, high in [(0,64),(64,128),(128,192),(192,256)]:
        selection = mask[...,None] & (base >= low) & (base < high)
        v = residual[selection]
        result['base_brightness_bins'].append({"range": [low,high], "channel_samples": int(v.size),
            "mae_codes": float(np.abs(v).mean()) if v.size else None,
            "rms_codes": float(np.sqrt(np.mean(v*v))) if v.size else None})
    return result


def analyze(archive, before, after, base_raw):
    content = Path(archive).read_bytes()
    with zipfile.ZipFile(io.BytesIO(content)) as z:
        reports = [n for n in z.namelist() if n.endswith('/report.json') and not n.startswith('__MACOSX/')]
        if len(reports) != 1: raise ValueError('expected one isolation report')
        rbytes = z.read(reports[0]); r = json.loads(rbytes)
        if r['runner_version'] != 'Isolate 1.0' or r['summary'] != 'CAPTURES_COMPLETE':
            raise ValueError('wrong or incomplete suite')
        if r['run_id'] != 'Stellar-Isolate-1790622394083-275372080':
            raise ValueError('this measurement requires the pinned isolation run')
        prefix = reports[0][:-len('report.json')]
        frames = {}; hashes = {}
        for name in ['cosmic_base','cosmic_grain']:
            case = next(c for c in r['cases'] if c['name'] == name)
            if case['status'] != 'PASS' or case['after_status'] != 'DONE' or case['host_errors']:
                raise ValueError('reference case failed')
            b = z.read(prefix+name+'/frame_00000.png'); hashes[name] = sha(b)
            with Image.open(io.BytesIO(b)) as im:
                im.load()
                if im.mode != 'RGB' or im.size != (512,288): raise ValueError('expected RGB8 512x288')
                frames[name] = np.asarray(im).copy()
    base = frames['cosmic_base']
    occluded = (base[...,0]>250) & (base[...,1]<5) & (base[...,2]<5)
    for _ in range(3):
        p = np.pad(occluded,1)
        occluded = p[1:-1,1:-1]|p[:-2,1:-1]|p[2:,1:-1]|p[1:-1,:-2]|p[1:-1,2:]
    mask = ~occluded
    local_base = output_projection(Path(base_raw),base.shape)
    result = {'schema':1,'run_id':r['run_id'],'archive_sha256':sha(content),'report_sha256':sha(rbytes),
              'reference_png_sha256':hashes,'unoccluded_pixels':int(mask.sum()),
              'scope':'RGB8 scalar statistics only; reference cross untouched; excludes visible red overlay plus 3 pixels',
              'reference':grain_statistics(frames['cosmic_grain'],base,mask),
              'base_projection_error':delta(local_base,base,mask),
              'base_raw_sha256':sha(Path(base_raw).read_bytes())}
    for key, path in [('before',before),('after',after)]:
        image = output_projection(Path(path),base.shape)
        result[key] = {'raw_sha256':sha(Path(path).read_bytes()),
            'statistics':grain_statistics(image,local_base,mask),
            'image_difference_from_reference':delta(image,frames['cosmic_grain'],mask)}
    result['limitations'] = ['Single static Size=1, Amount=20%, Color=100% fixture.',
        'Existing independent Stellar PRNG retained: different random field; no pixel-exact match.',
        'No new AE/Metal execution, HDR/alpha certification or validated other grain settings.',
        'Turbulence/Softness/Glow/Diffusion algorithms not corrected by this change.']
    result['release_status'] = 'NOT_APPROVED'
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive',type=Path);p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True);p.add_argument('--base',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    a.out.write_text(json.dumps(analyze(a.archive,a.before,a.after,a.base),indent=2,sort_keys=True)+'\n')
