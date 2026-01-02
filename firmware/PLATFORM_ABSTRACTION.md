# Platform Abstraction Layer

This document describes the platform abstraction layer that enables easy migration between Arduino/PlatformIO and STM32CubeIDE/HAL.

---

## Overview

The abstraction layer provides platform-independent interfaces for:

| Module | Header | Description |
|--------|--------|-------------|
| **Timing** | `platform_timing.h` | millis, micros, delays |
| **Serial** | `platform_serial.h` | USB CDC communication |
| **Storage** | `platform_storage.h` | EEPROM read/write |
| **GPIO** | `platform_gpio.h` | Pin mode, read, write |

---

## Current Implementation

### Arduino (Working)

| Header | Implementation |
|--------|----------------|
| `platform_timing.h` | `platform_timing_arduino.cpp` |
| `platform_serial.h` | `platform_serial_arduino.cpp` |
| `platform_storage.h` | `platform_storage_arduino.cpp` |
| `platform_gpio.h` | `platform_gpio_arduino.cpp` |

### STM32 HAL (For Migration)

Create these files when migrating to STM32CubeIDE:

| Header | Implementation |
|--------|----------------|
| `platform_timing.h` | `platform_timing_hal.cpp` |
| `platform_serial.h` | `platform_serial_hal.cpp` |
| `platform_storage.h` | `platform_storage_hal.cpp` |
| `platform_gpio.h` | `platform_gpio_hal.cpp` |

---

## Quick Reference API

### Timing Functions

```cpp
#include "platform_timing.h"

platform_timing_init();              // Call once at startup

uint32_t ms = platform_millis();     // Milliseconds since startup
uint32_t us = platform_micros();     // Microseconds since startup

platform_delay_ms(100);              // Delay 100 milliseconds
platform_delay_us(50);               // Delay 50 microseconds
```

### Serial/USB Communication

```cpp
#include "platform_serial.h"

platform_serial_begin(115200);       // Initialize (baud ignored for USB CDC)

int avail = platform_serial_available();  // Check available bytes
int byte = platform_serial_read();        // Read byte (-1 if none)

platform_serial_print("Hello");      // Print string
platform_serial_println("World");    // Print with newline
platform_serial_print(42);           // Print integer
platform_serial_print((uint32_t)n);  // Print unsigned
platform_serial_print_hex(0xAB);     // Print "AB"
platform_serial_flush();             // Wait for transmission
```

### Storage/EEPROM

```cpp
#include "platform_storage.h"

platform_storage_begin(2048);        // Initialize storage (size in bytes)

uint8_t val = platform_storage_read(addr);    // Read byte
platform_storage_write(addr, 0x42);           // Write byte
platform_storage_commit();                    // Persist writes
```

### GPIO Operations

```cpp
#include "platform_gpio.h"

// Configure pin mode
platform_gpio_pin_mode(pin, PLATFORM_GPIO_MODE_INPUT);
platform_gpio_pin_mode(pin, PLATFORM_GPIO_MODE_INPUT_PULLUP);
platform_gpio_pin_mode(pin, PLATFORM_GPIO_MODE_OUTPUT);
platform_gpio_pin_mode(pin, PLATFORM_GPIO_MODE_OUTPUT_OD);  // Open-drain

// Read/Write
bool state = platform_gpio_read(pin);  // Returns true if HIGH
platform_gpio_write(pin, PLATFORM_GPIO_HIGH);
platform_gpio_write(pin, PLATFORM_GPIO_LOW);
```

---

## Migration Guide

### Step 1: Create HAL Implementations

Copy Arduino implementations as templates and replace with HAL equivalents:

#### Timing (`platform_timing_hal.cpp`)

```cpp
#include "platform_timing.h"
#include "stm32l0xx_hal.h"

void platform_timing_init() {
    // HAL_Init() already called by main()
    // Enable DWT for microsecond timing
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t platform_millis() {
    return HAL_GetTick();
}

uint32_t platform_micros() {
    return DWT->CYCCNT / (SystemCoreClock / 1000000);
}

void platform_delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

void platform_delay_us(uint32_t us) {
    uint32_t start = platform_micros();
    while (platform_micros() - start < us);
}
```

#### Serial (`platform_serial_hal.cpp`)

