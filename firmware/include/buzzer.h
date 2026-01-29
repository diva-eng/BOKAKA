#pragma once
#include <stdint.h>
#include <stdbool.h>

// =====================================================
// Buzzer Controller
// =====================================================
// High-level buzzer interface for feedback tones.
// Uses platform_buzzer HAL for PWM tone generation.
//
// Target: HS-F02A passive piezo buzzer
// - Resonant frequency: ~2700 Hz
// - Effective range: 2000-4000 Hz
//
// Tone Events:
// - Detection: Short beep when connection detected
// - Success: Ascending melody after ID exchange
// =====================================================

class Buzzer {
public:
    // Predefined tone frequencies (Hz)
    static constexpr uint32_t FREQ_LOW     = 2000;   // Low tone
    static constexpr uint32_t FREQ_MID     = 2700;   // Mid tone (resonant)
    static constexpr uint32_t FREQ_HIGH    = 3500;   // High tone
    static constexpr uint32_t FREQ_CONFIRM = 3200;   // Confirmation tone

    // Predefined durations (ms)
    static constexpr uint32_t DUR_SHORT    = 50;     // Short beep
    static constexpr uint32_t DUR_MEDIUM   = 100;    // Medium beep
    static constexpr uint32_t DUR_LONG     = 200;    // Long beep

    Buzzer();

    // Initialize buzzer on specified pin
    void begin(uint32_t pin);

    // Play predefined feedback tones
    void playDetectionTone();         // Short beep on detection
    void playSuccessTone();           // Ascending melody on ID exchange success
    void playErrorTone();             // Descending tone on error
    void playConfirmTone();           // Single confirmation beep

    // Schedule a success tone with delay (for quick events)
    // delayMs: Delay before playing the tone
    void scheduleSuccessTone(uint32_t delayMs);

    // Low-level tone control
    void playTone(uint32_t frequencyHz, uint32_t durationMs);
    void stop();

    // Update buzzer state - call from main loop
    // Handles multi-note melodies and scheduled tones
    void loop();

    // Check if buzzer is currently active
    bool isPlaying() const { return _isPlaying || _melodyActive || _scheduledPending; }

private:
    // Melody note structure
    struct Note {
        uint32_t frequencyHz;
        uint32_t durationMs;
        uint32_t pauseAfterMs;   // Silence after this note
    };

    // Melody playback state
    void startMelody(const Note* notes, uint8_t noteCount);
    void updateMelody();

    uint32_t _pin;
    bool _initialized;
    bool _isPlaying;

    // Melody state
    const Note* _melody;
    uint8_t _melodyLength;
    uint8_t _melodyIndex;
    bool _melodyActive;
    bool _inPause;                 // Currently in pause between notes
    uint32_t _noteStartMs;

    // Scheduled tone state
    bool _scheduledPending;
    uint32_t _scheduledStartMs;
    uint32_t _scheduledDelayMs;

    // Predefined melodies
    static const Note MELODY_SUCCESS[];
    static const Note MELODY_ERROR[];
    static constexpr uint8_t MELODY_SUCCESS_LEN = 3;
    static constexpr uint8_t MELODY_ERROR_LEN = 2;
};
