# Platform Hardware Abstraction Layer Design

## Overview

The platform HAL provides hardware-independent interfaces for GPIO, timing, serial, storage, and device identification. This enables the same application code to run on Arduino/PlatformIO and STM32CubeIDE with minimal changes.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Application                             │
├─────────────────────────────────────────────────────────────┤
│                    Platform Interfaces                       │
│  ┌─────────┐ ┌─────────┐ ┌──────────┐ ┌─────────┐ ┌───────┐ │
│  │  GPIO   │ │ Timing  │ │ Serial   │ │ Storage │ │Device │ │
│  └────┬────┘ └────┬────┘ └────┬─────┘ └────┬────┘ └───┬───┘ │
├───────┼──────────┼──────────┼───────────┼──────────┼───────┤
│       │          │          │           │          │        │
│  ┌────┴────┐┌────┴────┐┌────┴─────┐┌────┴────┐┌────┴────┐  │
│  │ Arduino ││ Arduino ││ Arduino  ││ Arduino ││ Arduino │  │
│  │  GPIO   ││ millis  ││ Serial   ││ EEPROM  ││STM32 HAL│  │
│  └─────────┘└─────────┘└──────────┘└─────────┘└─────────┘  │
│                                                              │
│  ┌─────────┐┌─────────┐┌──────────┐┌─────────┐┌─────────┐  │
│  │STM32 HAL││STM32 HAL││ STM32 USB││STM32 HAL││STM32 HAL│  │
│  │  GPIO   ││  TIM    ││   CDC    ││ FLASH   ││  UID    │  │
│  └─────────┘└─────────┘└──────────┘└─────────┘└─────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## GPIO Interface

**Header:** `include/platform_gpio.h`  
**Arduino impl:** `src/platform_gpio_arduino.cpp`

### Pin Modes

```cpp
typedef enum {
    PLATFORM_GPIO_MODE_INPUT,        // High-impedance input
    PLATFORM_GPIO_MODE_INPUT_PULLUP, // Input with internal pull-up
    PLATFORM_GPIO_MODE_OUTPUT,       // Push-pull output
    PLATFORM_GPIO_MODE_OUTPUT_OD     // Open-drain output
} platform_gpio_mode_t;
```

### Functions

```cpp
void platform_gpio_pin_mode(uint32_t pin, platform_gpio_mode_t mode);
bool platform_gpio_read(uint32_t pin);
void platform_gpio_write(uint32_t pin, platform_gpio_state_t state);
```

### Arduino Implementation Notes

- `PLATFORM_GPIO_MODE_OUTPUT_OD` maps to standard OUTPUT mode
- True open-drain behavior depends on hardware capability
- Pin identifiers use Arduino constants (PA5, PA6, etc.)

### STM32 HAL Migration

```cpp
// Arduino
pinMode(pin, INPUT_PULLUP);
digitalRead(pin);
digitalWrite(pin, HIGH);

// STM32 HAL
GPIO_InitTypeDef init = {.Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLUP};
HAL_GPIO_Init(GPIOx, &init);
HAL_GPIO_ReadPin(GPIOx, pin);
HAL_GPIO_WritePin(GPIOx, pin, GPIO_PIN_SET);
```

## Timing Interface

**Header:** `include/platform_timing.h`  
**Arduino impl:** `src/platform_timing_arduino.cpp`

### Functions

```cpp
void platform_timing_init();           // Initialize timing system
uint32_t platform_millis();            // Milliseconds since boot (wraps ~49 days)
uint32_t platform_micros();            // Microseconds since boot (wraps ~71 min)
void platform_delay_ms(uint32_t ms);   // Blocking millisecond delay
void platform_delay_us(uint32_t us);   // Blocking microsecond delay
```

### Arduino Implementation

Direct wrapper around Arduino functions:
- `millis()`, `micros()`, `delay()`, `delayMicroseconds()`
- `platform_timing_init()` is a no-op (Arduino auto-initializes)

### STM32 HAL Migration

```cpp
// Uses SysTick or dedicated timer
HAL_GetTick();           // 1ms resolution
TIM->CNT;                // Microsecond counter from hardware timer
HAL_Delay(ms);           // Blocking delay
// Custom DWT cycle counter for microsecond delays
```

### Overflow Handling

The application handles timer overflow correctly:

```cpp
uint32_t elapsedMicros(uint32_t startTime) {
    uint32_t now = platform_micros();
    if (now >= startTime) {
        return now - startTime;
    } else {
        return (UINT32_MAX - startTime) + now + 1;
    }
}
```

## Serial Interface

**Header:** `include/platform_serial.h`  
**Arduino impl:** `src/platform_serial_arduino.cpp`

### Functions