```cpp
#include "platform_serial.h"
#include "usbd_cdc_if.h"
#include <stdio.h>

void platform_serial_begin(uint32_t baud) {
    // USB CDC doesn't use baud rate
    while (!CDC_IsConnected()) HAL_Delay(10);
}

int platform_serial_available() {
    return CDC_GetRxBufferLen();
}

int platform_serial_read() {
    uint8_t buf;
    return (CDC_Receive(&buf, 1) > 0) ? buf : -1;
}

void platform_serial_print(const char* str) {
    CDC_Transmit((uint8_t*)str, strlen(str));
}
// ... implement remaining functions
```

#### Storage (`platform_storage_hal.cpp`)

Use STM32 EEPROM emulation library from STM32CubeL0.

#### GPIO (`platform_gpio_hal.cpp`)

```cpp
#include "platform_gpio.h"
#include "stm32l0xx_hal.h"

void platform_gpio_pin_mode(uint32_t pin, platform_gpio_mode_t mode) {
    GPIO_InitTypeDef init = {0};
    init.Pin = get_gpio_pin(pin);
    
    switch (mode) {
        case PLATFORM_GPIO_MODE_INPUT:
            init.Mode = GPIO_MODE_INPUT;
            init.Pull = GPIO_NOPULL;
            break;
        case PLATFORM_GPIO_MODE_INPUT_PULLUP:
            init.Mode = GPIO_MODE_INPUT;
            init.Pull = GPIO_PULLUP;
            break;
        case PLATFORM_GPIO_MODE_OUTPUT:
            init.Mode = GPIO_MODE_OUTPUT_PP;
            break;
        case PLATFORM_GPIO_MODE_OUTPUT_OD:
            init.Mode = GPIO_MODE_OUTPUT_OD;
            break;
    }
    HAL_GPIO_Init(get_gpio_port(pin), &init);
}
```

### Step 2: Update Build System

- Add HAL implementation files to project
- Remove/exclude Arduino implementation files
- Configure include paths

### Step 3: Convert Main Entry Point

Replace `setup()`/`loop()` with `main()`:

```cpp
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USB_DEVICE_Init();
    
    platform_timing_init();
    // ... existing setup code (unchanged!) ...
    
    while (1) {
        // ... existing loop code (unchanged!) ...
    }
}
```

---

## Function Replacement Reference

| Arduino | STM32 HAL | Notes |
|---------|-----------|-------|
| `millis()` | `HAL_GetTick()` | Requires SysTick |
| `micros()` | DWT cycle counter | Enable DWT in init |
| `delay(ms)` | `HAL_Delay(ms)` | Blocking |
| `delayMicroseconds(us)` | Custom DWT | Busy wait |
| `pinMode(pin, mode)` | `HAL_GPIO_Init()` | Configure struct |
| `digitalRead(pin)` | `HAL_GPIO_ReadPin()` | Returns SET/RESET |
| `digitalWrite(pin, val)` | `HAL_GPIO_WritePin()` | Use SET/RESET |
| `Serial.begin()` | USB CDC init | Wait for connection |
| `Serial.available()` | `CDC_GetRxBufferLen()` | Check buffer |
| `Serial.read()` | `CDC_Receive()` | From USB buffer |
| `Serial.print()` | `CDC_Transmit()` | Non-blocking |
| `EEPROM.begin()` | `EE_Init()` | EEPROM emulation |
| `EEPROM.read()` | `EE_ReadVariable()` | 16-bit words |
| `EEPROM.write()` | `EE_WriteVariable()` | 16-bit words |

---

## Benefits

1. **Zero Application Code Changes**: All application logic uses abstractions
2. **Easy Migration**: Just swap implementation files
3. **Testability**: Test Arduino version while developing HAL version
4. **Maintainability**: Clear separation between platform and application

---

## File Structure

```
firmware/
├── include/
│   ├── platform_timing.h
│   ├── platform_serial.h
│   ├── platform_storage.h
│   └── platform_gpio.h
└── src/
    ├── platform_timing_arduino.cpp   # Arduino (current)
    ├── platform_serial_arduino.cpp
    ├── platform_storage_arduino.cpp
    ├── platform_gpio_arduino.cpp
    ├── platform_timing_hal.cpp       # HAL (create for migration)
    ├── platform_serial_hal.cpp
    ├── platform_storage_hal.cpp
    └── platform_gpio_hal.cpp
```

---

*Last Updated: January 2026*

