#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mpv/client.h>
#include "jni_utils.h"

void free_mpv_node(mpv_node *node);

jobject mpv_node_to_jobject(JNIEnv *env, const mpv_node *node) {
    if (!node) return NULL;

    switch (node->format) {
        case MPV_FORMAT_NONE: {
            return env->GetStaticObjectField(mpv_MPVNode_None, mpv_MPVNode_None_INSTANCE);
        }
        case MPV_FORMAT_STRING: {
            if (!node->u.string)
                return NULL;
            jstring jstr = env->NewStringUTF(node->u.string);
            if (!jstr)
                return NULL;
            jobject result = env->NewObject(mpv_MPVNode_StringNode,
                                            mpv_MPVNode_StringNode_init, jstr);
            env->DeleteLocalRef(jstr);
            return result;
        }
        case MPV_FORMAT_FLAG: {
            return env->NewObject(mpv_MPVNode_BooleanNode, mpv_MPVNode_BooleanNode_init, (jboolean)node->u.flag);
        }
        case MPV_FORMAT_INT64: {
            return env->NewObject(mpv_MPVNode_IntNode, mpv_MPVNode_IntNode_init, (jlong)node->u.int64);
        }
        case MPV_FORMAT_DOUBLE: {
            return env->NewObject(mpv_MPVNode_DoubleNode, mpv_MPVNode_DoubleNode_init, (jdouble)node->u.double_);
        }
        case MPV_FORMAT_BYTE_ARRAY: {
            if (!node->u.ba || node->u.ba->size > static_cast<size_t>(INT32_MAX) ||
                    (node->u.ba->size > 0 && !node->u.ba->data))
                return NULL;
            jbyteArray bytes = env->NewByteArray(static_cast<jsize>(node->u.ba->size));
            if (!bytes)
                return NULL;
            if (node->u.ba->size > 0) {
                env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(node->u.ba->size),
                    reinterpret_cast<const jbyte *>(node->u.ba->data));
                if (env->ExceptionCheck()) {
                    env->DeleteLocalRef(bytes);
                    return NULL;
                }
            }
            jobject result = env->NewObject(mpv_MPVNode_ByteArrayNode,
                                            mpv_MPVNode_ByteArrayNode_init, bytes);
            env->DeleteLocalRef(bytes);
            return result;
        }
        case MPV_FORMAT_NODE_ARRAY: {
            if (!node->u.list || node->u.list->num < 0 ||
                    (node->u.list->num > 0 && !node->u.list->values))
                return NULL;
            jobjectArray nodeArray = env->NewObjectArray(node->u.list->num, mpv_MPVNode, NULL);
            if (!nodeArray)
                return NULL;
            for (int i = 0; i < node->u.list->num; i++) {
                jobject childNode = mpv_node_to_jobject(env, &node->u.list->values[i]);
                if (!childNode) {
                    env->DeleteLocalRef(nodeArray);
                    return NULL;
                }
                env->SetObjectArrayElement(nodeArray, i, childNode);
                env->DeleteLocalRef(childNode);
                if (env->ExceptionCheck()) {
                    env->DeleteLocalRef(nodeArray);
                    return NULL;
                }
            }
            jobject result = env->NewObject(mpv_MPVNode_ArrayNode,
                                            mpv_MPVNode_ArrayNode_init, nodeArray);
            env->DeleteLocalRef(nodeArray);
            return result;
        }
        case MPV_FORMAT_NODE_MAP: {
            if (!node->u.list || node->u.list->num < 0 ||
                    (node->u.list->num > 0 &&
                     (!node->u.list->values || !node->u.list->keys)))
                return NULL;
            jobject hashMap = env->NewObject(java_util_HashMap, java_util_HashMap_init);
            if (!hashMap)
                return NULL;
            for (int i = 0; i < node->u.list->num; i++) {
                if (!node->u.list->keys[i]) {
                    env->DeleteLocalRef(hashMap);
                    return NULL;
                }
                jstring key = env->NewStringUTF(node->u.list->keys[i]);
                if (!key) {
                    env->DeleteLocalRef(hashMap);
                    return NULL;
                }
                jobject childNode = mpv_node_to_jobject(env, &node->u.list->values[i]);
                if (!childNode) {
                    env->DeleteLocalRef(key);
                    env->DeleteLocalRef(hashMap);
                    return NULL;
                }
                jobject previous = env->CallObjectMethod(hashMap, java_util_HashMap_put,
                                                         key, childNode);
                if (previous)
                    env->DeleteLocalRef(previous);
                env->DeleteLocalRef(childNode);
                env->DeleteLocalRef(key);
                if (env->ExceptionCheck()) {
                    env->DeleteLocalRef(hashMap);
                    return NULL;
                }
            }
            jobject result = env->NewObject(mpv_MPVNode_MapNode,
                                            mpv_MPVNode_MapNode_init, hashMap);
            env->DeleteLocalRef(hashMap);
            return result;
        }
        default:
            return NULL;
    }
}

