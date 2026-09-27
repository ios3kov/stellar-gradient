# Stellar Gradient — AE Hot Loader shell adapter

Branch: `feature/hot-loader-shell`.

Public After Effects identity is unchanged:

- Name: `Stellar Gradient`
- Match name: `StellarLabs.StellarGradient`
- Category: `Stellar`
- AE effect API: `13.29`

## Bundle layout

```
StellarGradient.plugin
└── Contents
    ├── MacOS/StellarGradient                    # stable C++ shell
    ├── Frameworks/StellarGradientImpl.dylib    # Rust implementation
    └── Resources/StellarGradient.rsrc           # existing PiPL
```

The shell owns the AE bundle entry points and forwards every `EffectMain` command to the active Rust implementation.

The implementation continues to own all existing parameters, presets, Smart Render, MFR and Metal behavior.

## Hot reload path

`~/Library/Application Support/AE Hot Loader/implementations/stellar-gradient/current.dylib`

Build and stage while AE stays open:

```bash
zsh sdkless/tools/stage_hot_reload_macos.command
```

Then click **Reload Plugins** in the AE Hot Loader panel.

A successful swap is atomic from the shell's point of view: the new function pointer is published only after runtime copy, `dlopen`, and `EffectMain` resolution succeed.

Old implementation images remain loaded until AE exits.

## First install

Installing this shell version requires one normal AE restart so the host registers the stable shell. Implementation-only updates after that are intended to reload without restarting AE.

## Merge gate

Before this branch can merge:

1. original match name and parameter order are unchanged;
2. all preset values and collapsed-group behavior stay unchanged;
3. existing projects open correctly;
4. CPU Smart Render works through the shell;
5. Metal/GPU setup, render and setdown work through the shell;
6. implementation swap activates without AE restart;
7. existing effect instances render after the swap;
8. repeated reload/noop cycles remain stable;
9. failed candidate loading leaves the previous implementation active.
