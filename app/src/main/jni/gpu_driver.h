#pragma once

#include <jni.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the custom vkGetInstanceProcAddr entry point resolved from
 * the adrenotools-hooked libvulkan.so instance, or NULL if standard/stock driver is in use.
 */
PFN_vkGetInstanceProcAddr get_adrenotools_vk_proc_addr(void);

#ifdef __cplusplus
}
#endif

