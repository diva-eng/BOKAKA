#include "tap_link_hal.h"
#include "platform_timing.h"
#include "platform_gpio.h"
#include <Arduino.h>  // Needed for PA9 pin definition

// Arduino implementation for 1-wire tap link interface
// Configure the GPIO pin for the 1-wire interface
// Default: PA9 (D8 on NUCLEO-L053R8, digital header CN5)
// This pin is on the digital header for easy access and supports open-drain mode

#ifndef TAP_LINK_PIN
#define TAP_LINK_PIN PA9
#endif

class OneWireHalArduino : public IOneWireHal {
private:
    uint32_t _pin;
    
public:
    OneWireHalArduino(uint32_t pin) : _pin(pin) {
        // Configure pin as input with pull-up initially (Hi-Z)
        platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_INPUT_PULLUP);
    }
    
    bool readLine() override {
        // Read the physical line state
        // true = high, false = low
        return platform_gpio_read(_pin);
    }
    
    void driveLow(bool enableLow) override {
        if (enableLow) {
            // Actively pull line low (open-drain)
            // Note: Arduino OUTPUT mode with LOW acts as open-drain when hardware supports it
            platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_OUTPUT);
            platform_gpio_write(_pin, PLATFORM_GPIO_LOW);
        } else {
            // Release line (input / Hi-Z with pull-up)
            platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_INPUT_PULLUP);
        }
    }
    
    uint32_t micros() override {
        return platform_micros();
    }
    
    void delayMicros(uint32_t us) override {
        platform_delay_us(us);
    }
};

// Global instance (will be initialized in setup)
static OneWireHalArduino* g_oneWireHal = nullptr;

// Factory function to create HAL instance
IOneWireHal* createOneWireHal() {
    if (g_oneWireHal == nullptr) {
        g_oneWireHal = new OneWireHalArduino(TAP_LINK_PIN);
    }
    return g_oneWireHal;
}
