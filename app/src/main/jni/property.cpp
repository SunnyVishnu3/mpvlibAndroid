#include <jni.h>
#include <stdint.h>
#include <stdlib.h>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"
#include "node.h"

extern "C" {
    jni_func(jint, setOptionString, jstring option, jstring value);

    jni_func(jobject, getPropertyInt, jstring property);
    jni_func(void, setPropertyInt, jstring property, jint value);
    jni_func(jint, setPropertyIntResult, jstring property, jint value);
    jni_func(jobject, getPropertyDouble, jstring property);
    jni_func(void, setPropertyDouble, jstring property, jdouble value);
    jni_func(jint, setPropertyDoubleResult, jstring property, jdouble value);
    jni_func(jobject, getPropertyBoolean, jstring property);
    jni_func(void, setPropertyBoolean, jstring property, jboolean value);
    jni_func(jint, setPropertyBooleanResult, jstring property, jboolean value);
    jni_func(jstring, getPropertyString, jstring jproperty);
    jni_func(void, setPropertyString, jstring jproperty, jstring jvalue);
    jni_func(jint, setPropertyStringResult, jstring jproperty, jstring jvalue);
    jni_func(jbyteArray, getPropertyByteArray, jstring jproperty);
    jni_func(jobject, getPropertyNode, jstring jproperty);
    jni_func(void, setPropertyNode, jstring jproperty, jobject jnode);
    jni_func(jint, setPropertyNodeResult, jstring jproperty, jobject jnode);

    jni_func(void, observeProperty, jstring property, jint format);
    jni_func(jint, observePropertyResult, jstring property, jint format);
}

static int get_utf_chars(JNIEnv *env, jstring string, const char **chars)
{
    *chars = NULL;
    if (!string)
        return MPV_ERROR_INVALID_PARAMETER;
    *chars = env->GetStringUTFChars(string, NULL);
    return *chars ? MPV_ERROR_SUCCESS : MPV_ERROR_NOMEM;
}

static void release_utf_chars(JNIEnv *env, jstring string, const char *chars)
{
    if (chars)
        env->ReleaseStringUTFChars(string, chars);
}

jni_func(jint, setOptionString, jstring joption, jstring jvalue) {
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;

    const char *option = NULL;
    const char *value = NULL;
    int result = get_utf_chars(env, joption, &option);
    if (result >= 0)
        result = get_utf_chars(env, jvalue, &value);
    if (result >= 0)
        result = mpv_set_option_string(handle.get(), option, value);

    release_utf_chars(env, joption, option);
    release_utf_chars(env, jvalue, value);

    return result;
}

static int common_get_property(JNIEnv *env, jstring jproperty, mpv_format format, void *output)
{
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;

    const char *prop = NULL;
    int result = get_utf_chars(env, jproperty, &prop);
    if (result < 0)
        return result;
    result = mpv_get_property(handle.get(), prop, format, output);
    if (result == MPV_ERROR_PROPERTY_UNAVAILABLE)
        ALOGV("mpv_get_property(%s) format %d was unavailable", prop, format);
    else if (result < 0)
        ALOGE("mpv_get_property(%s) format %d returned error %s", prop, format, mpv_error_string(result));
    release_utf_chars(env, jproperty, prop);

    return result;
}

static int common_set_property(JNIEnv *env, jstring jproperty, mpv_format format, void *value)
{
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;

    const char *prop = NULL;
    int result = get_utf_chars(env, jproperty, &prop);
    if (result < 0)
        return result;
    result = mpv_set_property(handle.get(), prop, format, value);
    if (result < 0)
        ALOGE("mpv_set_property(%s, %p) format %d returned error %s", prop, value, format, mpv_error_string(result));
    release_utf_chars(env, jproperty, prop);

    return result;
}

jni_func(jobject, getPropertyInt, jstring jproperty) {
    int64_t value = 0;
    if (common_get_property(env, jproperty, MPV_FORMAT_INT64, &value) < 0)
        return NULL;
    return env->NewObject(java_Integer, java_Integer_init, (jint)value);
}

jni_func(jobject, getPropertyDouble, jstring jproperty) {
    double value = 0;
    if (common_get_property(env, jproperty, MPV_FORMAT_DOUBLE, &value) < 0)
        return NULL;
    return env->NewObject(java_Double, java_Double_init, (jdouble)value);
}

jni_func(jobject, getPropertyBoolean, jstring jproperty) {
    int value = 0;
    if (common_get_property(env, jproperty, MPV_FORMAT_FLAG, &value) < 0)
        return NULL;
    return env->NewObject(java_Boolean, java_Boolean_init, (jboolean)value);
}

