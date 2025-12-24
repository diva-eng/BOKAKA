# Status Display Documentation

## Overview

The `StatusDisplay` class provides a platform-agnostic LED status indication system for the Bokaka-Eval firmware. It uses the platform abstraction layer (`platform_gpio.h` and `platform_timing.h`) to drive LEDs with configurable blink patterns, making it easy to migrate between Arduino and STM32 HAL implementations.

## Features

- **Platform-Independent**: Uses GPIO and timing abstractions, no direct hardware dependencies
- **Extensible**: Easy to add new patterns, states, or LEDs without modifying core driver code
- **Non-Blocking**: Pattern updates happen in the main loop without blocking operations
- **Multiple LEDs**: Supports up to 4 LEDs (configurable via `MAX_LEDS`)
- **Pattern-Based**: Blink patterns defined as reusable sequences

## Architecture

### LED Assignment

- **LED 0**: Device readiness and handshake status
  - Shows boot, idle, detecting, negotiating, waiting, exchanging, success, and error states
- **LED 1**: Master/Slave role indication
  - Steady ON = Master
  - Steady OFF = Slave
  - Blinking = Unknown/Not determined

### Pattern System

Patterns are defined as sequences of `BlinkStep` structures:
```cpp
struct BlinkStep {
    uint16_t durationMs;  // How long this step lasts
    bool levelHigh;        // LED state (true = ON, false = OFF)
};
```

Patterns can be:
- **Blinking**: Sequence of on/off steps that repeat
- **Steady**: Constant state (ON or OFF)

## API Reference

### Initialization

```cpp
#include "status_display.h"

StatusDisplay gStatusDisplay;

void setup() {
    // Define LED pins
    static const uint32_t LED_PINS[] = { STATUS_LED0_PIN, STATUS_LED1_PIN };
    
    // Initialize with 2 LEDs
    gStatusDisplay.begin(LED_PINS, 2);
    
    // Set initial patterns
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
}
```

### Main Loop

```cpp
void loop() {
    // Update status based on application state
    updateStatusDisplay();
    
    // Must call loop() to update LED patterns
    gStatusDisplay.loop();
    
    platform_delay_ms(5);
}
```

### Setting Patterns

#### Ready Pattern (LED 0)

```cpp
// Set device readiness/handshake pattern
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Idle);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Detecting);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Negotiating);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::WaitingAck);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Exchanging);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);
gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Error);
```

#### Role Pattern (LED 1)

```cpp
// Set master/slave role indication
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);
gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);
```

## Pattern Definitions

### Ready Patterns (LED 0)

| Pattern | Description | Blink Sequence |
|---------|-------------|----------------|
| **Booting** | Device startup | 120ms ON, 380ms OFF (repeat) |
| **Idle** | Waiting for connection | 120ms ON, 880ms OFF (repeat) |
| **Detecting** | Connection detected | 120ms ON, 120ms OFF, 120ms ON, 640ms OFF (repeat) |
| **Negotiating** | Master/slave negotiation | 150ms ON, 150ms OFF (fast blink) |
| **WaitingAck** | Waiting for ACK | 80ms ON, 120ms OFF, 80ms ON, 720ms OFF (double blink) |
| **Exchanging** | Exchanging data | 220ms ON, 220ms OFF (medium blink) |
| **Success** | Connection successful | 500ms ON, 500ms OFF (slow blink) |
| **Error** | Error state | 80ms ON, 80ms OFF, 80ms ON, 80ms OFF, 80ms ON, 500ms OFF (triple blink) |

### Role Patterns (LED 1)

| Pattern | Description | LED State |
|---------|-------------|-----------|
| **Unknown** | Role not determined | 90ms ON, 910ms OFF (very slow blink) |
| **Master** | Device is master | Steady ON |
| **Slave** | Device is slave | Steady OFF |

## Integration Example

### With TapLink State Machine

```cpp
#include "status_display.h"
#include "tap_link.h"

StatusDisplay gStatusDisplay;
TapLink* gTapLink = nullptr;

// Map TapLink states to status patterns
static StatusDisplay::ReadyPattern readyPatternForState(TapLink::State state) {
    switch (state) {
        case TapLink::State::Detecting:
            return StatusDisplay::ReadyPattern::Detecting;
        case TapLink::State::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;
        case TapLink::State::MasterWaitAck:
            return StatusDisplay::ReadyPattern::WaitingAck;
        case TapLink::State::MasterDone:
        case TapLink::State::SlaveReceive:
        case TapLink::State::SlaveSend:
        case TapLink::State::SlaveDone:
            return StatusDisplay::ReadyPattern::Exchanging;
        case TapLink::State::Error:
            return StatusDisplay::ReadyPattern::Error;
        case TapLink::State::Idle:
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
}

static void updateStatusDisplay() {
    if (!gTapLink) return;
    
    // Update ready pattern based on state
    gStatusDisplay.setReadyPattern(readyPatternForState(gTapLink->getState()));
    
    // Update role pattern
    StatusDisplay::RolePattern role = StatusDisplay::RolePattern::Unknown;
    if (gTapLink->hasRole()) {
        role = gTapLink->isMaster() 
            ? StatusDisplay::RolePattern::Master
            : StatusDisplay::RolePattern::Slave;
    }
    gStatusDisplay.setRolePattern(role);
}

void setup() {
    platform_timing_init();
    
    // Initialize status display
    static const uint32_t LED_PINS[] = { STATUS_LED0_PIN, STATUS_LED1_PIN };
    gStatusDisplay.begin(LED_PINS, 2);
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
    
    // ... other initialization ...
}

void loop() {
    // Update status display
    updateStatusDisplay();
    
    // Poll tap link
    if (gTapLink) {
        gTapLink->poll();
        
        if (gTapLink->isConnectionComplete()) {
            // Show success briefly
            gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);
            gTapLink->reset();
        }
    }
    
    // Update LED patterns (must be called)
    gStatusDisplay.loop();
    
    platform_delay_ms(5);
}
```

