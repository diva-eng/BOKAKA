#pragma once
#include <stdint.h>
#include <stdbool.h>

// =====================================================
// Platform Buzzer Abstraction (PWM-based)
// =====================================================
// This abstraction layer allows easy migration between
// Arduino PWM tone generation and STM32 HAL timer/PWM.
//
// Target: HS-F02A passive piezo buzzer (or similar)
// - Recommended frequency range: 2000-4000 Hz
// - Typical resonant frequency: ~2700 Hz
//
// Usage:
//   - Call platform_buzzer_init(pin) once at startup
//   - Use platform_buzzer_tone() to play a frequency
//   - Use platform_buzzer_stop() to stop playback
// =====================================================

// Initialize the buzzer on the specified pin
// pin: Platform-specific pin identifier (e.g., PC7 for Arduino, TIM channel for HAL)
void platform_buzzer_init(uint32_t pin);

// Play a tone at the specified frequency
// frequencyHz: Tone frequency in Hz (0 to stop)
void platform_buzzer_tone(uint32_t frequencyHz);

// Play a tone for a specific duration (non-blocking)
// frequencyHz: Tone frequency in Hz
// durationMs: Duration in milliseconds (0 = continuous until stop)
void platform_buzzer_tone_duration(uint32_t frequencyHz, uint32_t durationMs);

// Stop any currently playing tone
void platform_buzzer_stop();

// Check if a timed tone is still playing
// Returns: true if tone is active, false if silent
bool platform_buzzer_is_playing();

// Update buzzer state (call from main loop for timed tones)
// This handles stopping timed tones after their duration expires
void platform_buzzer_loop();
