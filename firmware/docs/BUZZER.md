# Buzzer Documentation

## Overview

The `Buzzer` class provides audio feedback for the Bokaka firmware using a passive piezo buzzer (HS-F02A or similar). It uses the platform abstraction layer (`platform_buzzer.h`) for PWM tone generation, enabling easy migration between Arduino and STM32 HAL.

## Features

- **Platform-Independent**: Uses PWM abstraction for tone generation
- **Non-Blocking**: Melody playback handled in main loop
- **Multi-Note Melodies**: Supports sequences with pauses between notes
- **Scheduled Tones**: Delay tone playback for fast events
- **Configurable Pin**: Override via build flags

---

## Hardware

### HS-F02A Passive Piezo Buzzer

| Specification | Value |
|---------------|-------|
| Type | Passive (requires PWM) |
| Resonant Frequency | ~2700 Hz |
| Effective Range | 2000–4000 Hz |
| Operating Voltage | 3.3V–5V |
| Current | ~30mA max |

### Wiring (NUCLEO-L053R8)

| Buzzer Pin | Board Pin | Arduino Name | STM32 Pin |
|------------|-----------|--------------|-----------|
| + (Signal) | D9 | PC7 | PC7 (TIM3_CH2 / TIM22_CH2) |
| − (Ground) | GND | GND | GND |

> **Note**: D9 on NUCLEO-L053R8 is PWM-capable via TIM3 or TIM22.

---

## Feedback Tones

### Tone Events

| Event | Tone | Description |
|-------|------|-------------|
| **Detection** | Short beep | Plays when devices first touch |
| **Success** | Ascending arpeggio | Plays after successful ID exchange |
| **Error** | Descending tone | Plays on communication error |
| **Confirm** | Single beep | General confirmation |

### Tone Specifications

| Tone | Frequencies | Durations | Total Time |
|------|-------------|-----------|------------|
| Detection | 2700 Hz | 50ms | ~50ms |
| Success | 2000 → 2700 → 3500 Hz | 50ms + 30ms pause each | ~240ms |
| Error | 2700 → 2000 Hz | 100ms + 50ms, 200ms | ~350ms |
| Confirm | 3200 Hz | 100ms | ~100ms |

### Timing Diagram

```
Detection Tone:
    ┌──────┐
    │ 2700 │
────┘      └────────────────────────────────────

Success Melody (with 150ms delay after ID exchange):
              ┌────┐   ┌────┐   ┌──────┐
              │2000│   │2700│   │ 3500 │
──────────────┘    └───┘    └───┘      └────────
   150ms delay   50ms 30ms 50ms 30ms  100ms
```

---

## API Reference

### Initialization

```cpp
#include "buzzer.h"

Buzzer buzzer;

void setup() {
    buzzer.begin(BUZZER_PIN);  // PC7 (D9) by default
}
```

### Main Loop

```cpp
void loop() {
    buzzer.loop();  // Must call to update melody playback
}
```

### Playing Tones

```cpp
// Predefined feedback tones
buzzer.playDetectionTone();   // Short beep on detection
buzzer.playSuccessTone();     // Ascending melody
buzzer.playErrorTone();       // Descending melody
buzzer.playConfirmTone();     // Single confirmation beep

// Scheduled tone (plays after delay)
buzzer.scheduleSuccessTone(150);  // Play success tone after 150ms

// Low-level control
buzzer.playTone(2700, 100);   // 2700 Hz for 100ms
buzzer.stop();                // Stop any playing tone
```

### State Queries

```cpp
if (buzzer.isPlaying()) {
    // Buzzer is currently active
}
```

---

## Integration Example

### With Tap Link Events

```cpp
void loop() {
    tapLink.poll();
    
    // Play detection tone when devices touch
    if (tapLink.isConnectionDetected()) {
        buzzer.playDetectionTone();
    }
    
    // Schedule success tone after ID exchange
    // (delayed because exchange happens quickly)
    if (idExchangeComplete) {
        buzzer.scheduleSuccessTone(150);
    }
    
    buzzer.loop();  // Update buzzer state
}
```

