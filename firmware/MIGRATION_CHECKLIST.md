# Migration Checklist: PlatformIO → STM32CubeIDE

Quick reference checklist for migrating Bokaka-Eval firmware.

## Pre-Migration

- [ ] Backup current PlatformIO project
- [ ] Install STM32CubeIDE
- [ ] Install STM32CubeMX (if separate)
- [ ] Verify ST-Link driver is installed
- [ ] Have target hardware ready

## Phase 1: Project Setup

### STM32CubeIDE Project Creation
- [ ] Create new STM32 project
- [ ] Select STM32L053R8
- [ ] Project name: `Bokaka-Eval`

### CubeMX Configuration
- [ ] Configure system clock (HSI16, 16 MHz or target frequency)
- [ ] Enable USB Device (CDC - Virtual COM Port)
  - [ ] Configure USB descriptors
  - [ ] Set USB device name
- [ ] Configure GPIO for tap link (PA9/D8 on CN5 digital header, or target pin)
  - [ ] Set as input with pull-up initially
- [ ] Generate code

### Project Structure
- [ ] Copy header files from `include/` to `Core/Inc/`
- [ ] Copy source files from `src/` to `Core/Src/`
- [ ] Verify all files are added to build

## Phase 2: Framework Replacement

### ✅ Application Code Status
**All application code is already platform-independent!** No changes needed to:
- `storage.cpp` - Uses `platform_storage_*()` abstractions
- `usb_serial.cpp` - Uses `platform_serial_*()` abstractions
- `tap_link_hal_arduino.cpp` - Uses `platform_gpio_*()` and `platform_timing_*()` abstractions
- `status_display.cpp` - Uses `platform_gpio_*()` and `platform_timing_*()` abstractions

### Main Entry Point
- [ ] Convert `main.cpp` to use `main()` function
- [ ] Move `setup()` code to `main()` initialization
- [ ] Move `loop()` code to `while(1)` loop
- [ ] Add `HAL_Init()` and `SystemClock_Config()` calls
- [ ] **Note:** Application code inside setup/loop remains unchanged (uses abstractions)

### Create HAL Implementations

#### Timing Functions
- [ ] Create `Core/Src/platform_timing_hal.cpp`
- [ ] Implement `platform_timing_init()` - Enable SysTick and DWT
- [ ] Implement `platform_millis()` - Use `HAL_GetTick()`
- [ ] Implement `platform_micros()` - Use DWT cycle counter
- [ ] Implement `platform_delay_ms()` - Use `HAL_Delay()`
- [ ] Implement `platform_delay_us()` - Use DWT or busy-wait
- [ ] **No changes needed to application code!**

#### GPIO Functions
- [ ] Create `Core/Src/platform_gpio_hal.cpp`
- [ ] Implement `platform_gpio_pin_mode()` - Use `HAL_GPIO_Init()`
- [ ] Implement `platform_gpio_read()` - Use `HAL_GPIO_ReadPin()`
- [ ] Implement `platform_gpio_write()` - Use `HAL_GPIO_WritePin()`
- [ ] Configure pin mapping (HAL pin → platform pin ID)
- [ ] **No changes needed to application code!**

#### Tap Link HAL
- [ ] Create `Core/Src/tap_link_hal_hal.cpp` (copy from `tap_link_hal_arduino.cpp`)
- [ ] Update pin definitions to use HAL GPIO handles
- [ ] **Logic remains unchanged - already uses abstractions!**

#### Status Display
- [ ] **No HAL implementation needed!** Status display uses platform abstractions
- [ ] Configure LED pins in CubeMX (or `main.h`)
  - [ ] Configure `STATUS_LED0_PIN` (e.g., PA5)
  - [ ] Configure `STATUS_LED1_PIN` (e.g., PA6)
- [ ] Verify pin mapping in `platform_gpio_hal.cpp` supports LED pins
- [ ] Test LED patterns after GPIO HAL is implemented
- [ ] **No changes needed to `status_display.cpp`!** It already uses abstractions.

## Phase 3: USB Serial Migration

### ✅ Application Code Status
**`usb_serial.cpp` already uses abstractions!** No changes needed.

