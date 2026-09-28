#!/bin/bash
# Compile the exact runtime MSL input. Does not require or exercise an AE/GPU session.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo 'usage: compile-metal.sh SOURCE.metal OUTPUT_DIRECTORY' >&2
    exit 2
fi
SOURCE="$1"
OUT="$2"
test -f "$SOURCE"
mkdir -p "$OUT"
xcrun --sdk macosx --show-sdk-version
xcrun --sdk macosx metal --version
# Match runtime safe/precise options. Language version remains the SDK default,
# as in sg_metal_create; this does not claim compatibility with every older SDK.
xcrun --sdk macosx metal -fno-fast-math -c "$SOURCE" -o "$OUT/StellarGradient.air"
xcrun --sdk macosx metallib "$OUT/StellarGradient.air" -o "$OUT/StellarGradient.metallib"
test -s "$OUT/StellarGradient.air"
python3 - "$OUT/StellarGradient.metallib" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
with p.open('rb') as f:
    if f.read(4) != b'MTLB':
        raise SystemExit('FAIL: shader library header is not MTLB')
print('PASS: compiled Metal IR library, %d bytes; GPU execution NOT RUN' % p.stat().st_size)
PY
# A malformed scratch input must fail. Never rewrite the canonical shader.
printf '#error SG_SHADER_NEGATIVE_CONTROL\n' > "$OUT/negative-control.metal"
if xcrun --sdk macosx metal -fno-fast-math -c "$OUT/negative-control.metal" \
    -o "$OUT/negative-control.air" > "$OUT/metal-negative.log" 2>&1; then
    echo 'FAIL: Metal compiler accepted a deliberate error' >&2
    exit 3
fi
grep -q 'SG_SHADER_NEGATIVE_CONTROL' "$OUT/metal-negative.log"
shasum -a 256 "$SOURCE" "$OUT/StellarGradient.air" "$OUT/StellarGradient.metallib"
echo 'PASS: real MSL compilation and negative control; no AE/render certification'
