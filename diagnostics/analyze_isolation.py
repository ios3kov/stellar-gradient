#!/usr/bin/env python3
"""Analyze returned isolation PNGs and optional local base CPU dumps; never edit inputs.
Requires numpy and Pillow. All exclusions apply to scalar statistics only.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile

import numpy as np
from PIL import Image


def sha(data):
    return hashlib.sha256(data).hexdigest()


def delta(a, b, mask):
    d = (a.astype(np.float64) - b.astype(np.float64))[mask]
    return {"mae_codes": float(np.abs(d).mean()), "max_codes": float(np.abs(d).max()),
            "rmse_codes": float(np.sqrt(np.mean(d*d)))}


def neighbors(image, mask):
    d = np.abs(np.diff(image.astype(float), axis=1))
    return float(d[mask[:, 1:] & mask[:, :-1]].mean())


def output_projection(path, shape):
    # Explicit reproduction of this particular RGB8 output, NOT a new color
    # transform inside the plugin. Does not establish OCIO/HDR host behavior.
    data = np.fromfile(path, dtype='<f4').reshape(shape[0], shape[1], 4)
    if not np.isfinite(data).all():
        raise ValueError("nonfinite replay")
    linear = np.floor(np.clip(data[:, :, :3].astype(float), 0, 1)*255 + .5)/255
    encoded = np.where(linear < .0031308, linear*12.92, 1.055*linear**(1/2.4)-.055)
    return np.floor(encoded*255 + .5).astype(np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--old-base', type=Path)
    parser.add_argument('--new-base', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    original = args.archive.read_bytes()
    with zipfile.ZipFile(io.BytesIO(original)) as z:
        reports = [n for n in z.namelist() if n.endswith('/report.json') and not n.startswith('__MACOSX/')]
        if len(reports) != 1:
            raise ValueError("expected one report")
        prefix = reports[0][:-len('report.json')]
        report_bytes = z.read(reports[0])
        r = json.loads(report_bytes)
        if r['runner_version'] != 'Isolate 1.0' or len(r['cases']) != 22:
            raise ValueError("wrong test suite")
        frames, hashes = {}, {}
        for case in r['cases']:
            name = case['name']
            if '/' in name or '\\' in name or '..' in name:
                raise ValueError("invalid case name")
            content = z.read(prefix + name + '/frame_00000.png')
            with Image.open(io.BytesIO(content)) as im:
                im.load()
                if im.mode != 'RGB' or im.size != (512, 288):
                    raise ValueError("expected RGB8 512x288")
                frames[name] = np.asarray(im).copy()
            hashes[name] = sha(content)
        result = {"schema": 1, "run_id": r['run_id'], "archive_sha256": sha(original),
                  "report_sha256": sha(report_bytes), "events_sha256": sha(z.read(prefix+'events.jsonl')),
                  "runner_version": r['runner_version'], "runner_build_id": r['runner_build_id'],
                  "reported_summary": r['summary'], "environment": r['environment'],
                  "project_settings": r['project_settings'], "loaded_build_id": r['loaded_build_id'],
                  "png_sha256": hashes, "decoded_png_count": len(frames),
                  "cases": [{k: c[k] for k in ('name','status','after_status','cleanup','host_errors')} for c in r['cases']]}
    base = frames['cosmic_base']
    # Exclude the visibly red diagonal overlay and a three-pixel safety border.
    # No replacement pixels, crop, inpainting or modified reference is produced.
    occluded = (base[:, :, 0] > 250) & (base[:, :, 1] < 5) & (base[:, :, 2] < 5)
    for _ in range(3):
        pad = np.pad(occluded, 1)
        occluded = pad[1:-1,1:-1] | pad[:-2,1:-1] | pad[2:,1:-1] | pad[1:-1,:-2] | pad[1:-1,2:]
    mask = ~occluded
    result['statistics_scope'] = {"unit":"8-bit RGB output code (0..255)", "unoccluded_pixels":int(mask.sum()),
                                  "overlay_exclusion":"pure red overlay plus three Manhattan pixels; scalar calculations only",
                                  "outputs":"22 original PNGs remain unchanged; no alpha/HDR data"}
    result['features'] = []
    for c in r['cases'][::2]:
        suffix = c['variant']
        feature = {"variant":suffix}
        for host in ('cosmic','stellar'):
            image = frames[host+'_'+suffix]
            feature[host] = {"difference_from_own_base":delta(image, frames[host+'_base'], mask),
                             "horizontal_neighbor_mae":neighbors(image,mask)}
        result['features'].append(feature)
    if args.old_base or args.new_base:
        result['local_cpu_replay'] = {"scope":"base only, output projection validated against old installed Stellar base; not a host/GPU test"}
        for label,path in [('old',args.old_base),('new',args.new_base)]:
            if path:
                image = output_projection(path,base.shape)
                result['local_cpu_replay'][label] = {"raw_sha256":sha(path.read_bytes()),
                    "versus_cosmic_base":delta(image,base,mask),
                    "versus_installed_stellar_base":delta(image,frames['stellar_base'],mask)}
    result['release_status'] = 'NOT_APPROVED'
    args.out.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')


if __name__ == '__main__':
    main()