### Create HAL Implementation
- [ ] Create `Core/Src/platform_serial_hal.cpp`
- [ ] Implement `platform_serial_begin()` - Initialize USB CDC, wait for connection
- [ ] Implement `platform_serial_available()` - Use `CDC_GetRxBufferLen()`
- [ ] Implement `platform_serial_read()` - Use `CDC_Receive()`
- [ ] Implement `platform_serial_print()` - Use `CDC_Transmit()`
- [ ] Implement `platform_serial_println()` - Use `CDC_Transmit()` + `\r\n`
- [ ] Implement `platform_serial_print(int)` - Use `sprintf()` + `CDC_Transmit()`
- [ ] Implement `platform_serial_print(uint32_t)` - Use `sprintf()` + `CDC_Transmit()`
- [ ] Implement `platform_serial_print_hex()` - Use `sprintf()` + `CDC_Transmit()`
- [ ] Implement `platform_serial_flush()` - Wait for transmission
- [ ] Handle USB connection state (wait for enumeration)
- [ ] **No changes needed to application code!**

### USB Buffer Management
- [ ] Review `usbd_cdc_if.c` (generated)
- [ ] Modify if needed for better buffer handling
- [ ] Test command reception doesn't block
- [ ] Test response transmission

## Phase 4: Storage Migration

### ✅ Application Code Status
**`storage.cpp` already uses abstractions!** No changes needed.

### EEPROM Emulation Setup
- [ ] Copy EEPROM emulation library from STM32CubeL0
- [ ] Add EEPROM emulation files to project
- [ ] Configure flash pages for EEPROM (avoid bootloader area)
- [ ] Initialize EEPROM emulation in `main()`

### Create HAL Implementation
- [ ] Create `Core/Src/platform_storage_hal.cpp`
- [ ] Implement `platform_storage_begin()` - Initialize EEPROM emulation
- [ ] Implement `platform_storage_read()` - Use `EE_ReadVariable()`, handle byte-level access
- [ ] Implement `platform_storage_write()` - Use `EE_WriteVariable()`, handle byte-level access
- [ ] Implement `platform_storage_commit()` - May be automatic
- [ ] Handle 16-bit word alignment for flash writes
- [ ] Map byte addresses to EEPROM emulation variables efficiently
- [ ] **No changes needed to application code!**

### Testing
- [ ] Test read/write operations
- [ ] Test power-loss recovery
- [ ] Verify flash wear leveling

## Phase 5: mbedTLS Integration

### Library Setup
- [ ] Download mbedTLS 2.23.0 (or compatible)
- [ ] Copy to `Middlewares/mbedTLS/`
- [ ] Copy `mbedtls_config.h` to `Middlewares/mbedTLS/include/`
- [ ] Add mbedTLS source files to project build

### Build Configuration
- [ ] Add include path: `Middlewares/mbedTLS/include`
- [ ] Add include path: `Middlewares/mbedTLS/include/mbedtls`
- [ ] Add preprocessor define: `MBEDTLS_CONFIG_FILE="mbedtls_config.h"`
- [ ] Verify `usb_serial.cpp` compiles with mbedTLS
- [ ] Test HMAC-SHA256 functionality

## Phase 6: Build System

### Compiler Settings
- [ ] Enable C++ support (C++11 or C++14)
- [ ] Set optimization level (-Os or -O2)
- [ ] Add preprocessor defines:
  - [ ] `MBEDTLS_CONFIG_FILE="mbedtls_config.h"`
  - [ ] `ENABLE_TEST_COMMANDS` (for debug builds)

### Include Paths
- [ ] Verify `Core/Inc` is in include paths
- [ ] Add `lib/crypto_config` if needed
- [ ] Add mbedTLS include paths

### Git Hash Injection
- [ ] Set up pre-build step for git hash
- [ ] Create `fw_build_hash.h` or integrate into existing header
- [ ] Verify `FW_BUILD_HASH` is available in code

## Phase 7: Code Cleanup

### Includes & Dependencies
- [ ] Verify all includes resolve correctly
- [ ] Remove unused includes
- [ ] Update relative paths if files moved

