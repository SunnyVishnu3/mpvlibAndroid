#include <jni.h>
#include <mutex>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"
#include "render.h"

extern "C" {
    jni_func(void, attachSurface, jobject surface_);
    jni_func(void, replaceSurface, jobject surface_);
    jni_func(void, detachSurface);
    jni_func(void, attachSubtitleSurface, jobject surface_);
    jni_func(void, replaceSubtitleSurface, jobject surface_);
    jni_func(void, detachSubtitleSurface);
};

static jobject surface;
static jobject subtitle_surface;
static std::mutex surface_mutex;

// Set wid via mpv_set_option (used during init before property is available)
static bool set_wid(mpv_handle *handle, int64_t wid)
{
    int result = mpv_set_option(handle, "wid", MPV_FORMAT_INT64, &wid);
    if (result < 0)
        ALOGE("mpv_set_option(wid) returned error %s", mpv_error_string(result));
    return result >= 0;
}

// Set an mpv surface property by name (used for hot-swap and subtitle surface)
static bool set_surface_property(const char *property, int64_t wid)
{
    int result = mpv_set_property(g_mpv, property, MPV_FORMAT_INT64, &wid);
    if (result < 0)
        ALOGE("mpv_set_property(%s) returned error %s", property, mpv_error_string(result));
    return result >= 0;
}

static void clear_surface(JNIEnv *env, jobject *target)
{
    if (!*target)
        return;
    env->DeleteGlobalRef(*target);
    *target = NULL;
}

// Generic surface updater: promotes surface_ to a global ref, sets the given
// mpv property, and swaps out the old global ref.
static void update_surface(JNIEnv *env, mpv_handle *handle, jobject surface_,
                           jobject *target, const char *property)
{
    if (!surface_) {
        throw_java_exception(env, "invalid surface provided");
        return;
    }

    jobject next_surface = env->NewGlobalRef(surface_);
    if (!next_surface) {
        if (!env->ExceptionCheck())
            throw_java_exception(env, "failed to retain surface");
        return;
    }

    int64_t wid = reinterpret_cast<intptr_t>(next_surface);
    bool ok = (handle != nullptr)
        ? set_wid(handle, wid)
        : set_surface_property(property, wid);

    if (!ok) {
        env->DeleteGlobalRef(next_surface);
        throw_java_exception(env, "failed to update mpv surface property");
        return;
    }

    clear_surface(env, target);
    *target = next_surface;
}

jni_func(void, attachSurface, jobject surface_) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    update_surface(env, handle.get(), surface_, &surface, "wid");
}

jni_func(void, replaceSurface, jobject surface_) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    update_surface(env, handle.get(), surface_, &surface, "wid");
}

jni_func(void, detachSurface) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    if (set_wid(handle.get(), 0))
        clear_surface(env, &surface);
}

// ── Subtitle surface (separate ANativeWindow for subtitle overlay) ────────────

jni_func(void, attachSubtitleSurface, jobject surface_) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    update_surface(env, nullptr, surface_, &subtitle_surface, "android-subtitle-wid");
}

jni_func(void, replaceSubtitleSurface, jobject surface_) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    update_surface(env, nullptr, surface_, &subtitle_surface, "android-subtitle-wid");
}

jni_func(void, detachSubtitleSurface) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return;
    }
    std::lock_guard<std::mutex> lock(surface_mutex);
    if (set_surface_property("android-subtitle-wid", 0))
        clear_surface(env, &subtitle_surface);
}

// ─────────────────────────────────────────────────────────────────────────────

void release_surface_reference(JNIEnv *env) {
    std::lock_guard<std::mutex> lock(surface_mutex);
    clear_surface(env, &surface);
    clear_surface(env, &subtitle_surface);
}