jni_func(jstring, getPropertyString, jstring jproperty) {
    char *value;
    if (common_get_property(env, jproperty, MPV_FORMAT_STRING, &value) < 0)
        return NULL;
    jstring jvalue = env->NewStringUTF(value);
    mpv_free(value);
    return jvalue;
}

jni_func(jint, setPropertyIntResult, jstring jproperty, jint jvalue) {
    int64_t value = static_cast<int64_t>(jvalue);
    return common_set_property(env, jproperty, MPV_FORMAT_INT64, &value);
}

jni_func(void, setPropertyInt, jstring jproperty, jint jvalue) {
    jni_func_name(setPropertyIntResult)(env, obj, jproperty, jvalue);
}

jni_func(jint, setPropertyDoubleResult, jstring jproperty, jdouble jvalue) {
    double value = static_cast<double>(jvalue);
    return common_set_property(env, jproperty, MPV_FORMAT_DOUBLE, &value);
}

jni_func(void, setPropertyDouble, jstring jproperty, jdouble jvalue) {
    jni_func_name(setPropertyDoubleResult)(env, obj, jproperty, jvalue);
}

jni_func(jint, setPropertyBooleanResult, jstring jproperty, jboolean jvalue) {
    int value = jvalue == JNI_TRUE ? 1 : 0;
    return common_set_property(env, jproperty, MPV_FORMAT_FLAG, &value);
}

jni_func(void, setPropertyBoolean, jstring jproperty, jboolean jvalue) {
    jni_func_name(setPropertyBooleanResult)(env, obj, jproperty, jvalue);
}

jni_func(jint, setPropertyStringResult, jstring jproperty, jstring jvalue) {
    const char *value = NULL;
    int result = get_utf_chars(env, jvalue, &value);
    if (result >= 0)
        result = common_set_property(env, jproperty, MPV_FORMAT_STRING, &value);
    release_utf_chars(env, jvalue, value);
    return result;
}

jni_func(void, setPropertyString, jstring jproperty, jstring jvalue) {
    jni_func_name(setPropertyStringResult)(env, obj, jproperty, jvalue);
}

jni_func(jbyteArray, getPropertyByteArray, jstring jproperty) {
    mpv_node node{};
    if (common_get_property(env, jproperty, MPV_FORMAT_NODE, &node) < 0)
        return NULL;

    jbyteArray result = NULL;
    if (node.format == MPV_FORMAT_BYTE_ARRAY && node.u.ba &&
            node.u.ba->size <= static_cast<size_t>(INT32_MAX) &&
            (node.u.ba->size == 0 || node.u.ba->data)) {
        jsize size = static_cast<jsize>(node.u.ba->size);
        result = env->NewByteArray(size);
        if (result && size > 0) {
            env->SetByteArrayRegion(result, 0, size,
                reinterpret_cast<const jbyte *>(node.u.ba->data));
        }
    }
    mpv_free_node_contents(&node);
    return result;
}

jni_func(jobject, getPropertyNode, jstring jproperty) {
    mpv_node result{};
    if (common_get_property(env, jproperty, MPV_FORMAT_NODE, &result) < 0)
        return NULL;

    jobject jresult = mpv_node_to_jobject(env, &result);
    mpv_free_node_contents(&result);

    return jresult;
}

jni_func(jint, setPropertyNodeResult, jstring jproperty, jobject jnode) {
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;

    const char *property = NULL;
    int result = get_utf_chars(env, jproperty, &property);
    if (result < 0)
        return result;

    mpv_node node{};
    int parse_error = jobject_to_mpv_node(env, jnode, &node);
    if (parse_error == 0) {
        result = mpv_set_property(handle.get(), property, MPV_FORMAT_NODE, &node);
        free_mpv_node(&node);
        if (result < 0)
            ALOGE("mpv_set_property(%s) returned error %s", property, mpv_error_string(result));
    } else {
        free_mpv_node(&node);
        result = MPV_ERROR_INVALID_PARAMETER;
    }

    release_utf_chars(env, jproperty, property);
    return result;
}

jni_func(void, setPropertyNode, jstring jproperty, jobject jnode) {
    jni_func_name(setPropertyNodeResult)(env, obj, jproperty, jnode);
}

jni_func(jint, observePropertyResult, jstring property, jint format) {
    MpvHandleGuard handle;
    if (!handle)
        return MPV_ERROR_UNINITIALIZED;
    const char *prop = NULL;
    int result = get_utf_chars(env, property, &prop);
    if (result < 0)
        return result;
    result = mpv_observe_property(handle.get(), 0, prop, (mpv_format)format);
    if (result < 0)
        ALOGE("mpv_observe_property(%s) format %d returned error %s", prop, format, mpv_error_string(result));
    release_utf_chars(env, property, prop);
    return result;
}

jni_func(void, observeProperty, jstring property, jint format) {
    jni_func_name(observePropertyResult)(env, obj, property, format);
}
