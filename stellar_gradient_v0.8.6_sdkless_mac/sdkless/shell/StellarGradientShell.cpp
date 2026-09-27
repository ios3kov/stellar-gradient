#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
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
constexpr std::uint32_t kImplementationAbi = 1;
constexpr std::uint64_t kImplementationStateAbi = 1;
constexpr const char* kImplementationKey = "stellar-gradient";

std::atomic<ImplEffectMainFn> g_effect_main{nullptr};
std::mutex g_reload_mutex;
std::shared_mutex g_call_gate;
std::vector<void*> g_loaded_handles;
std::string g_loaded_source;
std::uint64_t g_loaded_fingerprint = 0;
std::uint64_t g_reload_ordinal = 0;

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
    std::filesystem::path root =
        std::filesystem::path("/private/tmp/AEHotLoaderShell") /
        std::to_string(pid);
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        return {};
    }

    const std::filesystem::path destination =
        root / ("stellar-gradient-" + std::to_string(ordinal) + ".dylib");

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

int LoadImplementationFromSourceLocked(
    const std::string& source,
    bool force,
    std::string* detail) {

    if (source.empty()) {
        if (detail) *detail = "No implementation dylib found.";
        return -4101;
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

    void* handle = dlopen(runtime_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* error = dlerror();
        if (detail) {
            *detail = std::string("dlopen failed: ") + (error ? error : "unknown");
        }
        return -4104;
    }

    auto effect_main = reinterpret_cast<ImplEffectMainFn>(dlsym(handle, "EffectMain"));
    if (!effect_main) {
        const char* error = dlerror();
        if (detail) {
            *detail = std::string("EffectMain missing: ") + (error ? error : "unknown");
        }
        g_loaded_handles.push_back(handle);
        return -4105;
    }

    auto abi_fn = reinterpret_cast<ImplAbiFn>(
        dlsym(handle, "AEHotLoader_ImplementationABI"));
    auto state_abi_fn = reinterpret_cast<ImplStateAbiFn>(
        dlsym(handle, "AEHotLoader_ImplementationStateABI"));
    auto key_fn = reinterpret_cast<ImplKeyFn>(
        dlsym(handle, "AEHotLoader_ImplementationKey"));

    if (!abi_fn || !state_abi_fn || !key_fn) {
        if (detail) *detail = "Implementation hot-reload ABI exports are missing.";
        g_loaded_handles.push_back(handle);
        return -4108;
    }

    if (abi_fn() != kImplementationAbi) {
        if (detail) *detail = "Implementation protocol ABI mismatch.";
        g_loaded_handles.push_back(handle);
        return -4109;
    }

    if (state_abi_fn() != kImplementationStateAbi) {
        if (detail) {
            *detail =
                "Implementation state/schema ABI mismatch; AE restart with a rebuilt shell is required.";
        }
        g_loaded_handles.push_back(handle);
        return -4110;
    }

    char key_buffer[256]{};
    if (key_fn(key_buffer, sizeof(key_buffer)) != 0 ||
        std::strcmp(key_buffer, kImplementationKey) != 0) {
        if (detail) {
            *detail = std::string("Implementation key mismatch: expected ") +
                      kImplementationKey + ", got " +
                      (key_buffer[0] ? key_buffer : "(invalid)");
        }
        g_loaded_handles.push_back(handle);
        return -4111;
    }

    std::string implementation_label = "(unknown)";
    auto label_fn = reinterpret_cast<ImplLabelFn>(
        dlsym(handle, "AEHotLoader_ImplementationLabel"));
    if (label_fn) {
        char label_buffer[256]{};
        if (label_fn(label_buffer, sizeof(label_buffer)) == 0 && label_buffer[0] != '\0') {
            implementation_label = label_buffer;
        }
    }

    // Quiescent swap: let already-running MFR/render calls finish and prevent
    // a mix of old/new implementation code from touching the same AE state.
    {
        std::unique_lock<std::shared_mutex> call_lock(g_call_gate);
        g_loaded_handles.push_back(handle);
        g_loaded_source = source;
        g_loaded_fingerprint = fingerprint;
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
    return LoadImplementationFromSourceLocked(SelectSource(), force, detail);
}

int EnsureImplementationLoaded(std::string* detail) {
    std::lock_guard<std::mutex> lock(g_reload_mutex);
    if (g_effect_main.load(std::memory_order_acquire) != nullptr) {
        if (detail) *detail = "Implementation already active.";
        return 1;
    }
    return LoadImplementationFromSourceLocked(SelectSource(), true, detail);
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

    // Shared gate keeps MFR calls concurrent with each other, but a reload
    // waits for all in-flight calls before publishing a new EffectMain.
    std::shared_lock<std::shared_mutex> call_lock(g_call_gate);
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
        std::string detail;
        const int result = LoadImplementation(false, &detail);
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