## Configuration

### Pin Configuration

Default pins are defined in `status_display.h`:

```cpp
#ifndef STATUS_LED0_PIN
#define STATUS_LED0_PIN 13  // Common built-in LED
#endif

#ifndef STATUS_LED1_PIN
#define STATUS_LED1_PIN 12  // Second LED placeholder
#endif
```

Override via build flags in `platformio.ini`:

```ini
build_flags =
    -DSTATUS_LED0_PIN=13
    -DSTATUS_LED1_PIN=12
```

For STM32CubeIDE, define in project settings or `main.h`:

```c
#define STATUS_LED0_PIN GPIO_PIN_5   // Example: PA5
#define STATUS_LED1_PIN GPIO_PIN_6   // Example: PA6
```

### Maximum LEDs

The maximum number of LEDs is configurable via `MAX_LEDS` (default: 4):

```cpp
static constexpr size_t MAX_LEDS = 4;
```

To support more LEDs, modify this constant in `status_display.h`.

## Extending the Status Display

### Adding a New Pattern

1. **Define the blink steps** in `status_display.cpp`:

```cpp
static constexpr StatusDisplay::BlinkStep PATTERN_NEW_STEPS[] = {
    {100, true},   // 100ms ON
    {200, false},  // 200ms OFF
    {100, true},   // 100ms ON
    {600, false}   // 600ms OFF
};
```

2. **Add pattern to enum** in `status_display.h`:

```cpp
enum class ReadyPattern {
    // ... existing patterns ...
    NewPattern
};
```

3. **Map pattern in `patternFor()`** in `status_display.cpp`:

```cpp
const StatusDisplay::LedPattern& StatusDisplay::patternFor(ReadyPattern pattern) {
    static const LedPattern NEW = {
        PATTERN_NEW_STEPS, 
        sizeof(PATTERN_NEW_STEPS) / sizeof(BlinkStep), 
        false, 
        false
    };
    
    switch (pattern) {
        // ... existing cases ...
        case ReadyPattern::NewPattern:
            return NEW;
        // ...
    }
}
```

### Adding a New LED

1. **Increase `MAX_LEDS`** if needed (default is 4)
2. **Add LED pin** to initialization:

```cpp
static const uint32_t LED_PINS[] = { 
    STATUS_LED0_PIN, 
    STATUS_LED1_PIN,
    STATUS_LED2_PIN  // New LED
};
gStatusDisplay.begin(LED_PINS, 3);  // Now 3 LEDs
```

3. **Add setter method** in `status_display.h`:

```cpp
void setCustomPattern(CustomPattern pattern);
```

4. **Implement setter** in `status_display.cpp`:

```cpp
void StatusDisplay::setCustomPattern(CustomPattern pattern) {
    if (!_initialized || _ledCount < 3) {
        return;
    }
    applyPattern(2, patternFor(pattern));  // LED index 2
}
```

## Platform Migration

### Arduino Implementation

The status display uses platform abstractions, so no changes are needed. It automatically uses:
- `platform_gpio_*()` from `platform_gpio_arduino.cpp`
- `platform_millis()` from `platform_timing_arduino.cpp`

### STM32 HAL Implementation

When migrating to STM32CubeIDE:

1. **No changes needed** to `status_display.h` or `status_display.cpp`
2. **Ensure HAL implementations exist**:
   - `platform_gpio_hal.cpp` - GPIO operations
   - `platform_timing_hal.cpp` - Timing functions
3. **Configure pins** in CubeMX or `main.h`:
   ```c
   #define STATUS_LED0_PIN GPIO_PIN_5
   #define STATUS_LED1_PIN GPIO_PIN_6
   ```
4. **Pin mapping**: The HAL GPIO implementation must map platform pin IDs to HAL GPIO handles

The status display will work automatically once the platform abstractions are implemented.

## Performance Considerations

- **CPU Usage**: Minimal - pattern updates happen only when state changes or timers expire
- **Memory**: ~100 bytes per LED (pattern pointers, state tracking)
- **Timing Accuracy**: Depends on `platform_millis()` accuracy (typically ±1ms)
- **Call Frequency**: `loop()` should be called regularly (every 5-10ms recommended)

## Troubleshooting

### LEDs Not Blinking

1. **Check initialization**: Ensure `begin()` was called
2. **Check loop()**: Must call `gStatusDisplay.loop()` in main loop
3. **Check pins**: Verify pin numbers are correct for your board
4. **Check GPIO**: Verify platform GPIO abstraction is working

### Pattern Not Changing

1. **Check setter calls**: Ensure `setReadyPattern()` or `setRolePattern()` is being called
2. **Check state**: Verify the pattern enum value is correct
3. **Check timing**: Patterns may take time to become visible (wait for current step to complete)

### Wrong LED Behavior

1. **Check pin mapping**: Verify LED pins are correct
2. **Check polarity**: Some LEDs may be active-low (invert logic if needed)
3. **Check GPIO mode**: Ensure pins are configured as outputs

## File Structure

```
firmware/
├── include/
│   └── status_display.h      # Status display interface
├── src/
│   └── status_display.cpp   # Status display implementation
└── STATUS_DISPLAY_README.md # This documentation
```

## See Also

- `PLATFORM_ABSTRACTION_README.md` - Platform abstraction overview
- `MIGRATION_STATUS.md` - Migration status and checklist
- `include/platform_gpio.h` - GPIO abstraction interface
- `include/platform_timing.h` - Timing abstraction interface

---

**Last Updated:** December 2025
