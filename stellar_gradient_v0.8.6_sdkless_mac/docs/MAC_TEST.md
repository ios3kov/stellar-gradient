# macOS first test — SDK-less host

## 1. Requirements

- After Effects 2025 or newer
- Apple Silicon Mac
- Xcode / Xcode command-line tools
- Internet connection on the first build only if Rust/Cargo or crates are not already cached

**Adobe After Effects SDK is not required.**

## 2. Build + install

Double-click `FIRST_MAC_BUILD.command`.

The script runs preflight/tests, builds, signs, installs and launches After Effects automatically.

The effect should appear at:

**Effect > Stellar > Stellar Gradient**

## 3. Minimum test

1. New comp, 1920x1080, 30 fps.
2. Add a text or shape layer with transparency around it.
3. Apply Stellar Gradient.
4. Confirm the gradient is clipped to the alpha silhouette.
5. Raise Glow Radius/Intensity and verify glow extends outside the original silhouette without a hard crop.
6. Animate Phase from 0 to 360 and verify the loop is seamless.
7. Test **Render Engine > Auto**, **GPU**, and **CPU**.
8. Switch Project Settings > Video Rendering between Metal and Software Only.
9. Repeat in 8, 16 and 32 bpc.
10. Zoom/pan/scrub so SmartFX requests different regions; no seams in Turbulence, Grain, Glow or Diffusion.
11. Compare Auto/GPU/CPU visually in 32 bpc; no visible quality loss is acceptable.

## 4. If build or AE loading fails

Send `mac_first_build_report.zip` created beside the project. Do not edit the source first; the exact first failure is the useful diagnostic.
