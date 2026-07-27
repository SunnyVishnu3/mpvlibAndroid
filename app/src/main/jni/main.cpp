#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <mpv/client.h>

#include <pthread.h>

#include "log.h"
#include "jni_utils.h"
#include "event.h"
#include "node.h"
#include "render.h"

extern "C" {
    jni_func(void, create, jobject appctx);
    jni_func(void, init);
    jni_func(void, destroy);

    jni_func(void, command, jobjectArray jarray);
    jni_func(jint, commandResult, jobjectArray jarray);
    jni_func(jobject, commandNode, jobjectArray jarray);
};

JavaVM *g_vm;
mpv_handle *g_mpv;
std::atomic<bool> g_event_thread_request_exit(false);

static pthread_t event_thread_id;
static bool event_thread_started;
static bool mpv_initialized;
static std::mutex lifecycle_call_mutex;
static constexpr int kMaxCommandArguments = 128;

static void throw_error_code(JNIEnv *env, const char *action, int result,
                             const char *detail)
{
    char message[256];
    if (detail)
        snprintf(message, sizeof(message), "%s failed (%d: %s)", action, result, detail);
    else
        snprintf(message, sizeof(message), "%s failed (%d)", action, result);
    throw_java_exception(env, message);
}

static void destroy_failed_mpv_context(JNIEnv *env)
{
    mpv_handle *handle = fail_mpv_init();
    if (!handle)
        return;
    mpv_terminate_destroy(handle);
    finish_mpv_destroy();
    mpv_initialized = false;
    release_surface_reference(env);
}

static bool prepare_environment(JNIEnv *env, jobject appctx) {
    setlocale(LC_NUMERIC, "C");

    if (!appctx) {
        throw_java_exception(env, "android app context is null");
        return false;
    }

    if (!init_android_jni_environment(env, appctx, &g_vm)) {
        if (!env->ExceptionCheck())
            throw_java_exception(env, "failed to initialize Android JNI environment");
        return false;
    }

    if (!init_methods_cache(env)) {
        if (!env->ExceptionCheck())
            throw_java_exception(env, "failed to initialize java method cache");
        return false;
    }
    return true;
}

jni_func(void, create, jobject appctx) {
    std::lock_guard<std::mutex> lifecycle_lock(lifecycle_call_mutex);
    if (!begin_mpv_create()) {
        throw_java_exception(env, "mpv is already created");
        return;
    }

    if (!prepare_environment(env, appctx)) {
        cancel_mpv_create();
        return;
    }

    mpv_handle *handle = mpv_create();
    if (!handle) {
        cancel_mpv_create();
        throw_java_exception(env, "context init failed");
        return;
    }

    // use terminal log level but request verbose messages
    // this way --msg-level can be used to adjust later
    int result = mpv_request_log_messages(handle, "terminal-default");
    if (result < 0)
        ALOGE("mpv_request_log_messages failed: %s", mpv_error_string(result));
    result = mpv_set_option_string(handle, "msg-level", "all=v");
    if (result < 0)
        ALOGE("setting msg-level failed: %s", mpv_error_string(result));
    finish_mpv_create(handle);
}

jni_func(void, init) {
    std::lock_guard<std::mutex> lifecycle_lock(lifecycle_call_mutex);
    if (mpv_initialized) {
        throw_java_exception(env, "mpv is already initialized");
        return;
    }

    mpv_handle *handle = begin_mpv_init();
    if (!handle) {
        throw_java_exception(env, "mpv is not created");
        return;
    }

    int result = mpv_initialize(handle);
    if (result < 0) {
        throw_error_code(env, "mpv_initialize", result, mpv_error_string(result));
        destroy_failed_mpv_context(env);
        return;
    }
    g_event_thread_request_exit = false;
    result = pthread_create(&event_thread_id, NULL, event_thread, NULL);
    if (result != 0) {
        throw_error_code(env, "pthread_create", result, strerror(result));
        destroy_failed_mpv_context(env);
        return;
    }
    event_thread_started = true;
    mpv_initialized = true;
    finish_mpv_init();
    pthread_setname_np(event_thread_id, "event_thread");
}

