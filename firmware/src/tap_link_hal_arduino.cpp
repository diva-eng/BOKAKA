#include "tap_link_hal.h"
#include "board_config.h"
#include "platform_timing.h"
#include "platform_gpio.h"

// Arduino implementation for 1-wire tap link interface

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
            platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_OUTPUT);
            platform_gpio_write(_pin, PLATFORM_GPIO_LOW);
        } else {
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
