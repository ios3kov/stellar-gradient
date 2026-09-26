#!/usr/bin/env python3
from pathlib import Path
import hashlib, sys

root=Path(__file__).resolve().parents[1]
manifest=root/'docs/SOURCE_FREEZE_V08_SHA256.txt'

def inventory():
    files=[]
    for base in ['src/core','src/cpu','src/gpu','sdkless','tests','tools']:
        d=root/base
        if not d.exists(): continue
        for p in d.rglob('*'):
            if not p.is_file(): continue
            rel=p.relative_to(root)
            parts=set(rel.parts)
            if 'target' in parts or '__pycache__' in parts: continue
            files.append(rel)
    for rel in [Path('CMakeLists.txt'),Path('FIRST_MAC_BUILD.command')]:
        if (root/rel).is_file(): files.append(rel)
    return sorted(set(files), key=lambda p:p.as_posix())

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()

if not manifest.exists():
    print('FAIL: v0.8 freeze manifest missing',file=sys.stderr); raise SystemExit(1)
expected={}
for line in manifest.read_text().splitlines():
    if not line.strip(): continue
    digest,path=line.split('  ',1)
    expected[path]=digest
current=[p.as_posix() for p in inventory()]
if sorted(expected)!=current:
    missing=sorted(set(expected)-set(current)); extra=sorted(set(current)-set(expected))
    for x in missing: print('FAIL missing source:',x,file=sys.stderr)
    for x in extra: print('FAIL untracked source:',x,file=sys.stderr)
    raise SystemExit(2)
for path,digest in expected.items():
    got=sha(root/path)
    if got!=digest:
        print(f'FAIL changed: {path}\n expected {digest}\n got      {got}',file=sys.stderr)
        raise SystemExit(3)
print(f'PASS: v0.8.6 source freeze ({len(expected)} files)')
