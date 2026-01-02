# Status Display Documentation

## Overview

The `StatusDisplay` class provides a platform-agnostic LED status indication system for the Bokaka firmware. It uses the platform abstraction layer (`platform_gpio.h` and `platform_timing.h`) to drive LEDs with configurable blink patterns.

## Features

- **Platform-Independent**: Uses GPIO and timing abstractions
- **Extensible**: Easy to add new patterns or LEDs
- **Non-Blocking**: Pattern updates happen in the main loop
- **Multiple LEDs**: Supports up to 4 LEDs (configurable via `MAX_LEDS`)
- **Pattern-Based**: Blink patterns defined as reusable sequences

---

## LED Assignment

| LED | Function | States |
|-----|----------|--------|
| **LED 0** | Device readiness/handshake status | Boot, Idle, Detecting, Negotiating, Exchanging, Success, Error |
| **LED 1** | Master/Slave role indication | Steady ON = Master, Steady OFF = Slave, Blinking = Unknown |

---

## Pattern Definitions

### Ready Patterns (LED 0)

| Pattern | Description | Blink Sequence |
|---------|-------------|----------------|
| **Booting** | Device startup | 120ms ON, 380ms OFF |
| **Idle** | Waiting for connection | 120ms ON, 880ms OFF |
| **Detecting** | Connection detected | 120ms ON, 120ms OFF, 120ms ON, 640ms OFF |
| **Negotiating** | Master/slave negotiation | 150ms ON, 150ms OFF (fast) |
| **WaitingAck** | Waiting for ACK | 80ms ON, 120ms OFF, 80ms ON, 720ms OFF |
| **Exchanging** | Exchanging data | 220ms ON, 220ms OFF |
| **Success** | Connection successful | 500ms ON, 500ms OFF |
| **Error** | Error state | Triple blink (80ms ON/OFF × 3, 500ms OFF) |

### Role Patterns (LED 1)

| Pattern | Description | LED State |
|---------|-------------|-----------|
| **Unknown** | Role not determined | 90ms ON, 910ms OFF |
| **Master** | Device is master | Steady ON |
| **Slave** | Device is slave | Steady OFF |

---

## API Reference

### Initialization

```cpp
#include "status_display.h"

StatusDisplay gStatusDisplay;

void setup() {
    static const uint32_t LED_PINS[] = { STATUS_LED0_PIN, STATUS_LED1_PIN };
    gStatusDisplay.begin(LED_PINS, 2);
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
}
```

### Main Loop

```cpp
void loop() {
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Idle);
    gStatusDisplay.loop();  // Must call to update LED patterns
    platform_delay_ms(5);
}
```

### Setting Patterns

```cpp
// Ready patterns (LED 0)
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Idle);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Detecting);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Negotiating);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);

// Role patterns (LED 1)
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);
```

---

## Configuration

### Pin Configuration

Default pins are defined in `status_display.h`:

```cpp
#ifndef STATUS_LED0_PIN
#define STATUS_LED0_PIN 13  // Common built-in LED
#endif

#ifndef STATUS_LED1_PIN
#define STATUS_LED1_PIN 12
#endif
```

Override via build flags in `platformio.ini`:

```ini
build_flags =
    -DSTATUS_LED0_PIN=PA5
    -DSTATUS_LED1_PIN=PA6
```

---

## Extending

### Adding a New Pattern

1. Define blink steps in `status_display.cpp`:

```cpp
static constexpr StatusDisplay::BlinkStep PATTERN_NEW_STEPS[] = {
    {100, true},   // 100ms ON
    {200, false},  // 200ms OFF
};
```

2. Add to enum in `status_display.h`:

```cpp
enum class ReadyPattern {
    // ... existing ...
    NewPattern
};
```

3. Map in `patternFor()`:

```cpp
case ReadyPattern::NewPattern:
    return NEW;
```

---

## Troubleshooting

| Issue | Check |
|-------|-------|
| LEDs not blinking | Verify `begin()` and `loop()` are called |
| Pattern not changing | Ensure setter is called, check enum value |
| Wrong LED behavior | Verify pin mapping and polarity |

---

*Last Updated: January 2026*

