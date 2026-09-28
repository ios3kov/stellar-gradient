#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using A_Err = std::int32_t;
using A_long = std::int32_t;
using PF_Err = std::int32_t;
using PF_Cmd = std::int32_t;

struct PF_PluginData;
using PF_PluginDataPtr = PF_PluginData*;
struct SPBasicSuite;
struct PF_InData;
struct PF_OutData;
struct PF_ParamDef;
struct PF_LayerDef;

using PF_PluginDataCB2 = A_Err (*)(
    PF_PluginDataPtr,
    const std::uint8_t*,
    const std::uint8_t*,
    const std::uint8_t*,
    const std::uint8_t*,
    A_long,
    A_long,
    A_long,
    A_long,
    const std::uint8_t*);

using ImplLabelFn = int (*)(char*, std::size_t);
using ImplAbiFn = std::uint32_t (*)();
using ImplStateAbiFn = std::uint64_t (*)();
using ImplKeyFn = int (*)(char*, std::size_t);
using ImplRuntimeAbiFn = int (*)(char*, std::size_t);
using ImplSetGenerationFn = void (*)(std::uint64_t);

using ImplEffectMainFn = PF_Err (*)(
    PF_Cmd,
    PF_InData*,
    PF_OutData*,
    PF_ParamDef**,
    PF_LayerDef*,
    void*);

namespace {

constexpr A_long FourCC(char a, char b, char c, char d) {
    return (static_cast<A_long>(static_cast<std::uint8_t>(a)) << 24) |
           (static_cast<A_long>(static_cast<std::uint8_t>(b)) << 16) |
           (static_cast<A_long>(static_cast<std::uint8_t>(c)) << 8) |
            static_cast<A_long>(static_cast<std::uint8_t>(d));
}

constexpr A_long kAEEffectKind = FourCC('e', 'F', 'K', 'T');
constexpr A_long kApiMajor = 13;
constexpr A_long kApiMinor = 29;
constexpr A_long kRegistrationReservedInfo = 8;
constexpr std::uint32_t kShellAbi = 1;
constexpr std::uint32_t kImplementationAbi = 2;
constexpr std::uint64_t kImplementationStateAbi = 3;
constexpr const char* kImplementationKey = "stellar-gradient";
constexpr std::size_t kMaxImplementationGenerations = 64;

std::atomic<ImplEffectMainFn> g_effect_main{nullptr};
std::mutex g_reload_mutex;
std::atomic<std::uint32_t> g_active_calls{0};
std::atomic<bool> g_swap_pending{false};
std::vector<void*> g_loaded_handles;
std::string g_loaded_source;
std::uint64_t g_loaded_fingerprint = 0;
std::uint64_t g_reload_ordinal = 0;
std::string g_runtime_abi;


class EffectCallGuard {
public:
    EffectCallGuard() {
        for (;;) {
            while (g_swap_pending.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            g_active_calls.fetch_add(1, std::memory_order_acq_rel);
            if (!g_swap_pending.load(std::memory_order_acquire)) {
                active_ = true;
                return;
            }

            g_active_calls.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    ~EffectCallGuard() {
        if (active_) {
            g_active_calls.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    EffectCallGuard(const EffectCallGuard&) = delete;
    EffectCallGuard& operator=(const EffectCallGuard&) = delete;

private:
    bool active_ = false;
};

class SwapPendingGuard {
public:
    explicit SwapPendingGuard(bool active) : active_(active) {}
    ~SwapPendingGuard() {
        if (active_) {
            g_swap_pending.store(false, std::memory_order_release);
        }
    }

    SwapPendingGuard(const SwapPendingGuard&) = delete;
    SwapPendingGuard& operator=(const SwapPendingGuard&) = delete;

private:
    bool active_ = false;
};

void Log(const std::string& message) {
    if (FILE* f = std::fopen("/tmp/ae-hot-loader-stellar-gradient-shell.log", "a")) {
        std::fprintf(f, "%s\n", message.c_str());
        std::fclose(f);
    }
}

std::string BundleRoot() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&BundleRoot), &info) == 0 || !info.dli_fname) {
        return {};
    }

    std::string binary_path(info.dli_fname);
    const std::string marker = "/Contents/MacOS/";
    const auto pos = binary_path.rfind(marker);
    if (pos == std::string::npos) {
        return {};
    }
    return binary_path.substr(0, pos);
}

std::string DefaultImplementationPath() {
    const std::string bundle = BundleRoot();
    if (bundle.empty()) {
        return {};
    }
    return bundle + "/Contents/Frameworks/libstellar_gradient_impl.dylib";
}

std::string ExternalImplementationPath() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        return {};
    }
    return std::string(home) +
           "/Library/Application Support/AE Hot Loader/implementations/stellar-gradient/current.dylib";
}

