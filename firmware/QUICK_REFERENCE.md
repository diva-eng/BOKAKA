# Quick Reference: Platform Abstraction API

## Timing Functions

```cpp
#include "platform_timing.h"

// Initialize (call once at startup)
platform_timing_init();

// Get current time
uint32_t ms = platform_millis();      // Milliseconds since startup
uint32_t us = platform_micros();       // Microseconds since startup

// Delays
platform_delay_ms(100);                // Delay 100 milliseconds
platform_delay_us(50);                 // Delay 50 microseconds
```

## Serial/USB Communication

```cpp
#include "platform_serial.h"

// Initialize (call once at startup)
platform_serial_begin(115200);         // Baud rate (ignored for USB CDC)

// Check for available data
int available = platform_serial_available();  // Returns byte count

// Read data
int byte = platform_serial_read();    // Returns -1 if no data

// Print strings
platform_serial_print("Hello");
platform_serial_println("World");      // Adds \r\n

// Print numbers
platform_serial_print(42);             // Integer
platform_serial_print((uint32_t)100);  // Unsigned integer
platform_serial_print_hex(0xAB);      // Prints "AB"

// Flush output
platform_serial_flush();               // Wait for transmission
```

## Storage/EEPROM

```cpp
#include "platform_storage.h"

// Initialize (call once at startup)
platform_storage_begin(2048);          // Size in bytes

// Read/Write
uint8_t value = platform_storage_read(0);
platform_storage_write(0, 0x42);

// Commit writes (some platforms auto-commit)
platform_storage_commit();
```

## GPIO Operations

```cpp
#include "platform_gpio.h"

// Configure pin mode
platform_gpio_pin_mode(PA0, PLATFORM_GPIO_MODE_INPUT_PULLUP);
platform_gpio_pin_mode(PA1, PLATFORM_GPIO_MODE_OUTPUT);
platform_gpio_pin_mode(PA2, PLATFORM_GPIO_MODE_OUTPUT_OD);  // Open-drain

// Read pin state
bool state = platform_gpio_read(PA0);  // Returns true if HIGH

// Write pin state
platform_gpio_write(PA1, PLATFORM_GPIO_HIGH);
platform_gpio_write(PA1, PLATFORM_GPIO_LOW);
```

## Migration Checklist

When creating HAL implementations:

### platform_timing_hal.cpp
- [ ] `platform_timing_init()` - Enable SysTick, DWT cycle counter
- [ ] `platform_millis()` - Use `HAL_GetTick()`
- [ ] `platform_micros()` - Use DWT cycle counter
- [ ] `platform_delay_ms()` - Use `HAL_Delay()`
- [ ] `platform_delay_us()` - Use DWT or timer

### platform_serial_hal.cpp
- [ ] `platform_serial_begin()` - Initialize USB CDC, wait for connection
- [ ] `platform_serial_available()` - Use `CDC_GetRxBufferLen()`
- [ ] `platform_serial_read()` - Use `CDC_Receive()`
- [ ] `platform_serial_print()` - Use `CDC_Transmit()`
- [ ] `platform_serial_println()` - Use `CDC_Transmit()` + `\r\n`
- [ ] `platform_serial_flush()` - Wait for transmission complete
- [ ] Number printing - Use `sprintf()` or similar

### platform_storage_hal.cpp
- [ ] `platform_storage_begin()` - Initialize EEPROM emulation
- [ ] `platform_storage_read()` - Use `EE_ReadVariable()`
- [ ] `platform_storage_write()` - Use `EE_WriteVariable()`
- [ ] `platform_storage_commit()` - May be automatic, or call commit

### platform_gpio_hal.cpp
- [ ] `platform_gpio_pin_mode()` - Use `HAL_GPIO_Init()`
- [ ] `platform_gpio_read()` - Use `HAL_GPIO_ReadPin()`
- [ ] `platform_gpio_write()` - Use `HAL_GPIO_WritePin()`

### tap_link_hal_hal.cpp
- [ ] Create HAL version of tap link HAL
- [ ] Use `platform_gpio_*()` functions (already abstracted)
- [ ] Use `platform_timing_*()` functions (already abstracted)

## File Mapping

| Current (Arduino) | Future (HAL) |
|-------------------|--------------|
| `platform_timing_arduino.cpp` | `platform_timing_hal.cpp` |
| `platform_serial_arduino.cpp` | `platform_serial_hal.cpp` |
| `platform_storage_arduino.cpp` | `platform_storage_hal.cpp` |
| `platform_gpio_arduino.cpp` | `platform_gpio_hal.cpp` |
| `tap_link_hal_arduino.cpp` | `tap_link_hal_hal.cpp` |

## Common Patterns

### Replacing Serial.print() with number
```cpp
// Old (Arduino)
Serial.print(42);

// New (Abstraction)
platform_serial_print(42);  // Works for int
platform_serial_print((uint32_t)42);  // For unsigned
```

### Replacing Serial.print() with hex
```cpp
// Old (Arduino)
Serial.print(byte, HEX);

// New (Abstraction)
platform_serial_print_hex(byte);
```

### Replacing millis() timing
```cpp
// Old (Arduino)
uint32_t start = millis();
if (millis() - start > 1000) { ... }

// New (Abstraction)
uint32_t start = platform_millis();
if (platform_millis() - start > 1000) { ... }
```

### Replacing EEPROM operations
```cpp
// Old (Arduino)
EEPROM.begin(2048);
uint8_t val = EEPROM.read(0);
EEPROM.write(0, 0x42);
EEPROM.commit();

// New (Abstraction)
platform_storage_begin(2048);
uint8_t val = platform_storage_read(0);
platform_storage_write(0, 0x42);
platform_storage_commit();
```
