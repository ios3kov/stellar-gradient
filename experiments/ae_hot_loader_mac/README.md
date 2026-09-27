# AE Hot Loader — macOS PoC

Experimental proof-of-concept for registering a new After Effects effect while AE is already running.

## Scope

First target only:
- macOS
- Apple Silicon (arm64)
- After Effects effect plug-ins
- no production claims until the stop criterion passes

## Stop criterion

1. Start After Effects with `AEHotLoader.plugin` installed.
2. Apply **AE Hot Loader** to a layer.
3. Click **Register Late Effect**.
4. Without restarting AE, **AE Hot Loader Late Effect** appears in the Effect menu.
5. Apply the late effect and confirm it renders correctly.

If this fails, the public registration callback is not sufficient for live registration and the next phase is internal-loader investigation.

## Architecture

- `wrapper/AEHotLoader.cpp`
  - is the actual bundle executable loaded by AE
  - captures `PF_PluginDataPtr` + `PF_PluginDataCB2`
  - registers the normal effect during startup
  - exposes a late-registration function
  - forwards `EffectMain` / `LateEffectMain` to the Rust core
- `core/`
  - SDK-less effect implementation using the same Rust `after-effects` stack as Stellar Gradient
  - contains a **Register Late Effect** button
  - passes frames through unchanged

## Safety

The late registration path is intentionally experimental. It never unloads an effect and allows only one late-registration attempt per AE process.
