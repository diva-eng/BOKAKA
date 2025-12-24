#pragma once
#include <stdint.h>

// =====================================================
// Platform Timing Abstraction
// =====================================================
// This abstraction layer allows easy migration between
// Arduino framework and STM32 HAL framework.
//
// Usage:
//   - Include this header instead of Arduino.h for timing
//   - Call platform_timing_init() once at startup
//   - Use platform_millis(), platform_micros(), etc.
// =====================================================

// Initialize timing system (call once at startup)
void platform_timing_init();

// Get milliseconds since startup (wraps every ~49 days)
uint32_t platform_millis();

// Get microseconds since startup (wraps every ~71 minutes)
uint32_t platform_micros();

// Blocking delay in milliseconds
void platform_delay_ms(uint32_t ms);

// Blocking delay in microseconds (may have limited precision)
void platform_delay_us(uint32_t us);
