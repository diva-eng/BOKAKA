# Migration Plan: PlatformIO → STM32CubeIDE

## Overview

This document outlines the migration plan for moving the Bokaka firmware from PlatformIO/Arduino to STM32CubeIDE/HAL.

**Target:** STM32L053R8  
**Status:** ✅ Platform abstraction complete - ready for HAL implementation

---

## Current Architecture

All application code is **platform-independent**. Only the platform implementation files need to be replaced:

| Layer | Status | Notes |
|-------|--------|-------|
| Application (`main.cpp`, `storage.cpp`, `usb_serial.cpp`, `tap_link.cpp`) | ✅ Abstracted | No changes needed |
| Platform interfaces (`platform_*.h`) | ✅ Complete | Define the abstraction API |
| Arduino implementations (`*_arduino.cpp`) | ✅ Working | Current PlatformIO build |
| HAL implementations (`*_hal.cpp`) | 🔄 To create | For STM32CubeIDE migration |

---

## Migration Steps

### Phase 1: STM32CubeIDE Project Setup

1. **Create new STM32 project** (STM32L053R8)
2. **Configure with CubeMX:**
   - System clock: HSI16 (16 MHz)
   - USB Device: CDC (Virtual COM Port)
   - GPIO: PA9 for tap link, PA5/PA6 for status LEDs
3. **Generate code**

### Phase 2: Copy Application Code

Copy to `Core/Inc/` and `Core/Src/`:

```
Headers (include/ → Core/Inc/):
  storage.h, usb_serial.h, tap_link.h, tap_link_hal.h
  device_id.h, status_display.h, fw_config.h
  platform_timing.h, platform_serial.h
  platform_storage.h, platform_gpio.h

Sources (src/ → Core/Src/):
  storage.cpp, usb_serial.cpp, tap_link.cpp
  device_id.cpp, status_display.cpp
```

### Phase 3: Create HAL Implementations

Create these files (see [PLATFORM_ABSTRACTION.md](PLATFORM_ABSTRACTION.md) for implementation details):

| File | Purpose |
|------|---------|
| `platform_timing_hal.cpp` | HAL timing (SysTick, DWT) |
| `platform_serial_hal.cpp` | USB CDC communication |
| `platform_storage_hal.cpp` | EEPROM emulation |
| `platform_gpio_hal.cpp` | GPIO operations |
| `tap_link_hal_hal.cpp` | Tap link HAL (copy from Arduino, update pins) |

### Phase 4: Convert Main Entry Point

Replace Arduino `setup()`/`loop()` with standard `main()`:

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

### Phase 5: Integrate mbedTLS

1. Download mbedTLS 2.23.0+
2. Copy to `Middlewares/mbedTLS/`
3. Copy `lib/crypto_config/mbedtls_config.h`
4. Configure build:
   - Add include paths
   - Add define: `MBEDTLS_CONFIG_FILE="mbedtls_config.h"`

### Phase 6: Build & Test

1. Enable C++ support in project settings
2. Fix compilation errors
3. Test all functionality

---

## HAL Implementation Notes

### Timing

```cpp
void platform_timing_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t platform_millis() { return HAL_GetTick(); }
uint32_t platform_micros() { return DWT->CYCCNT / (SystemCoreClock / 1000000); }
void platform_delay_ms(uint32_t ms) { HAL_Delay(ms); }
```

### USB Serial

Use CubeMX-generated `usbd_cdc_if.c` functions:
- `CDC_Transmit()` for output
- `CDC_Receive()` for input

### Storage

Use STM32 EEPROM Emulation library from STM32CubeL0:
- `EE_Init()` for initialization
- `EE_ReadVariable()` / `EE_WriteVariable()` for read/write
- Handle 16-bit word alignment

### GPIO

```cpp
void platform_gpio_pin_mode(uint32_t pin, platform_gpio_mode_t mode) {
    GPIO_InitTypeDef init = {0};
    init.Pin = get_gpio_pin(pin);
    init.Mode = (mode == PLATFORM_GPIO_MODE_OUTPUT) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    init.Pull = (mode == PLATFORM_GPIO_MODE_INPUT_PULLUP) ? GPIO_PULLUP : GPIO_NOPULL;
    HAL_GPIO_Init(get_gpio_port(pin), &init);
}
```

---

## Testing Checklist

### Hardware Tests

- [ ] GPIO (tap link) - read/write, open-drain
- [ ] GPIO (status LEDs) - LED patterns
- [ ] USB CDC - connect, enumerate, communicate
- [ ] Flash storage - read/write, power cycle
- [ ] Device UID - read correctly
- [ ] Timing - millis, micros, delays accurate

### Functional Tests

- [ ] Storage: begin(), saveNow(), clearAll(), addLink()
- [ ] USB commands: hello, get_state, clear, dump
- [ ] USB commands: provision_key, sign_state
- [ ] Tap link: connection protocol, state machine
- [ ] Status display: LED patterns for each state
- [ ] Crypto: HMAC-SHA256 signing

### Comparison Tests

- [ ] Command responses match PlatformIO version
- [ ] Timing behavior matches
- [ ] Performance similar or better

---

## Timeline Estimate

| Phase | Duration |
|-------|----------|
| Project Setup | 2-4 hours |
| Core Migration | 8-12 hours |
| Storage Migration | 4-6 hours |
| mbedTLS Integration | 2-4 hours |
| Build System | 2-3 hours |
| Testing & Debugging | 8-16 hours |
| **Total** | **26-45 hours** |

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| USB CDC buffer issues | Implement ring buffers, test thoroughly |
| Flash wear | Use ST EEPROM emulation, monitor cycles |
| Timing accuracy | Use DWT counter, verify with scope |
| GPIO behavior | Test open-drain, verify pull-up/down |
| mbedTLS config | Start minimal, test incrementally |

---

## Resources

- [STM32L0 HAL Documentation](https://www.st.com/resource/en/user_manual/um1884-description-of-stm32l0-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
- [STM32CubeL0 Examples](https://www.st.com/en/embedded-software/stm32cubel0.html)
- [mbedTLS Documentation](https://mbed-tls.readthedocs.io/)

---

*Last Updated: January 2026*
