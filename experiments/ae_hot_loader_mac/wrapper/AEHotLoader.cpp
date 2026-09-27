#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string>

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

using CoreEffectMainFn = PF_Err (*)(
    PF_Cmd,
    PF_InData*,
    PF_OutData*,
    PF_ParamDef**,
    PF_LayerDef*,
    void*);

using CoreSetRegisterFn = void (*)(void*);

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

PF_PluginDataPtr g_plugin_data = nullptr;
PF_PluginDataCB2 g_register_callback = nullptr;
SPBasicSuite* g_basic_suite = nullptr;
std::atomic<bool> g_late_registered{false};

std::once_flag g_core_once;
void* g_core_handle = nullptr;
CoreEffectMainFn g_core_effect_main = nullptr;

void Log(const char* message) {
    if (FILE* f = std::fopen("/tmp/ae-hot-loader.log", "a")) {
        std::fprintf(f, "%s\n", message);
        std::fclose(f);
    }
}

std::string CorePath() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&CorePath), &info) == 0 || !info.dli_fname) {
        return {};
    }

    std::string binary_path(info.dli_fname);
    const std::string marker = "/Contents/MacOS/";
    const auto pos = binary_path.rfind(marker);
    if (pos == std::string::npos) {
        return {};
    }

    return binary_path.substr(0, pos) +
           "/Contents/Frameworks/libae_hot_loader_core.dylib";
}

void LoadCore() {
    std::call_once(g_core_once, [] {
        const std::string path = CorePath();
        if (path.empty()) {
            Log("core: unable to resolve bundle path");
            return;
        }

        g_core_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!g_core_handle) {
            const char* error = dlerror();
            std::string msg = "core: dlopen failed: ";
            msg += error ? error : "unknown error";
            Log(msg.c_str());
            return;
        }

        g_core_effect_main = reinterpret_cast<CoreEffectMainFn>(
            dlsym(g_core_handle, "EffectMain"));
        auto set_register = reinterpret_cast<CoreSetRegisterFn>(
            dlsym(g_core_handle, "AEHotLoaderCore_SetRegisterFn"));

        if (!g_core_effect_main || !set_register) {
            Log("core: required export missing");
            g_core_effect_main = nullptr;
            return;
        }

        extern A_Err AEHotLoader_RegisterLateEffect();
        set_register(reinterpret_cast<void*>(&AEHotLoader_RegisterLateEffect));
        Log("core: loaded and callback bridge installed");
    });
}

PF_Err ForwardEffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef** params,
    PF_LayerDef* output,
    void* extra) {
    LoadCore();
    if (!g_core_effect_main) {
        return -1002;
    }
    return g_core_effect_main(cmd, in_data, out_data, params, output, extra);
}

}  // namespace

extern "C" __attribute__((visibility("default")))
A_Err PluginDataEntryFunction2(
    PF_PluginDataPtr in_ptr,
    PF_PluginDataCB2 in_callback,
    SPBasicSuite* in_basic_suite,
    const char* in_host_name,
    const char* in_host_version) {

    g_plugin_data = in_ptr;
    g_register_callback = in_callback;
    g_basic_suite = in_basic_suite;

    char message[512]{};
    std::snprintf(
        message,
        sizeof(message),
        "startup: host=%s version=%s data=%p callback=%p suite=%p",
        in_host_name ? in_host_name : "(null)",
        in_host_version ? in_host_version : "(null)",
        static_cast<void*>(in_ptr),
        reinterpret_cast<void*>(in_callback),
        static_cast<void*>(in_basic_suite));
    Log(message);

    if (!in_callback) {
        Log("startup: registration callback is null");
        return -1003;
    }

    const A_Err result = in_callback(
        in_ptr,
        reinterpret_cast<const std::uint8_t*>("AE Hot Loader"),
        reinterpret_cast<const std::uint8_t*>("OS3KOV.AEHotLoader"),
        reinterpret_cast<const std::uint8_t*>("AE Hot Loader"),
        reinterpret_cast<const std::uint8_t*>("EffectMain"),
        kAEEffectKind,
        kApiMajor,
        kApiMinor,
        kRegistrationReservedInfo,
        reinterpret_cast<const std::uint8_t*>("https://github.com/ios3kov"));

    std::snprintf(message, sizeof(message), "startup: primary registration result=%d", result);
    Log(message);
    return result;
}

extern "C" __attribute__((visibility("default")))
A_Err AEHotLoader_RegisterLateEffect() {
    if (!g_register_callback || !g_plugin_data) {
        Log("late: registration state unavailable");
        return -1004;
    }

    bool expected = false;
    if (!g_late_registered.compare_exchange_strong(expected, true)) {
        Log("late: already registered in this AE process");
        return 0;
    }

    Log("late: invoking saved AE registration callback");

    const A_Err result = g_register_callback(
        g_plugin_data,
        reinterpret_cast<const std::uint8_t*>("AE Hot Loader Late Effect"),
        reinterpret_cast<const std::uint8_t*>("OS3KOV.AEHotLoader.Late"),
        reinterpret_cast<const std::uint8_t*>("AE Hot Loader"),
        reinterpret_cast<const std::uint8_t*>("LateEffectMain"),
        kAEEffectKind,
        kApiMajor,
        kApiMinor,
        kRegistrationReservedInfo,
        reinterpret_cast<const std::uint8_t*>("https://github.com/ios3kov"));

    char message[256]{};
    std::snprintf(message, sizeof(message), "late: callback result=%d", result);
    Log(message);

    if (result != 0) {
        g_late_registered.store(false);
    }
    return result;
}

extern "C" __attribute__((visibility("default")))
PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef** params,
    PF_LayerDef* output,
    void* extra) {
    return ForwardEffectMain(cmd, in_data, out_data, params, output, extra);
}

extern "C" __attribute__((visibility("default")))
PF_Err LateEffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef** params,
    PF_LayerDef* output,
    void* extra) {
    return ForwardEffectMain(cmd, in_data, out_data, params, output, extra);
}