### Compilation
- [ ] Fix all compilation errors
- [ ] Address all warnings (or document them)
- [ ] Verify project builds successfully

## Phase 8: Testing

### Hardware Tests
- [ ] GPIO (tap link) - read/write, open-drain
- [ ] GPIO (status LEDs) - verify LED patterns work correctly
- [ ] USB CDC - connect, enumerate, communicate
- [ ] Flash storage - read/write, power cycle
- [ ] Device UID - read correctly
- [ ] Timing - `millis()`, `micros()`, delays accurate

### Functional Tests
- [ ] Storage: `begin()`, `saveNow()`, `clearAll()`, `addLink()`
- [ ] USB commands: `hello`, `get_state`, `clear`, `dump`
- [ ] USB commands: `provision_key`, `sign_state`
- [ ] Tap link: connection protocol, state machine
- [ ] Status display: verify LED patterns for each state
  - [ ] Booting pattern
  - [ ] Idle pattern
  - [ ] Detecting pattern
  - [ ] Negotiating pattern
  - [ ] Exchanging pattern
  - [ ] Success pattern
  - [ ] Error pattern
  - [ ] Master/Slave role indication
- [ ] Crypto: HMAC-SHA256 signing
- [ ] Power management: behavior during power cycles

### Integration Tests
- [ ] End-to-end tap link connection flow
- [ ] Key provisioning and signing workflow
- [ ] Data persistence across power cycles
- [ ] Flash wear leveling (if monitoring available)

### Comparison Tests
- [ ] Command responses match PlatformIO version
- [ ] Timing behavior matches
- [ ] Storage format compatible (if possible)
- [ ] Performance similar or better

## Phase 9: Documentation & Cleanup

- [ ] Update code comments
- [ ] Document any deviations from migration plan
- [ ] Update README with STM32CubeIDE build instructions
- [ ] Commit changes to git
- [ ] Tag release version

## Post-Migration

- [ ] Verify all checklist items completed
- [ ] Code review
- [ ] Final testing on target hardware
- [ ] Archive PlatformIO project (keep as reference)
- [ ] Update project documentation

---

## Quick Reference: Function Replacements

| Arduino | STM32 HAL | Notes |
|---------|-----------|-------|
| `millis()` | `HAL_GetTick()` | Requires SysTick configured |
| `micros()` | Custom using DWT | Enable DWT in `HAL_Init()` |
| `delay(ms)` | `HAL_Delay(ms)` | Blocking delay |
| `delayMicroseconds(us)` | Custom implementation | Use DWT or timer |
| `pinMode(pin, mode)` | `HAL_GPIO_Init()` | Configure GPIO struct first |
| `digitalRead(pin)` | `HAL_GPIO_ReadPin()` | Returns `GPIO_PIN_SET` or `GPIO_PIN_RESET` |
| `digitalWrite(pin, val)` | `HAL_GPIO_WritePin()` | Use `GPIO_PIN_SET`/`GPIO_PIN_RESET` |
| `Serial.begin(baud)` | USB CDC init | Wait for `CDC_IsConnected()` |
| `Serial.available()` | `CDC_GetRxBufferLen()` | Check buffer length |
| `Serial.read()` | `CDC_Receive()` | Read from USB buffer |
| `Serial.print(str)` | `CDC_Transmit()` | Non-blocking if possible |
| `platform_storage_begin(size)` | `EE_Init()` | EEPROM emulation init (in HAL impl) |
| `platform_storage_read(addr)` | `EE_ReadVariable()` | Read from emulated EEPROM (in HAL impl) |
| `platform_storage_write(addr, val)` | `EE_WriteVariable()` | Write to emulated EEPROM (in HAL impl) |
| `platform_storage_commit()` | (automatic) | Not needed with EEPROM emulation |
| `platform_gpio_pin_mode(pin, mode)` | `HAL_GPIO_Init()` | Configure GPIO (in HAL impl) |
| `platform_gpio_read(pin)` | `HAL_GPIO_ReadPin()` | Read GPIO (in HAL impl) |
| `platform_gpio_write(pin, state)` | `HAL_GPIO_WritePin()` | Write GPIO (in HAL impl) |

---

**Last Updated:** [Current Date]
