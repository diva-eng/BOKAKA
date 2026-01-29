#include "buzzer.h"
#include "platform_buzzer.h"
#include "platform_timing.h"

// =====================================================
// Predefined Melodies
// =====================================================

// Success melody: Ascending three-note arpeggio
const Buzzer::Note Buzzer::MELODY_SUCCESS[] = {
    { FREQ_LOW,  DUR_SHORT, 30 },   // Low note + short pause
    { FREQ_MID,  DUR_SHORT, 30 },   // Mid note + short pause
    { FREQ_HIGH, DUR_MEDIUM, 0 }    // High note, end
};

// Error melody: Descending two-note
const Buzzer::Note Buzzer::MELODY_ERROR[] = {
    { FREQ_MID, DUR_MEDIUM, 50 },   // Mid note + pause
    { FREQ_LOW, DUR_LONG,   0 }     // Low note, end
};

// =====================================================
// Constructor / Initialization
// =====================================================

Buzzer::Buzzer()
    : _pin(0)
    , _initialized(false)
    , _isPlaying(false)
    , _melody(nullptr)
    , _melodyLength(0)
    , _melodyIndex(0)
    , _melodyActive(false)
    , _inPause(false)
    , _noteStartMs(0)
    , _scheduledPending(false)
    , _scheduledStartMs(0)
    , _scheduledDelayMs(0)
{
}

void Buzzer::begin(uint32_t pin) {
    _pin = pin;
    platform_buzzer_init(pin);
    _initialized = true;
}

// =====================================================
// Predefined Feedback Tones
// =====================================================

void Buzzer::playDetectionTone() {
    if (!_initialized) {
        return;
    }
    // Short mid-frequency beep
    playTone(FREQ_MID, DUR_SHORT);
}

void Buzzer::playSuccessTone() {
    if (!_initialized) {
        return;
    }
    startMelody(MELODY_SUCCESS, MELODY_SUCCESS_LEN);
}

void Buzzer::playErrorTone() {
    if (!_initialized) {
        return;
    }
    startMelody(MELODY_ERROR, MELODY_ERROR_LEN);
}

void Buzzer::playConfirmTone() {
    if (!_initialized) {
        return;
    }
    playTone(FREQ_CONFIRM, DUR_MEDIUM);
}

void Buzzer::scheduleSuccessTone(uint32_t delayMs) {
    if (!_initialized) {
        return;
    }
    _scheduledPending = true;
    _scheduledStartMs = platform_millis();
    _scheduledDelayMs = delayMs;
}

// =====================================================
// Low-Level Tone Control
// =====================================================

void Buzzer::playTone(uint32_t frequencyHz, uint32_t durationMs) {
    if (!_initialized) {
        return;
    }
    
    platform_buzzer_tone_duration(frequencyHz, durationMs);
    _isPlaying = true;
    _noteStartMs = platform_millis();
}

void Buzzer::stop() {
    if (!_initialized) {
        return;
    }
    
    platform_buzzer_stop();
    _isPlaying = false;
    _melodyActive = false;
    _melody = nullptr;
    _scheduledPending = false;
}

// =====================================================
// Main Loop Update
// =====================================================

void Buzzer::loop() {
    if (!_initialized) {
        return;
    }

    // Update HAL state
    platform_buzzer_loop();

    // Check for scheduled tone
    if (_scheduledPending) {
        uint32_t elapsed = platform_millis() - _scheduledStartMs;
        if (elapsed >= _scheduledDelayMs) {
            _scheduledPending = false;
            playSuccessTone();
        }
    }

    // Update melody playback
    if (_melodyActive) {
        updateMelody();
    }

    // Update simple tone state
    if (_isPlaying && !_melodyActive) {
        _isPlaying = platform_buzzer_is_playing();
    }
}

// =====================================================
// Melody Playback
// =====================================================

void Buzzer::startMelody(const Note* notes, uint8_t noteCount) {
    if (!notes || noteCount == 0) {
        return;
    }

    _melody = notes;
    _melodyLength = noteCount;
    _melodyIndex = 0;
    _melodyActive = true;
    _inPause = false;

    // Start first note
    const Note& first = _melody[0];
    platform_buzzer_tone_duration(first.frequencyHz, first.durationMs);
    _noteStartMs = platform_millis();
    _isPlaying = true;
}

void Buzzer::updateMelody() {
    if (!_melodyActive || !_melody) {
        return;
    }

    uint32_t now = platform_millis();
    const Note& current = _melody[_melodyIndex];

    if (_inPause) {
        // Waiting in pause between notes
        uint32_t pauseElapsed = now - _noteStartMs;
        if (pauseElapsed >= current.pauseAfterMs) {
            // Move to next note
            _melodyIndex++;
            _inPause = false;

            if (_melodyIndex >= _melodyLength) {
                // Melody complete
                _melodyActive = false;
                _isPlaying = false;
                _melody = nullptr;
                return;
            }

            // Start next note
            const Note& next = _melody[_melodyIndex];
            platform_buzzer_tone_duration(next.frequencyHz, next.durationMs);
            _noteStartMs = now;
        }
    } else {
        // Playing a note
        uint32_t noteElapsed = now - _noteStartMs;
        if (noteElapsed >= current.durationMs) {
            // Note finished
            if (current.pauseAfterMs > 0) {
                // Enter pause
                platform_buzzer_stop();
                _inPause = true;
                _noteStartMs = now;
            } else {
                // No pause, check if more notes
                _melodyIndex++;
                if (_melodyIndex >= _melodyLength) {
                    // Melody complete
                    _melodyActive = false;
                    _isPlaying = false;
                    _melody = nullptr;
                } else {
                    // Start next note immediately
                    const Note& next = _melody[_melodyIndex];
                    platform_buzzer_tone_duration(next.frequencyHz, next.durationMs);
                    _noteStartMs = now;
                }
            }
        }
    }
}
