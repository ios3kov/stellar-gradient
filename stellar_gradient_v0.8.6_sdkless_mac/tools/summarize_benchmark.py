#!/usr/bin/env python3
import csv, math, statistics, sys
from collections import defaultdict

if len(sys.argv) != 2:
    print('usage: summarize_benchmark.py results.csv', file=sys.stderr)
    raise SystemExit(2)
rows=defaultdict(list)
with open(sys.argv[1], newline='') as f:
    for r in csv.DictReader(f):
        key=(r['plugin'],r['resolution'],r['case'],r.get('engine',''))
        rows[key].append(float(r['ms']))
print('plugin,resolution,case,engine,n,median_ms,p95_ms,fps_at_median')
for key,vals in sorted(rows.items()):
    vals=sorted(vals)
    n=len(vals)
    p95=vals[min(n-1,max(0,math.ceil(n*0.95)-1))]
    med=statistics.median(vals)
    fps=1000.0/med if med>0 else 0.0
    print(','.join(map(str,(*key,n,f'{med:.4f}',f'{p95:.4f}',f'{fps:.2f}'))))
