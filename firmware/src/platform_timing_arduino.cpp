// Platform timing implementation for Arduino framework
#include "platform_timing.h"
#include <Arduino.h>

void platform_timing_init() {
    // No-op for Arduino
}

uint32_t platform_millis() {
    return millis();
}

uint32_t platform_micros() {
    return micros();
}

void platform_delay_ms(uint32_t ms) {
    delay(ms);
}

void platform_delay_us(uint32_t us) {
    delayMicroseconds(us);
}
