APP_ABI :=
ifneq ($(PREFIX32),)
APP_ABI += armeabi-v7a
endif
ifneq ($(PREFIX64),)
APP_ABI += arm64-v8a
# ── Snapdragon 8s Gen 3 / Cortex-X4 optimisation flags ──────────────────────
APP_CFLAGS  += -O3 -march=armv8.2-a+crypto+dotprod+fp16+i8mm+bf16+sha3 \
               -mtune=cortex-x4 \
               -fno-math-errno -fno-trapping-math -ffp-contract=fast \
               -fomit-frame-pointer -pipe \
               -ffunction-sections -fdata-sections \
               -fno-plt \
               -flto=thin
APP_CPPFLAGS += -fno-exceptions -fno-rtti -std=c++17
APP_LDFLAGS  += -Wl,--gc-sections,--icf=safe,--as-needed \
                -Wl,-z,now,-z,relro \
                -flto=thin
endif
ifneq ($(PREFIX_X64),)
APP_ABI += x86_64
endif
ifneq ($(PREFIX_X86),)
APP_ABI += x86
endif

APP_PLATFORM := android-24
APP_STL      := c++_shared
APP_OPTIM    := release
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
