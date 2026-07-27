#pragma once

#include <android/log.h>
#include <jni.h>
#include <mpv/client.h>

#define DEBUG 1

#define LOG_TAG "mpv"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...) (void)0
#endif

void throw_java_exception(JNIEnv *env, const char *msg);
bool check_mpv_initialized();
bool require_mpv_initialized(JNIEnv *env);

class MpvHandleGuard {
public:
    MpvHandleGuard();
    ~MpvHandleGuard();

    MpvHandleGuard(const MpvHandleGuard &) = delete;
    MpvHandleGuard &operator=(const MpvHandleGuard &) = delete;

    explicit operator bool() const { return handle != NULL; }
    mpv_handle *get() const { return handle; }

private:
    mpv_handle *handle;
};

bool begin_mpv_create();
void finish_mpv_create(mpv_handle *handle);
void cancel_mpv_create();
mpv_handle *begin_mpv_init();
void finish_mpv_init();
mpv_handle *fail_mpv_init();
mpv_handle *begin_mpv_destroy();
void finish_mpv_destroy();
void cancel_mpv_destroy();
