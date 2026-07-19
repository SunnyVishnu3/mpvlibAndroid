#define UTIL_EXTERN
#include "jni_utils.h"

#include <jni.h>
#include <mutex>

bool acquire_jni_env(JavaVM *vm, JNIEnv **env)
{
    int ret = vm->GetEnv((void**) env, JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED)
        return vm->AttachCurrentThread(env, NULL) == 0;
    else
        return ret == JNI_OK;
}

// Apparently it's considered slow to FindClass and GetMethodID every time we need them,
// so let's have a nice cache here.

static bool cache_global_class(JNIEnv *env, jclass *cached_class, const char *name)
{
    if (*cached_class)
        return true;
    jclass local_class = env->FindClass(name);
    if (!local_class)
        return false;
    *cached_class = reinterpret_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    return *cached_class != NULL;
}

static bool cache_method(JNIEnv *env, jmethodID *cached_method, jclass clazz,
                         const char *name, const char *signature)
{
    if (!*cached_method)
        *cached_method = env->GetMethodID(clazz, name, signature);
    return *cached_method != NULL;
}

static bool cache_static_method(JNIEnv *env, jmethodID *cached_method, jclass clazz,
                                const char *name, const char *signature)
{
    if (!*cached_method)
        *cached_method = env->GetStaticMethodID(clazz, name, signature);
    return *cached_method != NULL;
}

static bool cache_static_field(JNIEnv *env, jfieldID *cached_field, jclass clazz,
                               const char *name, const char *signature)
{
    if (!*cached_field)
        *cached_field = env->GetStaticFieldID(clazz, name, signature);
    return *cached_field != NULL;
}

bool init_methods_cache(JNIEnv *env)
{
    static std::mutex init_mutex;
    static bool methods_initialized = false;
    std::lock_guard<std::mutex> lock(init_mutex);
    
    if (methods_initialized)
        return true;

    bool success =
        cache_global_class(env, &java_Integer, "java/lang/Integer") &&
        cache_method(env, &java_Integer_init, java_Integer, "<init>", "(I)V") &&
        cache_global_class(env, &java_Double, "java/lang/Double") &&
        cache_method(env, &java_Double_init, java_Double, "<init>", "(D)V") &&
        cache_global_class(env, &java_Boolean, "java/lang/Boolean") &&
        cache_method(env, &java_Boolean_init, java_Boolean, "<init>", "(Z)V") &&
        cache_global_class(env, &android_graphics_Bitmap, "android/graphics/Bitmap") &&
        cache_global_class(env, &android_graphics_Bitmap_Config, "android/graphics/Bitmap$Config") &&
        cache_static_method(env, &android_graphics_Bitmap_createBitmap,
                            android_graphics_Bitmap, "createBitmap",
                            "([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;") &&
        cache_static_method(env, &android_graphics_Bitmap_createBitmapWH,
                            android_graphics_Bitmap, "createBitmap",
                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;") &&
        cache_static_field(env, &android_graphics_Bitmap_Config_ARGB_8888,
                           android_graphics_Bitmap_Config, "ARGB_8888",
                           "Landroid/graphics/Bitmap$Config;") &&
        cache_global_class(env, &mpv_MPVLib, "is/xyz/mpv/MPVLib") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_S, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;)V") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_Sb, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;Z)V") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_Sl, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;J)V") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_Sd, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;D)V") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_SS, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;Ljava/lang/String;)V") &&
        cache_static_method(env, &mpv_MPVLib_eventProperty_SN, mpv_MPVLib,
                            "eventProperty", "(Ljava/lang/String;Lis/xyz/mpv/MPVNode;)V") &&
        cache_static_method(env, &mpv_MPVLib_event, mpv_MPVLib, "event",
                            "(ILis/xyz/mpv/MPVNode;)V") &&
        cache_static_method(env, &mpv_MPVLib_eventEndFile_iiSN, mpv_MPVLib,
                            "eventEndFile",
                            "(IILjava/lang/String;Lis/xyz/mpv/MPVNode;)V") &&
        cache_static_method(env, &mpv_MPVLib_logMessage_SiS, mpv_MPVLib,
                            "logMessage", "(Ljava/lang/String;ILjava/lang/String;)V") &&
        cache_global_class(env, &mpv_MPVNode, "is/xyz/mpv/MPVNode") &&
        cache_global_class(env, &mpv_MPVNode_None, "is/xyz/mpv/MPVNode$None") &&
        cache_static_field(env, &mpv_MPVNode_None_INSTANCE, mpv_MPVNode_None,
                           "INSTANCE", "Lis/xyz/mpv/MPVNode$None;") &&
        cache_global_class(env, &mpv_MPVNode_StringNode, "is/xyz/mpv/MPVNode$StringNode") &&
        cache_method(env, &mpv_MPVNode_StringNode_init, mpv_MPVNode_StringNode,
                     "<init>", "(Ljava/lang/String;)V") &&
        cache_global_class(env, &mpv_MPVNode_BooleanNode, "is/xyz/mpv/MPVNode$BooleanNode") &&
        cache_method(env, &mpv_MPVNode_BooleanNode_init, mpv_MPVNode_BooleanNode,
                     "<init>", "(Z)V") &&
        cache_global_class(env, &mpv_MPVNode_IntNode, "is/xyz/mpv/MPVNode$IntNode") &&
        cache_method(env, &mpv_MPVNode_IntNode_init, mpv_MPVNode_IntNode,
                     "<init>", "(J)V") &&
        cache_global_class(env, &mpv_MPVNode_DoubleNode, "is/xyz/mpv/MPVNode$DoubleNode") &&
        cache_method(env, &mpv_MPVNode_DoubleNode_init, mpv_MPVNode_DoubleNode,
                     "<init>", "(D)V") &&
        cache_global_class(env, &mpv_MPVNode_ByteArrayNode, "is/xyz/mpv/MPVNode$ByteArrayNode") &&
        cache_method(env, &mpv_MPVNode_ByteArrayNode_init, mpv_MPVNode_ByteArrayNode,
                     "<init>", "([B)V") &&
        cache_global_class(env, &mpv_MPVNode_ArrayNode, "is/xyz/mpv/MPVNode$ArrayNode") &&
        cache_method(env, &mpv_MPVNode_ArrayNode_init, mpv_MPVNode_ArrayNode,
                     "<init>", "([Lis/xyz/mpv/MPVNode;)V") &&
        cache_global_class(env, &mpv_MPVNode_MapNode, "is/xyz/mpv/MPVNode$MapNode") &&
        cache_method(env, &mpv_MPVNode_MapNode_init, mpv_MPVNode_MapNode,
                     "<init>", "(Ljava/util/Map;)V") &&
        cache_global_class(env, &java_util_ArrayList, "java/util/ArrayList") &&
        cache_method(env, &java_util_ArrayList_init, java_util_ArrayList,
                     "<init>", "()V") &&
        cache_method(env, &java_util_ArrayList_add, java_util_ArrayList,
                     "add", "(Ljava/lang/Object;)Z") &&
        cache_global_class(env, &java_util_HashMap, "java/util/HashMap") &&
        cache_method(env, &java_util_HashMap_init, java_util_HashMap,
                     "<init>", "()V") &&
        cache_method(env, &java_util_HashMap_put, java_util_HashMap,
                     "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    methods_initialized = success;
    return success;
}
