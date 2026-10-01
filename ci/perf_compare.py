#!/usr/bin/env python3
import argparse
import json
import re
import statistics
import subprocess
import sys
from pathlib import Path

LINE = re.compile(r"^(\d+x\d+) (base|procedural|full): ([0-9]+(?:\.[0-9]+)?) ms/frame$")
EXPECTED = {
    "640x360_base",
    "640x360_procedural",
    "640x360_full",
    "1280x720_base",
    "1280x720_procedural",
    "1280x720_full",
}

def run_once(binary: Path):
    proc = subprocess.run([str(binary)], check=True, text=True, capture_output=True)
    parsed = {}
    for raw in proc.stdout.splitlines():
        m = LINE.match(raw.strip())
        if not m:
            continue
        size, case, ms = m.groups()
        parsed[f"{size}_{case}"] = float(ms)
    if set(parsed) != EXPECTED:
        missing = sorted(EXPECTED - set(parsed))
        extra = sorted(set(parsed) - EXPECTED)
        raise RuntimeError(f"unexpected benchmark output; missing={missing} extra={extra}\n{proc.stdout}")
    return parsed, proc.stdout

def summarize(values):
    return {
        "median_ms": statistics.median(values),
        "min_ms": min(values),
        "max_ms": max(values),
        "samples_ms": values,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline-bin", required=True, type=Path)
    ap.add_argument("--current-bin", required=True, type=Path)
    ap.add_argument("--samples", type=int, default=11)
    ap.add_argument("--max-regression", type=float, default=1.30)
    ap.add_argument("--min-regression-ms", type=float, default=5.0)
    ap.add_argument("--json-out", required=True, type=Path)
    args = ap.parse_args()
    if args.samples < 3:
        raise SystemExit("--samples must be >= 3")
    for binary in (args.baseline_bin, args.current_bin):
        if not binary.is_file():
            raise SystemExit(f"missing benchmark binary: {binary}")

    # One unmeasured process-level warmup per build.
    run_once(args.baseline_bin)
    run_once(args.current_bin)

    raw = {"baseline": {k: [] for k in EXPECTED}, "current": {k: [] for k in EXPECTED}}
    logs = []
    for i in range(args.samples):
        order = ("baseline", "current") if i % 2 == 0 else ("current", "baseline")
        for label in order:
            binary = args.baseline_bin if label == "baseline" else args.current_bin
            parsed, stdout = run_once(binary)
            logs.append({"sample": i + 1, "build": label, "stdout": stdout})
            for key, value in parsed.items():
                raw[label][key].append(value)

    result = {
        "schema": 2,
        "samples_per_build": args.samples,
        "guard_max_regression_ratio": args.max_regression,
        "guard_min_regression_ms": args.min_regression_ms,
        "cases": {},
        "status": "PASS",
        "note": "Level-1 synthetic CPU benchmark; not After Effects, GPU, or release performance evidence.",
    }
    failed = []
    for key in sorted(EXPECTED):
        b = summarize(raw["baseline"][key])
        c = summarize(raw["current"][key])
        ratio = c["median_ms"] / b["median_ms"] if b["median_ms"] > 0.0 else None
        delta_ms = c["median_ms"] - b["median_ms"]
        paired_ratios = [
            current / baseline
            for baseline, current in zip(raw["baseline"][key], raw["current"][key])
            if baseline > 0.0
        ]
        paired_deltas = [
            current - baseline
            for baseline, current in zip(raw["baseline"][key], raw["current"][key])
        ]
        paired_ratio = statistics.median(paired_ratios)
        paired_delta_ms = statistics.median(paired_deltas)
        result["cases"][key] = {
            "baseline": b,
            "current": c,
            "current_over_baseline": ratio,
            "delta_ms": delta_ms,
            "paired_median_ratio": paired_ratio,
            "paired_median_delta_ms": paired_delta_ms,
            "paired_ratios": paired_ratios,
            "paired_deltas_ms": paired_deltas,
        }
        if paired_ratio > args.max_regression and paired_delta_ms >= args.min_regression_ms:
            failed.append((key, paired_ratio, paired_delta_ms))
        print(
            f"{key:24s} baseline={b['median_ms']:9.3f} ms  current={c['median_ms']:9.3f} ms  "
            f"paired_delta={paired_delta_ms:+8.3f} ms  paired_ratio={paired_ratio:6.3f}"
        )

    if failed:
        result["status"] = "FAIL"
        result["failed_cases"] = [{"case": key, "ratio": ratio, "delta_ms": delta_ms} for key, ratio, delta_ms in failed]

    args.json_out.parent.mkdir(parents=True, exist_ok=True)
    args.json_out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if failed:
        for key, ratio, delta_ms in failed:
            print(
                f"FAIL: {key} regression ratio {ratio:.3f} exceeds {args.max_regression:.3f} "
                f"and delta {delta_ms:.3f} ms reaches {args.min_regression_ms:.3f} ms",
                file=sys.stderr,
            )
        return 1
    print("PASS: no paired synthetic CPU case regressed beyond the guard; this is not a speedup claim.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
