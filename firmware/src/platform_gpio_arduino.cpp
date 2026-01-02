// Platform GPIO implementation for Arduino framework
#include "platform_gpio.h"
#include <Arduino.h>

void platform_gpio_pin_mode(uint32_t pin, platform_gpio_mode_t mode) {
    switch (mode) {
        case PLATFORM_GPIO_MODE_INPUT:
            pinMode(pin, INPUT);
            break;
        case PLATFORM_GPIO_MODE_INPUT_PULLUP:
            pinMode(pin, INPUT_PULLUP);
            break;
        case PLATFORM_GPIO_MODE_OUTPUT:
            pinMode(pin, OUTPUT);
            break;
        case PLATFORM_GPIO_MODE_OUTPUT_OD:
            // Open-drain emulated via OUTPUT mode
            pinMode(pin, OUTPUT);
            break;
    }
}

bool platform_gpio_read(uint32_t pin) {
    return digitalRead(pin) == HIGH;
}

void platform_gpio_write(uint32_t pin, platform_gpio_state_t state) {
    digitalWrite(pin, state == PLATFORM_GPIO_HIGH ? HIGH : LOW);
}
