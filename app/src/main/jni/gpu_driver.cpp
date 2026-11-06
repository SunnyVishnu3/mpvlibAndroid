#include <jni.h>
#include <dlfcn.h>
#include <string>
#include <android/log.h>
#include <vulkan/vulkan.h>

#include "jni_utils.h"
#include "log.h"
#include "gpu_driver.h"

#define ADRENOTOOLS_DRIVER_CUSTOM 1
#define ADRENOTOOLS_DRIVER_FILE_REDIRECT 2

typedef void *(*fn_adrenotools_open_libvulkan)(
    int dlopenMode,
    int featureFlags,
    const char *tmpLibDir,
    const char *hookLibDir,
    const char *customDriverDir,
    const char *customDriverName,
    const char *fileRedirectDir,
    void **userMappingHandle
);

typedef bool (*fn_adrenotools_set_freedreno_env)(
    const char *varName,
    const char *value
);

static void *g_adrenotools_handle = nullptr;
static PFN_vkGetInstanceProcAddr g_custom_vkGetInstanceProcAddr = nullptr;

extern "C" {

jni_func(jboolean, setCustomGpuDriver, jstring hookLibDir, jstring customDriverDir, jstring customDriverName, jstring fileRedirectDir) {
#if defined(__aarch64__)
    std::string hook_dir, driver_dir, driver_name, redirect_dir;
    if (!jstring_to_utf8(env, hookLibDir, &hook_dir)) {
        ALOGE("setCustomGpuDriver: failed to get hookLibDir");
        return JNI_FALSE;
    }
    if (customDriverDir) jstring_to_utf8(env, customDriverDir, &driver_dir);
    if (customDriverName) jstring_to_utf8(env, customDriverName, &driver_name);
    if (fileRedirectDir) jstring_to_utf8(env, fileRedirectDir, &redirect_dir);

    // Dynamically load libadrenotools.so from the app's nativeLibraryDir
    std::string adrenotools_path = hook_dir;
    if (!adrenotools_path.empty() && adrenotools_path.back() != '/')
        adrenotools_path += '/';
    adrenotools_path += "libadrenotools.so";

    void *adrenotools_lib = dlopen(adrenotools_path.c_str(), RTLD_NOW);
    if (!adrenotools_lib) {
        // Fallback to default search path
        adrenotools_lib = dlopen("libadrenotools.so", RTLD_NOW);
    }
    if (!adrenotools_lib) {
        ALOGW("setCustomGpuDriver: libadrenotools.so not loaded: %s", dlerror());
        return JNI_FALSE;
    }

    auto open_libvulkan = reinterpret_cast<fn_adrenotools_open_libvulkan>(
        dlsym(adrenotools_lib, "adrenotools_open_libvulkan"));
    if (!open_libvulkan) {
        ALOGE("setCustomGpuDriver: adrenotools_open_libvulkan symbol not found");
        return JNI_FALSE;
    }

    int featureFlags = 0;
    const char *redirect_dir_ptr = nullptr;
    if (!redirect_dir.empty()) {
        featureFlags |= ADRENOTOOLS_DRIVER_FILE_REDIRECT;
        redirect_dir_ptr = redirect_dir.c_str();
    }

    if (!driver_name.empty() && !driver_dir.empty()) {
        featureFlags |= ADRENOTOOLS_DRIVER_CUSTOM;
        ALOGI("setCustomGpuDriver: loading custom GPU driver %s from %s", driver_name.c_str(), driver_dir.c_str());
        g_adrenotools_handle = open_libvulkan(
            RTLD_NOW,
            featureFlags,
            nullptr,
            hook_dir.c_str(),
            driver_dir.c_str(),
            driver_name.c_str(),
            redirect_dir_ptr,
            nullptr
        );
    } else {
        ALOGI("setCustomGpuDriver: loading default driver via adrenotools");
        g_adrenotools_handle = open_libvulkan(
            RTLD_NOW,
            featureFlags,
            nullptr,
            hook_dir.c_str(),
            nullptr,
            nullptr,
            redirect_dir_ptr,
            nullptr
        );
    }

    if (!g_adrenotools_handle) {
        ALOGE("setCustomGpuDriver: adrenotools_open_libvulkan failed");
        return JNI_FALSE;
    }

    g_custom_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(g_adrenotools_handle, "vkGetInstanceProcAddr"));

    if (!g_custom_vkGetInstanceProcAddr) {
        ALOGE("setCustomGpuDriver: failed to find vkGetInstanceProcAddr in hooked libvulkan");
        return JNI_FALSE;
    }

    ALOGI("setCustomGpuDriver: custom Vulkan loader hook successfully initialized!");
    return JNI_TRUE;
#else
    ALOGW("setCustomGpuDriver: Adreno Tools custom drivers are only supported on arm64-v8a");
    return JNI_FALSE;
#endif
}

jni_func(jboolean, setFreedrenoEnv, jstring name, jstring value) {
#if defined(__aarch64__)
    std::string var_name, var_value;
    if (!jstring_to_utf8(env, name, &var_name) || !jstring_to_utf8(env, value, &var_value))
        return JNI_FALSE;

    void *adrenotools_lib = dlopen("libadrenotools.so", RTLD_NOW);
    if (!adrenotools_lib) return JNI_FALSE;

    auto set_env = reinterpret_cast<fn_adrenotools_set_freedreno_env>(
        dlsym(adrenotools_lib, "adrenotools_set_freedreno_env"));
    if (!set_env) return JNI_FALSE;

    return set_env(var_name.c_str(), var_value.c_str()) ? JNI_TRUE : JNI_FALSE;
#else
    return JNI_FALSE;
#endif
}

PFN_vkGetInstanceProcAddr get_adrenotools_vk_proc_addr(void) {
    return g_custom_vkGetInstanceProcAddr;
}

}

