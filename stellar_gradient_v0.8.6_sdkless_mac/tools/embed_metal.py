#!/usr/bin/env python3
from pathlib import Path
src=Path(__file__).resolve().parents[1]/"src/gpu/StellarGradientMetal.metal"
out=Path(__file__).resolve().parents[1]/"src/gpu/StellarGradientMetalSource.generated.h"
s=src.read_text()
out.write_text('#pragma once\nstatic const char kStellarGradientMetalSource[] = R"STELLAR_MSL(\n'+s+'\n)STELLAR_MSL";\n')
print(out)