jni_func(void, destroy) {
    std::unique_lock<std::mutex> lifecycle_lock(lifecycle_call_mutex);
    if (event_thread_started && pthread_equal(pthread_self(), event_thread_id)) {
        throw_java_exception(env, "mpv destroy cannot run on the event thread");
        return;
    }

    mpv_handle *handle = begin_mpv_destroy();
    if (!handle) {
        ALOGV("mpv destroy called but it's already destroyed");
        return;
    }

    bool join_event_thread = event_thread_started;
    pthread_t thread_to_join = event_thread_id;
    event_thread_started = false;
    lifecycle_lock.unlock();

    if (join_event_thread) {
        g_event_thread_request_exit = true;
        mpv_wakeup(handle);
        int result = pthread_join(thread_to_join, NULL);
        if (result != 0) {
            lifecycle_lock.lock();
            event_thread_id = thread_to_join;
            event_thread_started = true;
            cancel_mpv_destroy();
            throw_error_code(env, "pthread_join", result, strerror(result));
            return;
        }
    }

    mpv_terminate_destroy(handle);
    lifecycle_lock.lock();
    mpv_initialized = false;
    release_surface_reference(env);
    finish_mpv_destroy();
}

static void release_command_arguments(JNIEnv *env, int len, jstring *strings,
                                      const char **arguments)
{
    for (int i = 0; i < len; ++i) {
        if (arguments[i])
            env->ReleaseStringUTFChars(strings[i], arguments[i]);
        if (strings[i])
            env->DeleteLocalRef(strings[i]);
    }
}

jni_func(jint, commandResult, jobjectArray jarray) {
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;

    const char *arguments[kMaxCommandArguments] = {0};
    jstring strings[kMaxCommandArguments] = {0};
    if (!jarray)
        return MPV_ERROR_INVALID_PARAMETER;
    int len = env->GetArrayLength(jarray);
    if (len >= kMaxCommandArguments)
        return MPV_ERROR_INVALID_PARAMETER;

    for (int i = 0; i < len; ++i) {
        strings[i] = (jstring)env->GetObjectArrayElement(jarray, i);
        if (!strings[i]) {
            release_command_arguments(env, len, strings, arguments);
            return MPV_ERROR_INVALID_PARAMETER;
        }
        arguments[i] = env->GetStringUTFChars(strings[i], NULL);
        if (!arguments[i]) {
            release_command_arguments(env, len, strings, arguments);
            return MPV_ERROR_NOMEM;
        }
    }

    int result = mpv_command(handle.get(), arguments);
    if (result < 0)
        ALOGE("mpv_command returned error %s", mpv_error_string(result));
    release_command_arguments(env, len, strings, arguments);
    return result;
}

jni_func(void, command, jobjectArray jarray) {
    jni_func_name(commandResult)(env, obj, jarray);
}

jni_func(jobject, commandNode, jobjectArray jarray) {
    MpvHandleGuard handle;
    if (!handle) {
        throw_java_exception(env, "libmpv is not initialized");
        return NULL;
    }
    if (!jarray) {
        throw_java_exception(env, "commandNode arguments are null");
        return NULL;
    }

    int len = env->GetArrayLength(jarray);
    if (len <= 0 || len > kMaxCommandArguments) {
        throw_java_exception(env, "commandNode argument count is invalid");
        return NULL;
    }

    std::vector<std::string> argument_storage;
    argument_storage.reserve(len);
    for (int i = 0; i < len; ++i) {
        jstring string = (jstring)env->GetObjectArrayElement(jarray, i);
        if (!string)
            return NULL;
        const char *chars = env->GetStringUTFChars(string, NULL);
        if (!chars) {
            env->DeleteLocalRef(string);
            return NULL;
        }
        argument_storage.emplace_back(chars);
        env->ReleaseStringUTFChars(string, chars);
        env->DeleteLocalRef(string);
    }

    std::vector<mpv_node> values(len);
    for (int i = 0; i < len; ++i) {
        values[i].format = MPV_FORMAT_STRING;
        values[i].u.string = const_cast<char *>(argument_storage[i].c_str());
    }
    mpv_node_list list{};
    list.num = len;
    list.values = values.data();
    mpv_node args{};
    args.format = MPV_FORMAT_NODE_ARRAY;
    args.u.list = &list;

    mpv_node result{};
    int error = mpv_command_node(handle.get(), &args, &result);
    if (error < 0) {
        ALOGE("mpv_command_node returned error %s", mpv_error_string(error));
        return NULL;
    }

    jobject jresult = mpv_node_to_jobject(env, &result);
    mpv_free_node_contents(&result);

    return jresult;
}
