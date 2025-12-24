// Platform storage implementation for Arduino framework
#include "platform_storage.h"
#include <EEPROM.h>
#include <cstddef>  // for size_t

static size_t g_storage_size = 0;

bool platform_storage_begin(size_t size) {
    g_storage_size = size;
    // STM32 Arduino EEPROM.begin() doesn't take a size parameter
    // It uses the size configured in the EEPROM library
    EEPROM.begin();
    return true;
}

uint8_t platform_storage_read(size_t address) {
    if (address >= g_storage_size) {
        return 0;
    }
    return EEPROM.read(address);
}

void platform_storage_write(size_t address, uint8_t value) {
    if (address >= g_storage_size) {
        return;
    }
    EEPROM.write(address, value);
}

bool platform_storage_commit() {
    // Arduino EEPROM library may auto-commit or require explicit commit
    // For STM32 Arduino cores, commit() may not exist, so we check
    // This is a no-op if commit() doesn't exist
    #ifdef EEPROM_COMMIT_AVAILABLE
    EEPROM.commit();
    #endif
    return true;
}
