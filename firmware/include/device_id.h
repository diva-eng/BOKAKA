#pragma once
#include <stdint.h>
#include <cstddef>  // for size_t

constexpr size_t DEVICE_UID_LEN = 12;            // STM32 UID = 96 bits = 12 bytes
constexpr size_t DEVICE_UID_HEX_LEN = 24;        // 12 bytes -> 24 hex chars

// Read raw UID (12 bytes, big-endian order)
void getDeviceUidRaw(uint8_t out[DEVICE_UID_LEN]);

// Convert to a 24-character hex string (null-terminated with '\0' for convenient use with Serial/USB)
void getDeviceUidHex(char out[DEVICE_UID_HEX_LEN + 1]);

// Check whether a UID is all zeros (used to determine if selfId has been initialized)
bool isUidAllZero(const uint8_t uid[DEVICE_UID_LEN]);
