// Platform buzzer implementation for Arduino framework
// Target: HS-F02A passive piezo buzzer (PWM-driven)
#include "platform_buzzer.h"
#include "platform_timing.h"
#include <Arduino.h>

// Module state
static uint32_t g_buzzerPin = 0;
static bool g_initialized = false;
static bool g_isPlaying = false;
static uint32_t g_toneStartMs = 0;
static uint32_t g_toneDurationMs = 0;

void platform_buzzer_init(uint32_t pin) {
    g_buzzerPin = pin;
    g_initialized = true;
    g_isPlaying = false;
    g_toneStartMs = 0;
    g_toneDurationMs = 0;
    
    // Configure pin as output (tone() will handle PWM)
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void platform_buzzer_tone(uint32_t frequencyHz) {
    if (!g_initialized) {
        return;
    }
    
    if (frequencyHz == 0) {
        platform_buzzer_stop();
        return;
    }
    
    tone(g_buzzerPin, frequencyHz);
    g_isPlaying = true;
    g_toneDurationMs = 0;  // Continuous until stopped
}

void platform_buzzer_tone_duration(uint32_t frequencyHz, uint32_t durationMs) {
    if (!g_initialized) {
        return;
    }
    
    if (frequencyHz == 0 || durationMs == 0) {
        platform_buzzer_stop();
        return;
    }
    
    // Use Arduino's built-in timed tone
    tone(g_buzzerPin, frequencyHz, durationMs);
    g_isPlaying = true;
    g_toneStartMs = platform_millis();
    g_toneDurationMs = durationMs;
}

void platform_buzzer_stop() {
    if (!g_initialized) {
        return;
    }
    
    noTone(g_buzzerPin);
    g_isPlaying = false;
    g_toneDurationMs = 0;
}

bool platform_buzzer_is_playing() {
    return g_isPlaying;
}

void platform_buzzer_loop() {
    if (!g_initialized || !g_isPlaying) {
        return;
    }
    
    // Check if timed tone has expired
    if (g_toneDurationMs > 0) {
        uint32_t elapsed = platform_millis() - g_toneStartMs;
        if (elapsed >= g_toneDurationMs) {
            g_isPlaying = false;
            g_toneDurationMs = 0;
            // Note: Arduino's tone() with duration auto-stops,
            // we just update our tracking state
        }
    }
}
