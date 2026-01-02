#pragma once
// =====================================================
// Platform Device Abstraction
// =====================================================
// Provides platform-independent access to device-specific
// hardware features like unique ID.
// =====================================================

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Device UID size (STM32 = 96 bits = 12 bytes)
#define PLATFORM_DEVICE_UID_SIZE 12

// =====================================================
// Device Unique ID
// =====================================================

// Read device unique ID (96 bits = 12 bytes, big-endian)
// The UID is guaranteed unique per device and persists across resets.
void platform_get_device_uid(uint8_t out[PLATFORM_DEVICE_UID_SIZE]);

#ifdef __cplusplus
}
#endif

