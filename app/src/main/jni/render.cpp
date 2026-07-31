#include <jni.h>

#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include <mpv/client.h>

#include "jni_utils.h"
#include "log.h"
#include "globals.h"
#include "render.h"

extern "C" {
    jni_func(void, attachSurface, jobject surface_);
    jni_func(void, replaceSurface, jobject surface_);
    jni_func(void, detachSurface);
    jni_func(void, attachOsdSurface, jobject surface_);
    jni_func(void, replaceOsdSurface, jobject surface_);
    jni_func(void, detachOsdSurface);
};

enum class PropertyUpdateType {
    VIDEO_SURFACE,
    OSD_SURFACE,
    STRING,
};

struct PropertyUpdate {
    PropertyUpdateType type;
    std::string property;
    std::string value;
    jobject surface;

    PropertyUpdate(PropertyUpdateType type_, const char *property_,
                   jobject surface_)
        : type(type_), property(property_), surface(surface_) {}

    PropertyUpdate(const char *property_, const char *value_)
        : type(PropertyUpdateType::STRING), property(property_), value(value_),
          surface(NULL) {}
};

static constexpr uint64_t PROPERTY_UPDATE_REPLY = 0x5355524641434501ULL;

// mpv may reorder asynchronous requests. Keep one update in flight so Surface
// references remain valid until the corresponding property reply arrives.
static std::mutex property_update_mutex;
static std::deque<std::unique_ptr<PropertyUpdate>> property_updates;
static std::unique_ptr<PropertyUpdate> property_update_in_flight;
static jobject surface;
static jobject osd_surface;

static void clear_surface(JNIEnv *env, jobject *target) {
    if (!*target)
        return;
    env->DeleteGlobalRef(*target);
    *target = NULL;
}

static void clear_update_surface(JNIEnv *env, PropertyUpdate *update) {
    if (!update || !update->surface)
        return;
    env->DeleteGlobalRef(update->surface);
    update->surface = NULL;
}

static jobject *get_applied_surface(PropertyUpdateType type) {
    if (type == PropertyUpdateType::VIDEO_SURFACE)
        return &surface;
    if (type == PropertyUpdateType::OSD_SURFACE)
        return &osd_surface;
    return NULL;
}

static int start_next_property_update_locked(JNIEnv *env) {
    if (property_update_in_flight || property_updates.empty())
        return MPV_ERROR_SUCCESS;

    // Use require_mpv_initialized to check state (our architecture)
    mpv_handle *context = g_mpv;
    if (!context)
        return MPV_ERROR_UNINITIALIZED;

    std::unique_ptr<PropertyUpdate> update = std::move(property_updates.front());
    property_updates.pop_front();

    int result;
    if (update->type == PropertyUpdateType::STRING) {
        const char *value = update->value.c_str();
        result = mpv_set_property_async(context, PROPERTY_UPDATE_REPLY,
            update->property.c_str(), MPV_FORMAT_STRING, &value);
    } else {
        int64_t wid = reinterpret_cast<intptr_t>(update->surface);
        result = mpv_set_property_async(context, PROPERTY_UPDATE_REPLY,
            update->property.c_str(), MPV_FORMAT_INT64, &wid);
    }
    if (result < 0) {
        ALOGE("mpv_set_property_async(%s) returned error %s",
              update->property.c_str(), mpv_error_string(result));
        clear_update_surface(env, update.get());
        return result;
    }

    property_update_in_flight = std::move(update);
    return MPV_ERROR_SUCCESS;
}

static int enqueue_property_update(JNIEnv *env,
                                   std::unique_ptr<PropertyUpdate> update) {
    std::lock_guard<std::mutex> lock(property_update_mutex);
    if (!g_mpv) {
        clear_update_surface(env, update.get());
        return MPV_ERROR_UNINITIALIZED;
    }
    property_updates.push_back(std::move(update));
    return start_next_property_update_locked(env);
}

static void update_surface(JNIEnv *env, jobject surface_,
                           PropertyUpdateType type, const char *property) {
    jobject next_surface = env->NewGlobalRef(surface_);
    if (!next_surface) {
        if (!env->ExceptionCheck())
            throw_java_exception(env, "invalid surface provided");
        return;
    }

    std::unique_ptr<PropertyUpdate> update(
        new PropertyUpdate(type, property, next_surface));
    int result = enqueue_property_update(env, std::move(update));
    if (result < 0)
        throw_java_exception(env, "failed to queue mpv surface property update");
}

static void detach_surface(JNIEnv *env, PropertyUpdateType type,
                           const char *property) {
    std::unique_ptr<PropertyUpdate> update(
        new PropertyUpdate(type, property, NULL));
    int result = enqueue_property_update(env, std::move(update));
    if (result < 0)
        throw_java_exception(env, "failed to queue mpv surface detach");
}

jni_func(void, attachSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_, PropertyUpdateType::VIDEO_SURFACE, "wid");
}

jni_func(void, replaceSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_, PropertyUpdateType::VIDEO_SURFACE, "wid");
}

jni_func(void, detachSurface) {
    if (!require_mpv_initialized(env))
        return;

    detach_surface(env, PropertyUpdateType::VIDEO_SURFACE, "wid");
}

jni_func(void, attachOsdSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_, PropertyUpdateType::OSD_SURFACE,
                   "android-osd-wid");
}

jni_func(void, replaceOsdSurface, jobject surface_) {
    if (!require_mpv_initialized(env))
        return;
    update_surface(env, surface_, PropertyUpdateType::OSD_SURFACE,
                   "android-osd-wid");
}

jni_func(void, detachOsdSurface) {
    if (!require_mpv_initialized(env))
        return;

    detach_surface(env, PropertyUpdateType::OSD_SURFACE, "android-osd-wid");
}

int enqueue_property_string(JNIEnv *env, const char *property,
                            const char *value) {
    std::unique_ptr<PropertyUpdate> update(
        new PropertyUpdate(property, value));
    return enqueue_property_update(env, std::move(update));
}

bool handle_property_update_reply(JNIEnv *env, mpv_event *event) {
    if (event->event_id != MPV_EVENT_SET_PROPERTY_REPLY ||
            event->reply_userdata != PROPERTY_UPDATE_REPLY)
        return false;

    std::lock_guard<std::mutex> lock(property_update_mutex);
    if (!property_update_in_flight) {
        ALOGE("received an unexpected property update reply");
        return true;
    }

    PropertyUpdate *update = property_update_in_flight.get();
    jobject *applied_surface = get_applied_surface(update->type);
    if (event->error < 0) {
        ALOGE("asynchronous mpv property update %s failed: %s",
              update->property.c_str(), mpv_error_string(event->error));
        clear_update_surface(env, update);
    } else if (applied_surface) {
        clear_surface(env, applied_surface);
        *applied_surface = update->surface;
        update->surface = NULL;
    }
    property_update_in_flight.reset();

    while (!property_updates.empty()) {
        int result = start_next_property_update_locked(env);
        if (result >= 0 || result == MPV_ERROR_UNINITIALIZED)
            break;
    }
    return true;
}

void release_surface_references(JNIEnv *env) {
    std::lock_guard<std::mutex> lock(property_update_mutex);
    clear_surface(env, &surface);
    clear_surface(env, &osd_surface);
    clear_update_surface(env, property_update_in_flight.get());
    property_update_in_flight.reset();
    for (const std::unique_ptr<PropertyUpdate> &update : property_updates)
        clear_update_surface(env, update.get());
    property_updates.clear();
}