---

## Configuration

### Pin Configuration

Default pin is defined in `board_config.h`:

```cpp
#ifndef BUZZER_PIN
#ifdef PC7
#define BUZZER_PIN PC7        // Arduino: PC7 (D9) - PWM capable
#else
#define BUZZER_PIN 9          // Generic: GPIO 9
#endif
#endif
```

Override via build flags in `platformio.ini`:

```ini
build_flags =
    -DBUZZER_PIN=PA0
```

### Frequency Constants

Defined in `buzzer.h`:

```cpp
static constexpr uint32_t FREQ_LOW     = 2000;   // Low tone
static constexpr uint32_t FREQ_MID     = 2700;   // Mid tone (resonant)
static constexpr uint32_t FREQ_HIGH    = 3500;   // High tone
static constexpr uint32_t FREQ_CONFIRM = 3200;   // Confirmation tone
```

### Duration Constants

```cpp
static constexpr uint32_t DUR_SHORT    = 50;     // Short beep
static constexpr uint32_t DUR_MEDIUM   = 100;    // Medium beep
static constexpr uint32_t DUR_LONG     = 200;    // Long beep
```

---

## Platform Abstraction

### Arduino Implementation

The Arduino implementation (`platform_buzzer_arduino.cpp`) uses the built-in `tone()` and `noTone()` functions:

```cpp
void platform_buzzer_tone(uint32_t frequencyHz) {
    tone(g_buzzerPin, frequencyHz);
}

void platform_buzzer_stop() {
    noTone(g_buzzerPin);
}
```

### STM32 HAL Migration

For STM32CubeIDE, implement `platform_buzzer_hal.c` using timer PWM:

```c
// Example using TIM3 CH2 on PC7
void platform_buzzer_init(uint32_t pin) {
    // Configure TIM3 for PWM output on CH2
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

void platform_buzzer_tone(uint32_t frequencyHz) {
    uint32_t period = SystemCoreClock / frequencyHz;
    __HAL_TIM_SET_AUTORELOAD(&htim3, period - 1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, period / 2);  // 50% duty
}

void platform_buzzer_stop() {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
}
```

---

## Extending

### Adding a New Melody

1. Define notes in `buzzer.cpp`:

```cpp
const Buzzer::Note Buzzer::MELODY_CUSTOM[] = {
    { 3000, 80, 20 },   // 3000 Hz for 80ms, 20ms pause
    { 2500, 80, 20 },   // 2500 Hz for 80ms, 20ms pause
    { 2000, 150, 0 }    // 2000 Hz for 150ms, end
};
static constexpr uint8_t MELODY_CUSTOM_LEN = 3;
```

2. Add method in `buzzer.h`:

```cpp
void playCustomTone();
```

3. Implement in `buzzer.cpp`:

```cpp
void Buzzer::playCustomTone() {
    if (!_initialized) return;
    startMelody(MELODY_CUSTOM, MELODY_CUSTOM_LEN);
}
```

---

## Troubleshooting

| Issue | Check |
|-------|-------|
| No sound | Verify `begin()` called with correct pin |
| Buzzer always on | Ensure `loop()` is called in main loop |
| Wrong frequency | Check if buzzer is passive (active buzzers ignore frequency) |
| Melody cuts off | Verify `loop()` polling rate is fast enough (<10ms) |
| Scheduled tone not playing | Confirm `loop()` is called after scheduling |

---

## File Structure

```
include/
├── platform_buzzer.h      # Platform abstraction interface
├── buzzer.h               # High-level Buzzer class
└── board_config.h         # Pin definitions (BUZZER_PIN)

src/
├── platform_buzzer_arduino.cpp  # Arduino implementation
└── buzzer.cpp                   # Buzzer class implementation
```

---

*Last Updated: January 2026*