bool FileFingerprint(const std::string& path, std::uint64_t* fingerprint) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    // FNV-1a 64-bit. This is change detection, not a security primitive.
    std::uint64_t hash = 1469598103934665603ULL;
    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<std::uint8_t>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    if (input.bad()) {
        return false;
    }

    *fingerprint = hash;
    return true;
}

std::string SelectSource() {
    const std::string external = ExternalImplementationPath();
    std::uint64_t ignored = 0;
    if (!external.empty() && FileFingerprint(external, &ignored)) {
        return external;
    }
    return DefaultImplementationPath();
}

std::string RuntimeCopyPath(const std::string& source) {
    const auto pid = static_cast<unsigned long long>(getpid());
    const auto ordinal = ++g_reload_ordinal;
    const char* tmpdir = std::getenv("TMPDIR");
    const std::filesystem::path temp_root =
        (tmpdir && *tmpdir) ? std::filesystem::path(tmpdir)
                            : std::filesystem::path("/private/tmp");
    std::filesystem::path root =
        temp_root / "AEHotLoaderShell" / std::to_string(pid);
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        return {};
    }

    const auto image_identity =
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(&BundleRoot));
    const std::filesystem::path destination =
        root / (
            "stellar-gradient-" + std::to_string(image_identity) + "-" +
            std::to_string(ordinal) + ".dylib");

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        return {};
    }
    return destination.string();
}


void RetainRejectedCandidate(void* handle, const std::string& runtime_path) {
    if (handle) {
        // A successful dlopen may have executed static initializers. Never run
        // unload/destructor code inside a live AE process; retain the image
        // until process exit. Rejected generations count toward the safety cap.
        g_loaded_handles.push_back(handle);
        return;
    }

    std::error_code remove_error;
    std::filesystem::remove(runtime_path, remove_error);
}

