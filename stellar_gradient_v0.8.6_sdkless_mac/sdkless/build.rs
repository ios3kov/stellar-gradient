use pipl::*;
use std::env;
use std::path::PathBuf;
use std::process::Command;

const PF_PLUG_IN_VERSION: u16 = 13;
const PF_PLUG_IN_SUBVERS: u16 = 29;

fn main() {
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

    // OUT_DIR is generated build data; canonical source files stay unchanged.
    // The existing Mac build toolchain already requires Python 3.
    let version = env::var("CARGO_PKG_VERSION").expect("Cargo package version");
    let target = env::var("TARGET").expect("Cargo target");
    let out_dir = env::var("OUT_DIR").expect("Cargo output directory");
    let identity = Command::new("python3")
        .arg(project.join("tools/build_identity.py"))
        .arg("--crate").arg(&root)
        .arg("--version").arg(&version)
        .arg("--target").arg(&target)
        .arg("--out").arg(&out_dir)
        .output().expect("Python 3 is required for build identity");
    assert!(identity.status.success(), "{}", String::from_utf8_lossy(&identity.stderr));
    print!("{}", String::from_utf8(identity.stdout).expect("UTF-8 build identity"));
    for key in ["SG_REQUIRE_CLEAN_BUILD", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"] {
        println!("cargo:rerun-if-env-changed={key}");
    }
    // A deliberately nonexistent path forces provenance to be re-evaluated on
    // incremental builds too: a changed Git HEAD must never retain an old ID.
    println!("cargo:rerun-if-changed={}/stellar-identity-always-check", out_dir);
    println!("cargo:rerun-if-changed=../tools/build_identity.py");
    println!("cargo:rerun-if-changed=Cargo.lock");


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
        Property::AE_Effect_Version {
            version: env::var("CARGO_PKG_VERSION_MAJOR").unwrap().parse().unwrap(),
            subversion: env::var("CARGO_PKG_VERSION_MINOR").unwrap().parse().unwrap(),
            bugversion: env::var("CARGO_PKG_VERSION_PATCH").unwrap().parse().unwrap(),
            stage: Stage::Beta, build: 1,
        },
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
