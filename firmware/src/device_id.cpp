#include "device_id.h"
#include "platform_device.h"

static char hexDigit(uint8_t v) {
    return v < 10 ? ('0' + v) : ('A' + (v - 10));
}

void getDeviceUidRaw(uint8_t out[DEVICE_UID_LEN]) {
    // Delegate to platform abstraction
    platform_get_device_uid(out);
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
