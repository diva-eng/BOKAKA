#include "device_id.h"
#include "stm32l0xx_hal.h"   // HAL_GetUIDw0/1/2 are defined here

static char hexDigit(uint8_t v) {
    return v < 10 ? ('0' + v) : ('A' + (v - 10));
}

void getDeviceUidRaw(uint8_t out[DEVICE_UID_LEN]) {
    // STM32L0 UID: 96-bit, split into three 32-bit words
    uint32_t w0 = HAL_GetUIDw0();
    uint32_t w1 = HAL_GetUIDw1();
    uint32_t w2 = HAL_GetUIDw2();

    // Split into 12 bytes here in big-endian order; as long as you use the same order later it's fine
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
    out[10] = (w2 >> 8) & 0xFF;
    out[11] = (w2 >> 0) & 0xFF;
}

void getDeviceUidHex(char out[DEVICE_UID_HEX_LEN + 1]) {
    uint8_t raw[DEVICE_UID_LEN];
    getDeviceUidRaw(raw);

    for (size_t i = 0; i < DEVICE_UID_LEN; ++i) {
        uint8_t b = raw[i];
        out[2 * i]     = hexDigit((b >> 4) & 0x0F);
        out[2 * i + 1] = hexDigit(b & 0x0F);
    }
    out[DEVICE_UID_HEX_LEN] = '\0';
}

bool isUidAllZero(const uint8_t uid[DEVICE_UID_LEN]) {
    for (size_t i = 0; i < DEVICE_UID_LEN; ++i) {
        if (uid[i] != 0) return false;
    }
    return true;
}
