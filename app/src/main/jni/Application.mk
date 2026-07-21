APP_ABI :=
ifneq ($(PREFIX32),)
APP_ABI += armeabi-v7a
endif
ifneq ($(PREFIX64),)
APP_ABI += arm64-v8a
APP_CFLAGS += -O3 -march=armv8.2-a+crypto+dotprod+fp16+i8mm+bf16+sha3 -mtune=cortex-x4 -fno-math-errno -fomit-frame-pointer -pipe -ffunction-sections -fdata-sections
APP_LDFLAGS += -Wl,--gc-sections,--icf=safe
endif
ifneq ($(PREFIX_X64),)
APP_ABI += x86_64
endif
ifneq ($(PREFIX_X86),)
APP_ABI += x86
endif

APP_PLATFORM := android-24
APP_STL := c++_shared
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
