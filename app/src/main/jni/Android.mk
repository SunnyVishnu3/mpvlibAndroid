LOCAL_PATH:= $(call my-dir)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
PREFIX = $(PREFIX32)
endif
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
PREFIX = $(PREFIX64)
endif
ifeq ($(TARGET_ARCH_ABI),x86_64)
PREFIX = $(PREFIX_X64)
endif
ifeq ($(TARGET_ARCH_ABI),x86)
PREFIX = $(PREFIX_X86)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libswresample
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libpostproc
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
# only include if library file exists
ifneq (,$(wildcard $(LOCAL_SRC_FILES)))
include $(PREBUILT_SHARED_LIBRARY)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavfilter
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavdevice
LOCAL_SRC_FILES := $(PREFIX)/lib/$(LOCAL_MODULE).so
LOCAL_EXPORT_C_INCLUDES := $(PREFIX)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmpv
LOCAL_SRC_FILES := $(PREFIX)/lib/libmpv.so
LOCAL_EXPORT_C_INCLUDES := $(PREFIX)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libplayer
LOCAL_CFLAGS    := -Werror
LOCAL_CPPFLAGS  += -std=c++17

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
LOCAL_CFLAGS   += -O3 -march=armv8.2-a+crypto+dotprod+fp16+i8mm+bf16+sha3 \
                  -mtune=cortex-x4 \
                  -fno-math-errno -fno-trapping-math -ffp-contract=fast \
                  -fomit-frame-pointer -pipe \
                  -ffunction-sections -fdata-sections \
                  -fno-plt
LOCAL_CPPFLAGS += -fno-exceptions -fno-rtti
LOCAL_LDFLAGS  += -Wl,--gc-sections,--icf=safe,--as-needed \
                  -Wl,-z,now,-z,relro
endif

LOCAL_SRC_FILES := \
	main.cpp \
	render.cpp \
	log.cpp \
	jni_utils.cpp \
	property.cpp \
	event.cpp \
	node.cpp \
	thumbnail.cpp
LOCAL_LDLIBS    := -llog -lGLESv3 -lEGL -ljnigraphics
LOCAL_SHARED_LIBRARIES := swscale avcodec avformat avutil mpv

include $(BUILD_SHARED_LIBRARY)
