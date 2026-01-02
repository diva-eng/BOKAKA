// =====================================================
// Platform Device - Arduino/STM32 Implementation
// =====================================================
// Uses STM32 HAL to read device unique ID.
// This works in Arduino framework because Arduino STM32
// includes the HAL headers.
// =====================================================

#include "platform_device.h"
#include "stm32l0xx_hal.h"

void platform_get_device_uid(uint8_t out[PLATFORM_DEVICE_UID_SIZE]) {
    // STM32L0 UID: 96-bit, split into three 32-bit words
    // Located at specific memory addresses, accessed via HAL functions
    uint32_t w0 = HAL_GetUIDw0();
    uint32_t w1 = HAL_GetUIDw1();
    uint32_t w2 = HAL_GetUIDw2();

    // Split into 12 bytes in big-endian order
    // (consistent ordering for ID comparison and storage)
    out[0]  = (w0 >> 24) & 0xFF;
    out[1]  = (w0 >> 16) & 0xFF;
    out[2]  = (w0 >>  8) & 0xFF;
    out[3]  = (w0 >>  0) & 0xFF;

    out[4]  = (w1 >> 24) & 0xFF;
    out[5]  = (w1 >> 16) & 0xFF;
    out[6]  = (w1 >>  8) & 0xFF;
    out[7]  = (w1 >>  0) & 0xFF;

    out[8]  = (w2 >> 24) & 0xFF;
    out[9]  = (w2 >> 16) & 0xFF;
    out[10] = (w2 >>  8) & 0xFF;
    out[11] = (w2 >>  0) & 0xFF;
}

