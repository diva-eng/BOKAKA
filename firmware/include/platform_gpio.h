#pragma once
#include <stdint.h>
#include <stdbool.h>

// =====================================================
// Platform GPIO Abstraction
// =====================================================
// This abstraction layer allows easy migration between
// Arduino GPIO and STM32 HAL GPIO.
//
// Usage:
//   - Include this header instead of Arduino.h for GPIO
//   - Use platform_gpio_* functions for pin operations
// =====================================================

// GPIO pin modes
typedef enum {
    PLATFORM_GPIO_MODE_INPUT,
    PLATFORM_GPIO_MODE_INPUT_PULLUP,
    PLATFORM_GPIO_MODE_OUTPUT,
    PLATFORM_GPIO_MODE_OUTPUT_OD  // Open-drain output
} platform_gpio_mode_t;

// GPIO pin states
typedef enum {
    PLATFORM_GPIO_LOW = 0,
    PLATFORM_GPIO_HIGH = 1
} platform_gpio_state_t;

// Configure a GPIO pin
// pin: Platform-specific pin identifier (e.g., PA0 for Arduino, GPIO_PIN_0 for HAL)
// mode: Pin mode (input, output, etc.)
void platform_gpio_pin_mode(uint32_t pin, platform_gpio_mode_t mode);

// Read GPIO pin state
// pin: Platform-specific pin identifier
// Returns: true if HIGH, false if LOW
bool platform_gpio_read(uint32_t pin);

// Write GPIO pin state
// pin: Platform-specific pin identifier
// state: HIGH or LOW
void platform_gpio_write(uint32_t pin, platform_gpio_state_t state);
