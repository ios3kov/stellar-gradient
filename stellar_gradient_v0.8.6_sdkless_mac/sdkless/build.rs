use pipl::*;
use std::process::Command;
use std::env;
use std::path::PathBuf;

const PF_PLUG_IN_VERSION: u16 = 13;
const PF_PLUG_IN_SUBVERS: u16 = 29;

fn hot_reload_lock_fingerprint(manifest_dir: &std::path::Path) -> String {
    let lock_path = manifest_dir.join("Cargo.lock");
    let bytes = std::fs::read(&lock_path)
        .unwrap_or_else(|error| panic!("read {}: {error}", lock_path.display()));
    println!("cargo:rerun-if-changed={}", lock_path.display());

    let mut hash = 1469598103934665603u64;
    for byte in bytes {
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(1099511628211u64);
    }
    format!("{hash:016x}")
}

fn main() {
    let rustc = std::env::var("RUSTC").unwrap_or_else(|_| "rustc".to_string());
    let rustc_version = Command::new(rustc)
        .arg("--version")
        .output()
        .ok()
        .and_then(|out| String::from_utf8(out.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "rustc-unknown".to_string());
    let target = std::env::var("TARGET").unwrap_or_else(|_| "target-unknown".to_string());
    let manifest_dir = std::path::PathBuf::from(
        std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"),
    );
    let lock_fingerprint = hot_reload_lock_fingerprint(&manifest_dir);
    println!(
        "cargo:rustc-env=AE_HOT_LOADER_RUNTIME_ABI={}|{}|after-effects=83dcc93734fd5db1335b6ec83cba7a6505a39dcc|lock={}",
        rustc_version,
        target,
        lock_fingerprint
    );
    println!("cargo:rerun-if-env-changed=AE_HOT_LOADER_IMPL_LABEL");
    // Rust 1.80+ validates cfg names at the destination crate. The
    // after-effects macro expands these cfgs in our crate, so register them
    // explicitly. Also pin MFR on here instead of depending on pipl's
    // build-script side effect; the PiPL flag below advertises the same
    // capability to After Effects.
    for name in [
        "catch_panics",
        "threaded_rendering",
        "smart_render",
        "gpu_render",
        "does_dialog",
        "uses_audio",
        "sends_update_params_ui",
        "with_premiere",
    ] {
        println!("cargo:rustc-check-cfg=cfg({name})");
    }
    println!("cargo:rustc-cfg=threaded_rendering");
    println!("cargo:rustc-cfg=smart_render");
    println!("cargo:rustc-cfg=gpu_render");

    // The AE entry point is a C ABI boundary. Keep Rust panics on the Rust side
    // in optimized builds as well; the after-effects crate wraps EffectMain in
    // catch_unwind when this cfg is enabled.
    println!("cargo:rustc-cfg=catch_panics");

    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let project = root.parent().expect("sdkless must live in project root");

    let mut native = cc::Build::new();
    native.cpp(true)
        .std("c++17")
        .warnings(true)
        .warnings_into_errors(true)
        .flag_if_supported("-fno-fast-math")
        .flag_if_supported("-ffp-contract=off")
        .include(root.join("bridge"))
        .include(project.join("src"))
        .file(root.join("bridge/StellarBridge.cpp"))
        .file(project.join("src/core/RenderPlan.cpp"))
        .file(project.join("src/cpu/ReferenceRenderer.cpp"));

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("macos") {
        native.file(root.join("bridge/MetalBridge.mm"));
        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=framework=Metal");
        println!("cargo:rustc-link-lib=framework=MetalPerformanceShaders");
        println!("cargo:rustc-link-lib=c++");
    } else {
        println!("cargo:rustc-link-lib=stdc++");
    }
    native.compile("stellar_gradient_native");

    for path in [
        "bridge/StellarBridge.h",
        "bridge/StellarBridge.cpp",
        "bridge/MetalBridge.mm",
        "../src/core/Params.h",
        "../src/core/Sanitize.h",
        "../src/core/RenderPlan.h",
        "../src/core/RenderPlan.cpp",
        "../src/cpu/ReferenceRenderer.h",
        "../src/cpu/ReferenceRenderer.cpp",
        "../src/gpu/SharedParams.h",
        "../src/gpu/StellarGradientMetalSource.generated.h",
    ] {
        println!("cargo:rerun-if-changed={path}");
    }

    pipl::plugin_build(vec![
        Property::Kind(PIPLType::AEEffect),
        Property::Name("Stellar Gradient"),
        Property::Category("Stellar"),
        #[cfg(target_os = "windows")]
        Property::CodeWin64X86("EffectMain"),
        #[cfg(target_os = "macos")]
        Property::CodeMacIntel64("EffectMain"),
        #[cfg(target_os = "macos")]
        Property::CodeMacARM64("EffectMain"),
        Property::AE_PiPL_Version { major: 2, minor: 0 },
        Property::AE_Effect_Spec_Version { major: PF_PLUG_IN_VERSION, minor: PF_PLUG_IN_SUBVERS },
        Property::AE_Effect_Version { version: 0, subversion: 8, bugversion: 8, stage: Stage::Beta, build: 16 },
        Property::AE_Effect_Info_Flags(0),
        Property::AE_Effect_Global_OutFlags(
            OutFlags::DeepColorAware | OutFlags::NonParamVary
        ),
        Property::AE_Effect_Global_OutFlags_2(
            OutFlags2::SupportsQueryDynamicFlags |
            OutFlags2::ParamGroupStartCollapsedFlag |
            OutFlags2::RevealsZeroAlpha |
            OutFlags2::SupportsSmartRender |
            OutFlags2::FloatColorAware |
            OutFlags2::SupportsThreadedRendering |
            OutFlags2::SupportsGetFlattenedSequenceData |
            OutFlags2::SupportsGpuRenderF32
        ),
        Property::AE_Effect_Match_Name("StellarLabs.StellarGradient"),
        Property::AE_Reserved_Info(0),
        Property::AE_Effect_Support_URL("https://github.com/ios3kov"),
    ]);
}