// recursively adding all nodes for map and arrays
int jobject_to_mpv_node(JNIEnv *env, jobject jnode, mpv_node *node) {
    if (!jnode || !node) return -1;
    memset(node, 0, sizeof(*node));
    node->format = MPV_FORMAT_NONE;

    if (env->IsInstanceOf(jnode, mpv_MPVNode_None)) {
        node->format = MPV_FORMAT_NONE;
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_StringNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_StringNode, "value", "Ljava/lang/String;");
        if (!valueField)
            return -1;
        jstring jstr = (jstring)env->GetObjectField(jnode, valueField);
        if (jstr) {
            const char *str = env->GetStringUTFChars(jstr, NULL);
            if (!str) {
                env->DeleteLocalRef(jstr);
                return -1;
            }
            node->format = MPV_FORMAT_STRING;
            node->u.string = strdup(str);
            env->ReleaseStringUTFChars(jstr, str);
        } else {
            node->format = MPV_FORMAT_STRING;
            node->u.string = strdup("");
        }
        if (jstr)
            env->DeleteLocalRef(jstr);
        return node->u.string ? 0 : -1;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_BooleanNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_BooleanNode, "value", "Z");
        if (!valueField)
            return -1;
        jboolean flag = env->GetBooleanField(jnode, valueField);
        if (env->ExceptionCheck())
            return -1;
        node->format = MPV_FORMAT_FLAG;
        node->u.flag = flag;
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_IntNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_IntNode, "value", "J");
        if (!valueField)
            return -1;
        jlong int64 = env->GetLongField(jnode, valueField);
        if (env->ExceptionCheck())
            return -1;
        node->format = MPV_FORMAT_INT64;
        node->u.int64 = int64;
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_DoubleNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_DoubleNode, "value", "D");
        if (!valueField)
            return -1;
        jdouble dbl = env->GetDoubleField(jnode, valueField);
        if (env->ExceptionCheck())
            return -1;
        node->format = MPV_FORMAT_DOUBLE;
        node->u.double_ = dbl;
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_ByteArrayNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_ByteArrayNode, "value", "[B");
        if (!valueField)
            return -1;
        jbyteArray bytes = reinterpret_cast<jbyteArray>(env->GetObjectField(jnode, valueField));
        if (env->ExceptionCheck() || !bytes)
            return -1;
        jsize size = env->GetArrayLength(bytes);
        node->u.ba = static_cast<mpv_byte_array *>(calloc(1, sizeof(mpv_byte_array)));
        if (!node->u.ba) {
            env->DeleteLocalRef(bytes);
            return -1;
        }
        node->u.ba->size = size;
        if (size > 0) {
            node->u.ba->data = malloc(size);
            if (!node->u.ba->data) {
                free(node->u.ba);
                node->u.ba = NULL;
                env->DeleteLocalRef(bytes);
                return -1;
            }
            env->GetByteArrayRegion(bytes, 0, size,
                static_cast<jbyte *>(node->u.ba->data));
            if (env->ExceptionCheck()) {
                free(node->u.ba->data);
                free(node->u.ba);
                node->u.ba = NULL;
                env->DeleteLocalRef(bytes);
                return -1;
            }
        }
        env->DeleteLocalRef(bytes);
        node->format = MPV_FORMAT_BYTE_ARRAY;
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_ArrayNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_ArrayNode, "value", "[Lis/xyz/mpv/MPVNode;");
        if (!valueField)
            return -1;
        jobjectArray jarray = (jobjectArray)env->GetObjectField(jnode, valueField);
        if (env->ExceptionCheck())
            return -1;

        if (jarray) {
            jint size = env->GetArrayLength(jarray);

            node->format = MPV_FORMAT_NODE_ARRAY;
            node->u.list = (mpv_node_list*)calloc(1, sizeof(mpv_node_list));
            if (!node->u.list) {
                env->DeleteLocalRef(jarray);
                return -1;
            }
            node->u.list->values = size > 0 ? (mpv_node*)calloc(size, sizeof(mpv_node)) : NULL;
            node->u.list->keys = NULL;
            if (size > 0 && !node->u.list->values) {
                env->DeleteLocalRef(jarray);
                free_mpv_node(node);
                return -1;
            }

            for (int i = 0; i < size; i++) {
                jobject childNode = env->GetObjectArrayElement(jarray, i);
                if (!childNode || jobject_to_mpv_node(
                        env, childNode, &node->u.list->values[i]) < 0) {
                    if (childNode)
                        env->DeleteLocalRef(childNode);
                    env->DeleteLocalRef(jarray);
                    free_mpv_node(&node->u.list->values[i]);
                    free_mpv_node(node);
                    return -1;
                }
                env->DeleteLocalRef(childNode);
                node->u.list->num++;
            }
            env->DeleteLocalRef(jarray);
        } else {
            node->format = MPV_FORMAT_NODE_ARRAY;
            node->u.list = (mpv_node_list*)calloc(1, sizeof(mpv_node_list));
            if (!node->u.list)
                return -1;
        }
        return 0;
    }

    if (env->IsInstanceOf(jnode, mpv_MPVNode_MapNode)) {
        jfieldID valueField = env->GetFieldID(mpv_MPVNode_MapNode, "value", "Ljava/util/Map;");
        if (!valueField)
            return -1;
        jobject jmap = env->GetObjectField(jnode, valueField);
        if (env->ExceptionCheck())
            return -1;

        if (!jmap) {
            node->format = MPV_FORMAT_NODE_MAP;
            node->u.list = (mpv_node_list*)calloc(1, sizeof(mpv_node_list));
            return node->u.list ? 0 : -1;
        }

        jclass mapClass = env->GetObjectClass(jmap);
        jmethodID sizeMethod = mapClass ? env->GetMethodID(mapClass, "size", "()I") : NULL;
        jmethodID entrySetMethod = mapClass ? env->GetMethodID(
            mapClass, "entrySet", "()Ljava/util/Set;") : NULL;
        if (!mapClass || !sizeMethod || !entrySetMethod) {
            if (mapClass) env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(jmap);
            return -1;
        }

        jint size = env->CallIntMethod(jmap, sizeMethod);
        if (env->ExceptionCheck()) {
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(jmap);
            return -1;
        }
        jobject entrySet = env->CallObjectMethod(jmap, entrySetMethod);
        if (env->ExceptionCheck() || !entrySet) {
            if (entrySet) env->DeleteLocalRef(entrySet);
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(jmap);
            return -1;
        }
        jclass setClass = entrySet ? env->GetObjectClass(entrySet) : NULL;
        jmethodID toArrayMethod = setClass ? env->GetMethodID(
            setClass, "toArray", "()[Ljava/lang/Object;") : NULL;
        if (env->ExceptionCheck() || !setClass || !toArrayMethod || size < 0) {
            if (setClass) env->DeleteLocalRef(setClass);
            env->DeleteLocalRef(entrySet);
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(jmap);
            return -1;
        }
        jobjectArray entryArray = (jobjectArray)env->CallObjectMethod(entrySet,
                                                                      toArrayMethod);
        if (env->ExceptionCheck() || !entryArray ||
                env->GetArrayLength(entryArray) != size) {
            if (entryArray) env->DeleteLocalRef(entryArray);
            env->DeleteLocalRef(setClass);
            env->DeleteLocalRef(entrySet);
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(jmap);
            return -1;
        }

        node->format = MPV_FORMAT_NODE_MAP;
        node->u.list = (mpv_node_list*)calloc(1, sizeof(mpv_node_list));
        if (node->u.list && size > 0) {
            node->u.list->values = (mpv_node*)calloc(size, sizeof(mpv_node));
            node->u.list->keys = (char**)calloc(size, sizeof(char*));
        }
        bool success = node->u.list &&
            (size == 0 || (node->u.list->values && node->u.list->keys));

        for (int i = 0; success && i < size; i++) {
            jobject entry = env->GetObjectArrayElement(entryArray, i);
            jclass entryClass = entry ? env->GetObjectClass(entry) : NULL;
            jmethodID getKeyMethod = entryClass ? env->GetMethodID(
                entryClass, "getKey", "()Ljava/lang/Object;") : NULL;
            jmethodID getValueMethod = entryClass ? env->GetMethodID(
                entryClass, "getValue", "()Ljava/lang/Object;") : NULL;
            if (env->ExceptionCheck() || !entry || !entryClass ||
                    !getKeyMethod || !getValueMethod) {
                success = false;
            } else {
                jobject keyObj = env->CallObjectMethod(entry, getKeyMethod);
                if (env->ExceptionCheck() || !keyObj ||
                        !env->IsInstanceOf(keyObj, java_String)) {
                    success = false;
                } else {
                    jobject valueObj = env->CallObjectMethod(entry, getValueMethod);
                    if (env->ExceptionCheck() || !valueObj) {
                        success = false;
                    } else {
                        jstring keyStr = reinterpret_cast<jstring>(keyObj);
                        const char *key = env->GetStringUTFChars(keyStr, NULL);
                        if (!key) {
                            success = false;
                        } else {
                            node->u.list->keys[i] = strdup(key);
                            env->ReleaseStringUTFChars(keyStr, key);
                            success = node->u.list->keys[i] &&
                                jobject_to_mpv_node(env, valueObj,
                                                    &node->u.list->values[i]) == 0;
                            if (success) {
                                node->u.list->num++;
                            } else {
                                free(node->u.list->keys[i]);
                                node->u.list->keys[i] = NULL;
                                free_mpv_node(&node->u.list->values[i]);
                            }
                        }
                        env->DeleteLocalRef(valueObj);
                    }
                }
                if (keyObj) env->DeleteLocalRef(keyObj);
            }
            if (entryClass) env->DeleteLocalRef(entryClass);
            if (entry) env->DeleteLocalRef(entry);
        }

        env->DeleteLocalRef(entryArray);
        env->DeleteLocalRef(setClass);
        env->DeleteLocalRef(entrySet);
        env->DeleteLocalRef(mapClass);
        env->DeleteLocalRef(jmap);
        if (!success) {
            free_mpv_node(node);
            return -1;
        }
        return 0;
    }

    return -1;
}

void free_mpv_node(mpv_node *node) {
    if (!node) return;

    switch (node->format) {
        case MPV_FORMAT_STRING:
            if (node->u.string) {
                free(node->u.string);
                node->u.string = NULL;
            }
            break;
        case MPV_FORMAT_BYTE_ARRAY:
            if (node->u.ba) {
                free(node->u.ba->data);
                free(node->u.ba);
                node->u.ba = NULL;
            }
            break;
        case MPV_FORMAT_NODE_ARRAY:
        case MPV_FORMAT_NODE_MAP:
            if (node->u.list) {
                if (node->u.list->values) {
                    for (int i = 0; i < node->u.list->num; i++)
                        free_mpv_node(&node->u.list->values[i]);
                }

                if (node->format == MPV_FORMAT_NODE_MAP && node->u.list->keys) {
                    for (int i = 0; i < node->u.list->num; i++)
                        if (node->u.list->keys[i]) free(node->u.list->keys[i]);
                    free(node->u.list->keys);
                }

                if (node->u.list->values)
                    free(node->u.list->values);
                free(node->u.list);
                node->u.list = NULL;
            }
            break;
        default:
            break;
    }
    node->format = MPV_FORMAT_NONE;
}