```cpp
void platform_serial_begin(uint32_t baud);   // Initialize (baud ignored for USB)
int platform_serial_available();             // Bytes available to read
int platform_serial_read();                  // Read byte (-1 if none)
void platform_serial_print(const char* str); // Print string
void platform_serial_print(int num);         // Print integer
void platform_serial_print(uint32_t num);    // Print unsigned
void platform_serial_print_hex(uint8_t byte);// Print hex byte
void platform_serial_println(const char* str);
void platform_serial_flush();                // Wait for TX complete
```

### Arduino Implementation

Direct wrapper around `Serial` object (USB CDC on STM32).

### STM32 HAL Migration

```cpp
// USB CDC requires CDC_Transmit_FS() and CDC_Receive_FS()
// Or UART with HAL_UART_Transmit/Receive
CDC_Transmit_FS((uint8_t*)str, strlen(str));
```

## Storage Interface

**Header:** `include/platform_storage.h`  
**Arduino impl:** `src/platform_storage_arduino.cpp`

### Functions

```cpp
bool platform_storage_begin(size_t size);      // Initialize storage
uint8_t platform_storage_read(size_t address); // Read byte
void platform_storage_write(size_t address, uint8_t value);
bool platform_storage_commit();                // Flush writes
```

### Arduino Implementation Notes

- Uses STM32 Arduino EEPROM library
- `EEPROM.begin()` doesn't take size parameter on STM32
- Size is tracked internally for bounds checking
- `commit()` may be no-op depending on core implementation

### STM32 HAL Migration

EEPROM emulation in flash requires:
- Page erase before write
- Virtual address mapping
- Wear leveling (optional)

```cpp
// STM32 HAL flash operations
HAL_FLASHEx_DATAEEPROM_Unlock();
HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, addr, data);
HAL_FLASHEx_DATAEEPROM_Lock();
```

## Device Identity Interface

**Header:** `include/platform_device.h`  
**Arduino impl:** `src/platform_device_arduino.cpp`

### Functions

```cpp
void platform_get_device_uid(uint8_t out[PLATFORM_DEVICE_UID_SIZE]);
```

Returns 12-byte (96-bit) unique device ID.

### STM32 Implementation

STM32L0 UID is stored at fixed memory addresses, accessible via HAL:

```cpp
uint32_t w0 = HAL_GetUIDw0();
uint32_t w1 = HAL_GetUIDw1();
uint32_t w2 = HAL_GetUIDw2();
```

Bytes are extracted in big-endian order for consistent ID comparison across devices.

### Memory Layout

```
UID Word 0 (address 0x1FF80050): out[0..3]
UID Word 1 (address 0x1FF80054): out[4..7]
UID Word 2 (address 0x1FF80064): out[8..11]
```

## TapLink HAL

**Header:** `include/tap_link_hal.h`  
**Arduino impl:** `src/tap_link_hal_arduino.cpp`

### Interface

```cpp
struct IOneWireHal {
    virtual bool readLine() = 0;              // Read physical line state
    virtual void driveLow(bool enableLow) = 0;// Control line driver
    virtual uint32_t micros() = 0;            // Get microseconds
    virtual void delayMicros(uint32_t us) = 0;// Blocking delay
};
```

### Arduino Implementation

```cpp
class OneWireHalArduino : public IOneWireHal {
    uint32_t _pin;
public:
    bool readLine() override {
        return platform_gpio_read(_pin);
    }
    
    void driveLow(bool enableLow) override {
        if (enableLow) {
            platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_OUTPUT);
            platform_gpio_write(_pin, PLATFORM_GPIO_LOW);
        } else {
            platform_gpio_pin_mode(_pin, PLATFORM_GPIO_MODE_INPUT_PULLUP);
        }
    }
    
    uint32_t micros() override { return platform_micros(); }
    void delayMicros(uint32_t us) override { platform_delay_us(us); }
};
```

## Board Configuration

**Header:** `include/board_config.h`

Pin definitions with defaults, overridable via build flags:

```cpp
#ifndef STATUS_LED0_PIN
#define STATUS_LED0_PIN PA5   // D13 on Nucleo
#endif

#ifndef STATUS_LED1_PIN
#define STATUS_LED1_PIN PA6   // D12 on Nucleo
#endif

#ifndef TAP_LINK_PIN
#define TAP_LINK_PIN PA9      // D8 on Nucleo
#endif
```

Override in `platformio.ini`:
```ini
build_flags = -DSTATUS_LED0_PIN=PB0
```

## Migration Checklist

To port to STM32CubeIDE:

1. **GPIO:** Replace pinMode/digitalRead/Write with HAL_GPIO_*
2. **Timing:** Configure SysTick, add microsecond timer
3. **Serial:** Implement USB CDC or UART wrapper
4. **Storage:** Implement flash EEPROM emulation
5. **Device:** Direct memory access for UID (already uses HAL)
6. **Sleep:** Add HAL_PWR_EnterSLEEPMode with GPIO wake-up

See `PLATFORM_ABSTRACTION.md` and `MIGRATION_PLAN.md` for detailed migration steps.

