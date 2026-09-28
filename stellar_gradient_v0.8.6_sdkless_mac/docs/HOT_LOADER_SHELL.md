# Stellar Gradient — AE Hot Loader adapter

Branch: `feature/ae-hot-loader-shell`.

This branch adapts Stellar Gradient to the stable-shell architecture used by AE Hot Loader. It does not modify `main`.

## Host-visible identity

The After Effects identity remains:

- name: `Stellar Gradient`;
- category: `Stellar`;
- match name: `StellarLabs.StellarGradient`;
- effect API: `13.29`;
- parameter IDs remain frozen;
- preset behavior and group/collapse metadata remain frozen;
- support URL remains `https://github.com/ios3kov`.

PiPL metadata and shell registration metadata are compared in CI.

## Bundle layout

```
StellarGradient.plugin
└── Contents
    ├── MacOS/StellarGradient
    ├── Frameworks/libstellar_gradient_impl.dylib
    └── Resources/StellarGradient.rsrc
```

The bundle executable is a stable C++ shell. The Rust/C++/Metal host is the swappable implementation dylib.

## Hot-reload contract

Current adapter contract:

- Shell ABI: `1`
- Implementation Protocol ABI: `2`
- StateABI: `4`
- implementation key: `stellar-gradient`
- pinned Rust toolchain: `1.98.1`

Required implementation exports:

- `EffectMain`
- `AEHotLoader_ImplementationABI`
- `AEHotLoader_ImplementationStateABI`
- `AEHotLoader_ImplementationKey`
- `AEHotLoader_ImplementationLabel`
- `AEHotLoader_ImplementationRuntimeABI`
- `AEHotLoader_SetGeneration`

The bundled implementation establishes the process Runtime ABI. A candidate built with another Rust/runtime contract is rejected without replacing the active build.

## Generation-safe persistent state

The shell assigns a content-derived generation before candidate publication.

Stellar Gradient generation-tags:

- `RenderStateC` SmartFX pre-render payload;
- GPU `GpuContext`.

GPU state also carries the destroy-function pointer from the generation that created the native Metal object. Because old dylibs remain loaded, stale GPU state is destroyed by creator-generation code.

State-contract CI freezes:

- exact numeric parameter IDs;
- empty plugin global/sequence state contract;
- `ParamsC`;
- `RenderStateC`;
- `GpuContext`;
- generation field;
- creator-owned destroy function;
- Protocol ABI / StateABI parity.

Any intentional parameter or persistent-state change requires an explicit StateABI bump and verifier update.

## MFR / reload synchronization

Concurrent EffectMain calls remain allowed.

Reload is fail-fast:

- when calls are in flight, reload reports busy/retry;
- it does not block AE's main thread waiting for render completion;
- candidate publication happens only with zero active calls.

## Reload workflow

After the shell has been installed and AE restarted once:

### Source workflow

```bash
zsh stellar_gradient_v0.8.6_sdkless_mac/sdkless/tools/stage_hot_reload_macos.command my-build-label
```

The source stager:

- verifies metadata/state contracts;
- pins Rust `1.98.1`;
- pins macOS target `11.0`;
- supports rustup-managed and standalone pinned Cargo;
- signs and atomically publishes the candidate.

### Packaged test kit

The CI artifact contains:

- `StellarGradient.plugin`
- `StellarGradientImpl-candidate.dylib`
- `INSTALL_HOT_LOADER.command`
- `STAGE_CANDIDATE.command`

The installer refuses ambiguous duplicate copies, backs up the existing bundle, verifies signature/arm64, and restores the previous installation if replacement fails.

Then click:

`Window → AE Hot Loader → Reload Plugins`

Removing the candidate and reloading returns to the bundled default implementation.

## Failure behavior

The active implementation remains unchanged when:

- candidate cannot load;
- required exports are missing;
- Protocol ABI / StateABI / Runtime ABI mismatches;
- implementation key is wrong;
- ABI strings are malformed;
- effect is busy;
- generation cap is reached.

## CI status

Current hardened checkpoint:

- Hot Loader Shell CI **#66 — SUCCESS**

Coverage includes:

- PiPL ↔ shell metadata parity;
- exact parameter/capability contract;
- state-contract verifier;
- generation behavior tests;
- default + candidate builds;
- default → candidate → unchanged → bundled rollback;
- deployment target check;
- dylib dependency check;
- signed binary test kit.

## Remaining live AE gate

Before merge:

1. existing projects resolve `StellarLabs.StellarGradient`;
2. presets, parameters and collapsed groups are unchanged;
3. CPU Smart Render works;
4. real MFR scheduling remains stable;
5. Metal setup/render/setdown works;
6. reload during active render returns retry;
7. reload between SmartPreRender/SmartRender rejects stale generation payload safely;
8. existing instances survive A→B→C swaps;
9. removing the candidate rolls back to bundled default;
10. save/reopen remains compatible.

No merge to `main` before this live gate passes.
