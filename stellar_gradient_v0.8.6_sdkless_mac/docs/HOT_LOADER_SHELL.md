# Stellar Gradient — AE Hot Loader adapter

Branch: `feature/ae-hot-loader-shell`.

This branch adapts Stellar Gradient to the stable-shell architecture used by AE Hot Loader. It does not modify `main`.

## Host-visible identity

The After Effects identity remains:

- name: `Stellar Gradient`;
- category: `Stellar`;
- match name: `StellarLabs.StellarGradient`;
- effect API: `13.29`;
- existing parameter IDs, preset behavior and group/collapse metadata remain unchanged.

## Bundle layout

```
StellarGradient.plugin
└── Contents
    ├── MacOS/StellarGradient
    ├── Frameworks/libstellar_gradient_impl.dylib
    └── Resources/StellarGradient.rsrc
```

The bundle executable is a stable C++ shell. The existing Rust/C++/Metal host becomes the implementation dylib.

## Hot-reload ABI

Required implementation exports:

- `EffectMain`;
- `AEHotLoader_ImplementationABI() = 1`;
- `AEHotLoader_ImplementationStateABI() = 1`;
- `AEHotLoader_ImplementationKey() = "stellar-gradient"`;
- `AEHotLoader_ImplementationLabel()`.

The shell validates the complete contract before the atomic pointer swap.

Incompatible parameter, persistent state or GPU-state changes require a new state ABI, rebuilt shell and one AE restart.

## Reload workflow

After the shell has been installed and AE restarted once:

```bash
zsh stellar_gradient_v0.8.6_sdkless_mac/sdkless/tools/stage_hot_reload_macos.command my-build-label
```

Candidate path:

`~/Library/Application Support/AE Hot Loader/implementations/stellar-gradient/current.dylib`

Then click:

`Window → AE Hot Loader → Reload Plugins`

Old implementation images are intentionally retained until AE exits.

## Failure behavior

The previous implementation stays active when the candidate cannot load, misses required exports, has a protocol/state ABI mismatch, or has the wrong implementation key.

## Merge gate

1. existing projects resolve `StellarLabs.StellarGradient`;
2. presets, parameters and collapsed groups are unchanged;
3. CPU Smart Render and MFR still work;
4. Metal setup/render/setdown work through the shell;
5. implementation swap works without AE restart;
6. existing effect instances survive the swap;
7. repeated reload/noop cycles remain stable;
8. invalid candidates do not replace the active implementation.