int LoadImplementationFromSourceLocked(
    const std::string& source,
    bool force,
    std::string* detail) {

    if (source.empty()) {
        if (detail) *detail = "No implementation dylib found.";
        return -4101;
    }

    // Fast path: do not even map a candidate while an EffectMain call is
    // already in flight. The final generation gate below still handles races.
    if (g_active_calls.load(std::memory_order_acquire) != 0 ||
        g_swap_pending.load(std::memory_order_acquire)) {
        if (detail) {
            *detail = "Effect is busy with an in-flight call; retry Reload Plugins.";
        }
        return -4112;
    }

    const std::string runtime_path = RuntimeCopyPath(source);
    if (runtime_path.empty()) {
        if (detail) *detail = "Could not stage implementation dylib.";
        return -4103;
    }

    std::uint64_t fingerprint = 0;
    if (!FileFingerprint(runtime_path, &fingerprint)) {
        std::error_code remove_error;
        std::filesystem::remove(runtime_path, remove_error);
        if (detail) *detail = "Could not fingerprint staged implementation dylib.";
        return -4102;
    }

    if (!force &&
        fingerprint == g_loaded_fingerprint &&
        g_effect_main.load(std::memory_order_acquire) != nullptr) {
        std::error_code remove_error;
        std::filesystem::remove(runtime_path, remove_error);
        if (detail) *detail = "Stellar Gradient implementation unchanged.";
        return 1;
    }

    if (g_loaded_handles.size() >= kMaxImplementationGenerations) {
        std::error_code remove_error;
        std::filesystem::remove(runtime_path, remove_error);
        if (detail) {
            *detail =
                "Hot-reload generation limit reached; restart After Effects before loading more builds.";
        }
        return -4114;
    }

    void* handle = dlopen(runtime_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* error = dlerror();
        if (detail) {
            *detail = std::string("dlopen failed: ") + (error ? error : "unknown");
        }
        RetainRejectedCandidate(nullptr, runtime_path);
        return -4104;
    }

    auto effect_main = reinterpret_cast<ImplEffectMainFn>(dlsym(handle, "EffectMain"));
    if (!effect_main) {
        const char* error = dlerror();
        if (detail) {
            *detail = std::string("EffectMain missing: ") + (error ? error : "unknown");
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4105;
    }

    auto abi_fn = reinterpret_cast<ImplAbiFn>(
        dlsym(handle, "AEHotLoader_ImplementationABI"));
    auto state_abi_fn = reinterpret_cast<ImplStateAbiFn>(
        dlsym(handle, "AEHotLoader_ImplementationStateABI"));
    auto key_fn = reinterpret_cast<ImplKeyFn>(
        dlsym(handle, "AEHotLoader_ImplementationKey"));
    auto label_fn = reinterpret_cast<ImplLabelFn>(
        dlsym(handle, "AEHotLoader_ImplementationLabel"));
    auto runtime_abi_fn = reinterpret_cast<ImplRuntimeAbiFn>(
        dlsym(handle, "AEHotLoader_ImplementationRuntimeABI"));
    auto set_generation_fn = reinterpret_cast<ImplSetGenerationFn>(
        dlsym(handle, "AEHotLoader_SetGeneration"));

    if (!abi_fn || !state_abi_fn || !key_fn || !label_fn || !runtime_abi_fn ||
        !set_generation_fn) {
        if (detail) *detail = "Implementation hot-reload ABI exports are missing.";
        RetainRejectedCandidate(handle, runtime_path);
        return -4108;
    }

    if (abi_fn() != kImplementationAbi) {
        if (detail) *detail = "Implementation protocol ABI mismatch.";
        RetainRejectedCandidate(handle, runtime_path);
        return -4109;
    }

    if (state_abi_fn() != kImplementationStateAbi) {
        if (detail) {
            *detail =
                "Implementation state/schema ABI mismatch; AE restart with a rebuilt shell is required.";
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4110;
    }

    char key_buffer[256]{};
    const int key_result = key_fn(key_buffer, sizeof(key_buffer));
    const auto* key_terminator = static_cast<const char*>(
        std::memchr(key_buffer, '\0', sizeof(key_buffer)));
    if (key_result != 0 || !key_terminator) {
        if (detail) {
            *detail = "Implementation key is invalid or not NUL-terminated.";
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4111;
    }
    if (std::strcmp(key_buffer, kImplementationKey) != 0) {
        if (detail) {
            *detail = std::string("Implementation key mismatch: expected ") +
                      kImplementationKey + ", got " + key_buffer;
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4111;
    }

    char label_buffer[256]{};
    const int label_result = label_fn(label_buffer, sizeof(label_buffer));
    const auto* label_terminator = static_cast<const char*>(
        std::memchr(label_buffer, '\0', sizeof(label_buffer)));
    if (label_result != 0 || !label_terminator || label_buffer[0] == '\0') {
        if (detail) {
            *detail = "Implementation label is invalid or not NUL-terminated.";
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4113;
    }
    const std::string implementation_label(
        label_buffer,
        static_cast<std::size_t>(label_terminator - label_buffer));


    char runtime_abi_buffer[512]{};
    const int runtime_abi_result =
        runtime_abi_fn(runtime_abi_buffer, sizeof(runtime_abi_buffer));
    const auto* runtime_abi_terminator = static_cast<const char*>(
        std::memchr(runtime_abi_buffer, '\0', sizeof(runtime_abi_buffer)));
    if (runtime_abi_result != 0 ||
        !runtime_abi_terminator ||
        runtime_abi_buffer[0] == '\0') {
        if (detail) {
            *detail = "Implementation runtime ABI is invalid or not NUL-terminated.";
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4115;
    }
    const std::string runtime_abi(
        runtime_abi_buffer,
        static_cast<std::size_t>(runtime_abi_terminator - runtime_abi_buffer));

    if (!g_runtime_abi.empty() && runtime_abi != g_runtime_abi) {
        if (detail) {
            *detail =
                "Implementation Rust runtime ABI mismatch; rebuild with the same pinned toolchain or restart with a matching shell.";
        }
        RetainRejectedCandidate(handle, runtime_path);
        return -4116;
    }

    // Quiescent swap: let already-running MFR/render calls finish and prevent
    // a mix of old/new implementation code from touching the same AE state.
    {
        bool expected = false;
        if (!g_swap_pending.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            RetainRejectedCandidate(handle, runtime_path);
            if (detail) {
                *detail = "Another implementation swap is already pending; retry Reload Plugins.";
            }
            return -4112;
        }

        SwapPendingGuard swap_guard(true);

        if (g_active_calls.load(std::memory_order_acquire) != 0) {
            RetainRejectedCandidate(handle, runtime_path);
            if (detail) {
                *detail = "Effect is busy with an in-flight call; retry Reload Plugins.";
            }
            return -4112;
        }

        // Generation is assigned by the shell from the exact staged dylib
        // bytes, so it changes whenever executable code changes even if the
        // human-readable build label is reused.
        set_generation_fn(fingerprint);

        g_loaded_handles.push_back(handle);
        g_loaded_source = source;
        g_loaded_fingerprint = fingerprint;
        if (g_runtime_abi.empty()) {
            g_runtime_abi = runtime_abi;
        }
        g_effect_main.store(effect_main, std::memory_order_release);
    }

    if (detail) {
        *detail = "Reloaded Stellar Gradient implementation " + implementation_label +
                  " from " + source;
    }
    Log("active implementation: " + runtime_path + " source=" + source);
    return 0;
}

int LoadImplementation(bool force, std::string* detail) {
    std::lock_guard<std::mutex> lock(g_reload_mutex);

    bool initialized_baseline = false;
    std::string baseline_detail;

    // A Reload click may happen before AE has ever called EffectMain for this
    // shell. Establish the process/runtime ABI from the bundled implementation
    // first so an arbitrary stale current.dylib can never define compatibility.
    if (g_effect_main.load(std::memory_order_acquire) == nullptr) {
        const int baseline_result = LoadImplementationFromSourceLocked(
            DefaultImplementationPath(),
            true,
            &baseline_detail);
        if (baseline_result < 0) {
            if (detail) {
                *detail = "Bundled implementation baseline failed: " + baseline_detail;
            }
            return baseline_result;
        }
        initialized_baseline = baseline_result == 0;
    }

    const std::string bundled_source = DefaultImplementationPath();
    const std::string source = SelectSource();
    if (source == bundled_source) {
        if (initialized_baseline) {
            if (detail) *detail = baseline_detail;
            return 0;
        }

        if (g_loaded_source == bundled_source) {
            if (detail) {
                *detail = "Bundled implementation active; no external candidate staged.";
            }
            return 1;
        }

        // Removing current.dylib is an explicit rollback request. Reload the
        // bundled implementation rather than merely changing the status text.
        return LoadImplementationFromSourceLocked(
            bundled_source,
            false,
            detail);
    }

    return LoadImplementationFromSourceLocked(source, force, detail);
}

int EnsureImplementationLoaded(std::string* detail) {
    std::lock_guard<std::mutex> lock(g_reload_mutex);
    if (g_effect_main.load(std::memory_order_acquire) != nullptr) {
        if (detail) *detail = "Implementation already active.";
        return 1;
    }

    // The bundled implementation is the only trusted process baseline.
    // A stale external current.dylib must never establish the Rust/state ABI
    // merely because it was left on disk from an earlier session.
    return LoadImplementationFromSourceLocked(
        DefaultImplementationPath(),
        true,
        detail);
}

int LoadBundledImplementation(std::string* detail) {
    std::lock_guard<std::mutex> lock(g_reload_mutex);
    if (g_effect_main.load(std::memory_order_acquire) != nullptr) {
        if (detail) *detail = "Implementation already active.";
        return 1;
    }
    return LoadImplementationFromSourceLocked(
        DefaultImplementationPath(),
        true,
        detail);
}

PF_Err ForwardEffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef** params,
    PF_LayerDef* output,
    void* extra) {

    if (g_effect_main.load(std::memory_order_acquire) == nullptr) {
        std::string detail;
        const int load_result = EnsureImplementationLoaded(&detail);
        Log("initial implementation load result=" + std::to_string(load_result) + " " + detail);

        if (load_result < 0 && g_effect_main.load(std::memory_order_acquire) == nullptr) {
            std::string fallback_detail;
            const int fallback_result = LoadBundledImplementation(&fallback_detail);
            Log(
                "bundled fallback result=" + std::to_string(fallback_result) +
                " " + fallback_detail);
        }
    }

    // Reentrant-safe call guard: MFR calls stay concurrent. A reload publishes
    // a new pointer only when no EffectMain call is active.
    EffectCallGuard call_guard;
    ImplEffectMainFn fn = g_effect_main.load(std::memory_order_acquire);
    if (!fn) {
        return -4106;
    }
    return fn(cmd, in_data, out_data, params, output, extra);
}

void CopyMessage(char* output, std::size_t capacity, const std::string& message) {
    if (!output || capacity == 0) {
        return;
    }
    std::snprintf(output, capacity, "%s", message.c_str());
}

}  // namespace

extern "C" __attribute__((visibility("default")))
A_Err PluginDataEntryFunction2(
    PF_PluginDataPtr in_ptr,
    PF_PluginDataCB2 in_callback,
    SPBasicSuite* in_basic_suite,
    const char* in_host_name,
    const char* in_host_version) {

    try {
        (void)in_basic_suite;

        if (!in_callback) {
            return -4107;
        }

        const A_Err result = in_callback(
            in_ptr,
            reinterpret_cast<const std::uint8_t*>("Stellar Gradient"),
            reinterpret_cast<const std::uint8_t*>("StellarLabs.StellarGradient"),
            reinterpret_cast<const std::uint8_t*>("Stellar"),
            reinterpret_cast<const std::uint8_t*>("EffectMain"),
            kAEEffectKind,
            kApiMajor,
            kApiMinor,
            kRegistrationReservedInfo,
            reinterpret_cast<const std::uint8_t*>("https://github.com/ios3kov"));

        char message[512]{};
        std::snprintf(
            message,
            sizeof(message),
            "shell startup host=%s version=%s registration=%d",
            in_host_name ? in_host_name : "(null)",
            in_host_version ? in_host_version : "(null)",
            result);
        Log(message);

        return result;
    } catch (...) {
        return -4190;
    }
}

extern "C" __attribute__((visibility("default")))
PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef** params,
    PF_LayerDef* output,
    void* extra) {

    try {
        return ForwardEffectMain(cmd, in_data, out_data, params, output, extra);
    } catch (...) {
        return -4191;
    }
}

extern "C" __attribute__((visibility("default")))
int AEHotLoader_ShellReload(char* output, std::size_t output_capacity) {
    try {
        bool initialized_baseline = false;
        std::string baseline_detail;

        if (g_effect_main.load(std::memory_order_acquire) == nullptr) {
            const int baseline_result =
                EnsureImplementationLoaded(&baseline_detail);
            if (baseline_result < 0) {
                CopyMessage(output, output_capacity, baseline_detail);
                Log(
                    "reload baseline failed=" +
                    std::to_string(baseline_result) + " " +
                    baseline_detail);
                return baseline_result;
            }
            initialized_baseline = baseline_result == 0;
        }

        std::string detail;
        int result = LoadImplementation(false, &detail);

        // If Reload itself had to establish the bundled baseline and there is
        // no external change, report that initialization as the successful
        // action. Later identical reloads still return unchanged=1.
        if (initialized_baseline && result == 1) {
            result = 0;
            detail = baseline_detail;
        }

        CopyMessage(output, output_capacity, detail);
        Log("reload result=" + std::to_string(result) + " " + detail);
        return result;
    } catch (...) {
        if (output && output_capacity > 0) {
            std::snprintf(output, output_capacity, "%s", "Unhandled shell exception.");
        }
        return -4192;
    }
}


extern "C" __attribute__((visibility("default")))
std::uint32_t AEHotLoader_ShellABI() {
    return kShellAbi;
}

extern "C" __attribute__((visibility("default")))
int AEHotLoader_ShellKey(char* output, std::size_t output_capacity) {
    try {
        CopyMessage(output, output_capacity, kImplementationKey);
        return (output && output_capacity > 0) ? 0 : -1;
    } catch (...) {
        return -4193;
    }
}
