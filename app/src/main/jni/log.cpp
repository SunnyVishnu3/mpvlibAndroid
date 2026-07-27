#include "log.h"

#include <condition_variable>
#include <mutex>

#include "globals.h"

static std::mutex lifecycle_mutex;
static std::condition_variable lifecycle_idle;
static unsigned int lifecycle_users;
static bool lifecycle_creating;
static bool lifecycle_initializing;
static bool lifecycle_destroying;

void throw_java_exception(JNIEnv *env, const char *msg)
{
    ALOGE("%s", msg);
    if (!env || env->ExceptionCheck())
        return;

    jclass exception_class = env->FindClass("java/lang/IllegalStateException");
    if (!exception_class)
        return;
    env->ThrowNew(exception_class, msg);
    env->DeleteLocalRef(exception_class);
}

bool check_mpv_initialized()
{
    bool available;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex);
        available = g_mpv != NULL && !lifecycle_initializing &&
                    !lifecycle_destroying;
    }
    if (__builtin_expect(available, 1))
        return true;
    ALOGE("libmpv is not initialized");
    return false;
}

bool require_mpv_initialized(JNIEnv *env)
{
    if (check_mpv_initialized())
        return true;
    throw_java_exception(env, "libmpv is not initialized");
    return false;
}

MpvHandleGuard::MpvHandleGuard() : handle(NULL)
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (!g_mpv || lifecycle_initializing || lifecycle_destroying)
        return;
    handle = g_mpv;
    lifecycle_users++;
}

MpvHandleGuard::~MpvHandleGuard()
{
    if (!handle)
        return;
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (--lifecycle_users == 0)
        lifecycle_idle.notify_all();
}

bool begin_mpv_create()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (g_mpv || lifecycle_creating || lifecycle_destroying)
        return false;
    lifecycle_creating = true;
    return true;
}

void finish_mpv_create(mpv_handle *handle)
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    g_mpv = handle;
    lifecycle_creating = false;
}

void cancel_mpv_create()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    lifecycle_creating = false;
}

mpv_handle *begin_mpv_init()
{
    std::unique_lock<std::mutex> lock(lifecycle_mutex);
    if (!g_mpv || lifecycle_initializing || lifecycle_destroying)
        return NULL;
    lifecycle_initializing = true;
    lifecycle_idle.wait(lock, [] { return lifecycle_users == 0; });
    return g_mpv;
}

void finish_mpv_init()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    lifecycle_initializing = false;
}

mpv_handle *fail_mpv_init()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (!g_mpv || !lifecycle_initializing)
        return NULL;
    lifecycle_initializing = false;
    lifecycle_destroying = true;
    return g_mpv;
}

mpv_handle *begin_mpv_destroy()
{
    std::unique_lock<std::mutex> lock(lifecycle_mutex);
    if (!g_mpv || lifecycle_initializing || lifecycle_destroying)
        return NULL;
    lifecycle_destroying = true;
    lifecycle_idle.wait(lock, [] { return lifecycle_users == 0; });
    return g_mpv;
}

void finish_mpv_destroy()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    g_mpv = NULL;
    lifecycle_destroying = false;
}

void cancel_mpv_destroy()
{
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    lifecycle_destroying = false;
}
