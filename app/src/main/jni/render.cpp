#include <jni.h>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"
#include "render.h"

extern "C" {
    jni_func(void, attachSurface, jobject surface_);
    jni_func(void, replaceSurface, jobject surface_);
    jni_func(void, detachSurface);
};

static jobject surface;

static bool set_wid(int64_t wid)
{
    int result = mpv_set_option(g_mpv, "wid", MPV_FORMAT_INT64, &wid);
    if (result < 0)
        ALOGE("mpv_set_option(wid) returned error %s", mpv_error_string(result));
    return result >= 0;
}

static void clear_surface(JNIEnv *env)
{
    if (!surface)
        return;
    env->DeleteGlobalRef(surface);
    surface = NULL;
}

static void update_surface(JNIEnv *env, jobject next)
{
    if (!next) {
        throw_java_exception(env, "invalid surface provided");
        return;
    }

    jobject next_surface = env->NewGlobalRef(next);
    if (!next_surface) {
        if (!env->ExceptionCheck())
            throw_java_exception(env, "failed to retain surface");
        return;
    }
    int64_t wid = reinterpret_cast<intptr_t>(next_surface);
    if (!set_wid(wid)) {
        env->DeleteGlobalRef(next_surface);
        return;
    }
    clear_surface(env);
    surface = next_surface;
}

jni_func(void, attachSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_);
}

jni_func(void, replaceSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_);
}

jni_func(void, detachSurface) {
    if (!require_mpv_initialized(env))
        return;

    if (set_wid(0))
        clear_surface(env);
}

void release_surface_reference(JNIEnv *env) {
    clear_surface(env);
}
