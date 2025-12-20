// tap_link_hal.h
#pragma once
#include <stdint.h>

struct IOneWireHal {
    virtual ~IOneWireHal() = default;

    // Read physical line: true = high, false = low
    virtual bool readLine() = 0;

    // Control driver: when enableLow = true, actively pull line low (open-drain)
    // when false, release (input / Hi-Z).
    virtual void driveLow(bool enableLow) = 0;

    // Time utilities
    virtual uint32_t micros() = 0;
    virtual void delayMicros(uint32_t us) = 0;
};

// Factory function to create HAL instance (platform-specific)
IOneWireHal* createOneWireHal();
