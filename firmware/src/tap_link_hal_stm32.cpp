#include "tap_link_hal.h"
#include <Arduino.h>

// STM32 HAL implementation for 1-wire tap link interface
// Configure the GPIO pin for the 1-wire interface
// Default: PA0 (can be changed via platformio.ini build flags)

#ifndef TAP_LINK_PIN
#define TAP_LINK_PIN PA0
#endif

class OneWireHalSTM32 : public IOneWireHal {
public:
    OneWireHalSTM32() {
        // Configure pin as open-drain output with pull-up
        // Initially in input mode (Hi-Z)
        pinMode(TAP_LINK_PIN, INPUT_PULLUP);
    }
    
    bool readLine() override {
        // Read the physical line state
        // true = high, false = low
        return digitalRead(TAP_LINK_PIN) == HIGH;
    }
    
    void driveLow(bool enableLow) override {
        if (enableLow) {
            // Actively pull line low (open-drain)
            pinMode(TAP_LINK_PIN, OUTPUT);
            digitalWrite(TAP_LINK_PIN, LOW);
        } else {
            // Release line (input / Hi-Z with pull-up)
            pinMode(TAP_LINK_PIN, INPUT_PULLUP);
        }
    }
    
    uint32_t micros() override {
        return ::micros();
    }
    
    void delayMicros(uint32_t us) override {
        delayMicroseconds(us);
    }
};

// Global instance (will be initialized in setup)
static OneWireHalSTM32* g_oneWireHal = nullptr;

// Factory function to create HAL instance
IOneWireHal* createOneWireHal() {
    if (g_oneWireHal == nullptr) {
        g_oneWireHal = new OneWireHalSTM32();
    }
    return g_oneWireHal;
}
