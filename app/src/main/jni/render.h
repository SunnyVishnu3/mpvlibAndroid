#pragma once

#include <jni.h>

struct mpv_event;

int enqueue_property_string(JNIEnv *env, const char *property,
                            const char *value);
bool handle_property_update_reply(JNIEnv *env, mpv_event *event);
void release_surface_references(JNIEnv *env);
