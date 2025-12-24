# Platform Abstraction Layer

This document describes the platform abstraction layer created to facilitate migration from PlatformIO/Arduino to STM32CubeIDE/HAL.

## Overview

The abstraction layer provides platform-independent interfaces for:
- **Timing functions** (millis, micros, delays)
- **Serial/USB communication** (print, read, available)
- **Storage/EEPROM** (read, write, commit)
- **GPIO operations** (pin mode, read, write)
- **Status display** (LED patterns using GPIO and timing abstractions)

## Architecture

### Current Implementation (Arduino)
- `src/platform_timing_arduino.cpp` - Arduino timing implementation
- `src/platform_serial_arduino.cpp` - Arduino Serial implementation
- `src/platform_storage_arduino.cpp` - Arduino EEPROM implementation
- `src/platform_gpio_arduino.cpp` - Arduino GPIO implementation
- `src/tap_link_hal_arduino.cpp` - Arduino tap link HAL implementation
- `src/status_display.cpp` - Status display (uses platform abstractions)

### Future Implementation (STM32 HAL)
When migrating to STM32CubeIDE, create:
- `src/platform_timing_hal.cpp` - HAL timing implementation
- `src/platform_serial_hal.cpp` - HAL USB CDC implementation
- `src/platform_storage_hal.cpp` - HAL EEPROM emulation implementation
- `src/platform_gpio_hal.cpp` - HAL GPIO implementation
- `src/tap_link_hal_hal.cpp` - HAL tap link HAL implementation
- `src/status_display.cpp` - No changes needed (uses platform abstractions)

## Usage

### Timing Functions

```cpp
#include "platform_timing.h"

void setup() {
    platform_timing_init();  // Call once at startup
}

void loop() {
    uint32_t ms = platform_millis();
    uint32_t us = platform_micros();
    platform_delay_ms(100);
    platform_delay_us(50);
}
```

### Serial/USB Communication

```cpp
#include "platform_serial.h"

void setup() {
    platform_serial_begin(115200);
}

void loop() {
    if (platform_serial_available() > 0) {
        int c = platform_serial_read();
        // Process character
    }
    
    platform_serial_print("Hello");
    platform_serial_print(42);
    platform_serial_println("World");
    platform_serial_flush();
}
```

### Storage/EEPROM

```cpp
#include "platform_storage.h"

void setup() {
    platform_storage_begin(2048);  // Initialize 2KB storage
}

void loop() {
    uint8_t value = platform_storage_read(0);
    platform_storage_write(0, 0x42);
    platform_storage_commit();  // Persist writes
}
```

### GPIO Operations

```cpp
#include "platform_gpio.h"

void setup() {
    // Configure pin as input with pull-up
    platform_gpio_pin_mode(PA0, PLATFORM_GPIO_MODE_INPUT_PULLUP);
    
    // Configure pin as output
    platform_gpio_pin_mode(PA1, PLATFORM_GPIO_MODE_OUTPUT);
}

void loop() {
    // Read pin state
    bool state = platform_gpio_read(PA0);
    
    // Write pin state
    platform_gpio_write(PA1, PLATFORM_GPIO_HIGH);
    platform_gpio_write(PA1, PLATFORM_GPIO_LOW);
}
```

### Status Display

```cpp
#include "status_display.h"

StatusDisplay gStatusDisplay;

void setup() {
    // Initialize status display
    static const uint32_t LED_PINS[] = { STATUS_LED0_PIN, STATUS_LED1_PIN };
    gStatusDisplay.begin(LED_PINS, 2);
    
    // Set patterns
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
}

void loop() {
    // Update patterns based on application state
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Idle);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);
    
    // Must call loop() to update LED patterns
    gStatusDisplay.loop();
}
```

**Note:** Status display uses `platform_gpio_*()` and `platform_timing_*()` abstractions, so it works automatically once those are implemented. See `STATUS_DISPLAY_README.md` for detailed documentation.

## Migration Guide

### Step 1: Create HAL Implementation Files

1. Copy the Arduino implementation files as templates
2. Replace Arduino API calls with HAL equivalents
3. Update build system to include HAL implementations instead of Arduino ones

### Step 2: Update Build Configuration

**PlatformIO (current):**
- Automatically includes `*_arduino.cpp` files
- No special configuration needed

**STM32CubeIDE (future):**
- Add `platform_timing_hal.cpp` to project
- Add `platform_serial_hal.cpp` to project
- Add `platform_storage_hal.cpp` to project
- Remove or exclude `*_arduino.cpp` files

### Step 3: Verify Functionality

After migration, test:
- [ ] Timing functions return correct values
- [ ] Serial communication works (USB CDC)
- [ ] Storage read/write operations succeed
- [ ] All application code compiles without errors

## File Structure

```
firmware/
├── include/
│   ├── platform_timing.h      # Timing abstraction interface
│   ├── platform_serial.h      # Serial/USB abstraction interface
│   ├── platform_storage.h     # Storage abstraction interface
│   ├── platform_gpio.h        # GPIO abstraction interface
│   └── status_display.h        # Status display interface
├── src/
│   ├── platform_timing_arduino.cpp    # Arduino implementation
│   ├── platform_serial_arduino.cpp     # Arduino implementation
│   ├── platform_storage_arduino.cpp   # Arduino implementation
│   ├── platform_gpio_arduino.cpp      # Arduino implementation
│   ├── tap_link_hal_arduino.cpp        # Arduino tap link HAL
│   ├── status_display.cpp              # Status display (uses abstractions)
│   ├── platform_timing_hal.cpp         # HAL implementation (to be created)
│   ├── platform_serial_hal.cpp        # HAL implementation (to be created)
│   ├── platform_storage_hal.cpp       # HAL implementation (to be created)
│   ├── platform_gpio_hal.cpp          # HAL implementation (to be created)
│   └── tap_link_hal_hal.cpp           # HAL tap link HAL (to be created)
└── ...
```

## Benefits

1. **Easy Migration**: Swap implementation files instead of modifying application code
2. **Code Reuse**: Application logic remains unchanged
3. **Testing**: Can test Arduino version while developing HAL version
4. **Maintainability**: Clear separation between platform and application code

## Notes

- All abstraction functions are C-linkage compatible (can be called from C or C++)
- The abstraction layer adds minimal overhead (function call indirection)
- Platform-specific includes (Arduino.h, HAL headers) are isolated to implementation files
- Application code no longer includes platform-specific headers directly
