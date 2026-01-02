# Refactoring Guide: Testability & Migration Improvements

This document identifies refactoring opportunities to improve testability, test isolation, and STM32CubeIDE migration readiness.

---

## Executive Summary

| Priority | Area | Issue | Status |
|----------|------|-------|--------|
| ✅ Done | `main.cpp` | Arduino.h dependency + hardcoded pins | Completed |
| ✅ Done | `device_id.cpp` | Direct HAL dependency not abstracted | Completed |
| ✅ Done | `usb_serial.cpp` | Tight coupling to Storage | Completed (uses IStorage) |
| ✅ Done | `main.cpp` | Global state, tight coupling | Completed (Application class) |
| 🟡 Medium | `tap_link.cpp` | `#ifdef EVAL_BOARD_TEST` duplication | Pending |

---

## Completed Refactoring

### ✅ 1. Pin Configuration Abstraction (`board_config.h`)
- Centralized pin definitions (STATUS_LED0_PIN, STATUS_LED1_PIN, TAP_LINK_PIN)
- Override via build flags for different boards
- Includes Arduino.h conditionally for STM32 pin constants

### ✅ 2. Device ID Abstraction (`platform_device.h`)
- Created `platform_get_device_uid()` abstraction
- `device_id.cpp` no longer has direct HAL dependency
- Arduino implementation in `platform_device_arduino.cpp`

### ✅ 3. Storage Interface (`i_storage.h`)
- Abstract interface for all storage operations
- `Storage` class implements `IStorage`
- `UsbCommandHandler` now accepts `IStorage&` (not `Storage&`)
- `MockStorage` class for unit testing in `test/mock_storage.h`

### ✅ 4. Application Class (`application.h`)
- All business logic extracted from `main.cpp` into `Application` class
- `main.cpp` now just 18 lines (setup/loop delegate to Application)
- Components (Storage, TapLink, StatusDisplay) are class members, not globals
- Master/slave command handling in separate methods for clarity
- Battery mode handling isolated in `handleBatteryMode()`

---

## 1. Remove Arduino.h from main.cpp

### Current Problem

```cpp
// main.cpp line 1
#include <Arduino.h>  // Only needed for PA5 constant and setup()/loop()
```

**Issues:**
- `PA5` constant used for LED pins (line 14)
- `setup()` and `loop()` are Arduino-specific entry points
- Blocks migration to STM32CubeIDE

### Solution

**Step 1:** Create pin configuration header:

```cpp
// include/board_config.h
#pragma once
#include <stdint.h>

// Pin definitions - override via build flags for different boards
#ifndef STATUS_LED0_PIN
#define STATUS_LED0_PIN 5  // PA5 equivalent
#endif

#ifndef STATUS_LED1_PIN
#define STATUS_LED1_PIN 6  // PA6 equivalent
#endif

#ifndef TAP_LINK_PIN
#define TAP_LINK_PIN 9     // PA9 equivalent
#endif
```

**Step 2:** Update `main.cpp`:

```cpp
// main.cpp - no Arduino.h needed!
#include "board_config.h"
#include "storage.h"
#include "usb_serial.h"
// ... rest unchanged
```

**Step 3:** For migration, use conditional compilation for entry point:

```cpp
#ifdef ARDUINO
void setup() { app_init(); }
void loop() { app_loop(); }
#else
int main(void) {
    HAL_Init();
    SystemClock_Config();
    app_init();
    while (1) { app_loop(); }
}
#endif
```

---

## 2. Abstract Device ID Behind Platform Layer

### Current Problem

```cpp
// device_id.cpp
#include "stm32l0xx_hal.h"   // Direct HAL dependency

void getDeviceUidRaw(uint8_t out[DEVICE_UID_LEN]) {
    uint32_t w0 = HAL_GetUIDw0();  // HAL-specific!
    uint32_t w1 = HAL_GetUIDw1();
    uint32_t w2 = HAL_GetUIDw2();
    // ...
}
```

**Issues:**
- Direct HAL call in application code
- Can't mock for testing
- Works by accident in Arduino (Arduino STM32 includes HAL)

### Solution

**Step 1:** Create platform abstraction:

```cpp
// include/platform_device.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read device unique ID (96 bits = 12 bytes)
void platform_get_device_uid(uint8_t out[12]);

#ifdef __cplusplus
}
#endif
```

**Step 2:** Create Arduino implementation:

```cpp
// src/platform_device_arduino.cpp
#include "platform_device.h"
#include "stm32l0xx_hal.h"

void platform_get_device_uid(uint8_t out[12]) {
    uint32_t w0 = HAL_GetUIDw0();
    uint32_t w1 = HAL_GetUIDw1();
    uint32_t w2 = HAL_GetUIDw2();
    // ... same logic
}
```

**Step 3:** Update `device_id.cpp` to use abstraction:

```cpp
// device_id.cpp - now platform-independent
#include "device_id.h"
#include "platform_device.h"

void getDeviceUidRaw(uint8_t out[DEVICE_UID_LEN]) {
    platform_get_device_uid(out);
}
```

---

## 3. Refactor EVAL_BOARD vs BATTERY Mode

### Current Problem

The `#ifdef EVAL_BOARD_TEST` conditional appears:
- `main.cpp`: lines 19-48, 62-99, 129-270 (150+ lines of duplicated branches)
- `tap_link.h`: lines 52-65, 72-155 (different class members)
- `tap_link.cpp`: lines 8-51, 54-425, 725-796 (massive duplication)

**Issues:**
- ~400 lines of code duplication
- Hard to test both modes
- Easy to miss fixing bugs in one branch

### Solution: Strategy Pattern

**Step 1:** Create mode interface:

```cpp
// include/tap_link_mode.h
#pragma once

class ITapLinkMode {
public:
    virtual ~ITapLinkMode() = default;
    
    // Called from main loop
    virtual void poll() = 0;
    
    // Connection events
    virtual bool isConnectionComplete() = 0;
    virtual void handleConnectionComplete(Storage& storage) = 0;
    
    // Role info
    virtual bool hasRole() const = 0;
    virtual bool isMaster() const = 0;
    
    // Reset state
    virtual void reset() = 0;
};
```

**Step 2:** Create mode implementations:

```cpp
// src/tap_link_mode_eval.cpp - Eval board mode (USB-powered, continuous)
class TapLinkModeEval : public ITapLinkMode {
    // Current EVAL_BOARD_TEST logic
};

// src/tap_link_mode_battery.cpp - Battery mode (sleep/wake)
class TapLinkModeBattery : public ITapLinkMode {
    // Current non-EVAL_BOARD_TEST logic
};
```

**Step 3:** Factory based on build config:

```cpp
ITapLinkMode* createTapLinkMode(IOneWireHal* hal) {
#ifdef EVAL_BOARD_TEST
    return new TapLinkModeEval(hal);
#else
    return new TapLinkModeBattery(hal);
#endif
}
```

**Benefits:**
- Each mode is a separate, focused class
- Can unit test each mode independently
- Easier to add new modes (e.g., low-power eval mode)

---

## 4. Extract Application Class from main.cpp

### Current Problem

```cpp
// main.cpp - global state
Storage gStorage;
UsbCommandHandler gUsb;
TapLink* gTapLink = nullptr;
StatusDisplay gStatusDisplay;

static uint32_t g_connectionDetectedTime = 0;  // More globals
static uint32_t g_lastCommandTime = 0;
```

**Issues:**
- Globals make testing difficult
- Can't run multiple test scenarios without reset
- Tight coupling between components

### Solution: Application Class

```cpp
// include/application.h
#pragma once
#include "storage.h"
#include "usb_serial.h"
#include "tap_link.h"
#include "status_display.h"

class Application {
public:
    Application();
    ~Application();
    
    void init();
    void loop();
    
    // For testing - expose components
    Storage& getStorage() { return _storage; }
    
private:
    Storage _storage;
    UsbCommandHandler _usb;
    TapLink* _tapLink;
    StatusDisplay _statusDisplay;
    
    uint32_t _connectionDetectedTime;
    uint32_t _lastCommandTime;
    
    void updateStatusDisplay();
    void handleMasterCommands();
    void handleSlaveCommands();
};
```

**Benefits:**
- Single responsibility
- Testable in isolation
- Dependency injection possible

---

## 5. Add IStorage Interface for Mocking

### Current Problem

```cpp
// UsbCommandHandler - tight coupling
void cmdGetState(Storage& storage);
void cmdClear(Storage& storage);
```

**Issues:**
- Can't test command handler without real storage
- Can't simulate storage failures

### Solution: Storage Interface

```cpp
// include/i_storage.h
#pragma once

class IStorage {
public:
    virtual ~IStorage() = default;
    
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual bool saveNow() = 0;
    
    virtual PersistPayloadV1& state() = 0;
    
    virtual bool hasSecretKey() const = 0;
    virtual const uint8_t* getSecretKey() const = 0;
    virtual uint8_t getKeyVersion() const = 0;
    virtual void setSecretKey(uint8_t version, const uint8_t key[32]) = 0;
    
    virtual void clearAll() = 0;
    virtual bool addLink(const uint8_t peerId[12]) = 0;
    virtual void incrementTapCount() = 0;
    virtual void saveTapCountOnly() = 0;
    virtual void saveLinkOnly() = 0;
};

// Storage class implements IStorage
class Storage : public IStorage {
    // ... existing implementation
};
```

**Test Usage:**

```cpp
// test/mock_storage.h
class MockStorage : public IStorage {
public:
    bool begin() override { return beginResult; }
    bool beginResult = true;
    
    void clearAll() override { clearAllCalled = true; }
    bool clearAllCalled = false;
    
    // ... mock all methods
};
```

---

## 6. Remove Arduino.h from tap_link_hal_arduino.cpp

### Current Problem

```cpp
// tap_link_hal_arduino.cpp line 4
#include <Arduino.h>  // Needed for PA9 pin definition
```

### Solution

Use the `board_config.h` from solution #1:

```cpp
// tap_link_hal_arduino.cpp
#include "tap_link_hal.h"
#include "board_config.h"  // For TAP_LINK_PIN
#include "platform_timing.h"
#include "platform_gpio.h"
// No Arduino.h needed!
```

---

## 7. Separate Command Parsing from Execution

### Current Problem

```cpp
void UsbCommandHandler::handleLine(Storage &storage, const char *line) {
    // Parsing AND execution mixed together
    if (strcmp(cmd, "HELLO") == 0) {
        cmdHello(storage);  // Immediately executes
    }
}
```

**Issues:**
- Can't test parsing independently
- Hard to add command queuing/batching

### Solution: Command Pattern

```cpp
// include/command.h
enum class CommandType {
    None,
    Hello,
    GetState,
    Clear,
    Dump,
    ProvisionKey,
    SignState,
    GetKey  // Test only
};

struct ParsedCommand {
    CommandType type;
    int offset;      // For DUMP
    int count;       // For DUMP
    int version;     // For PROVISION_KEY
    char keyHex[65]; // For PROVISION_KEY
    char nonce[65];  // For SIGN_STATE
};

class UsbCommandHandler {
public:
    // Separate parsing from execution
    ParsedCommand parseCommand(const char* line);
    void executeCommand(const ParsedCommand& cmd, Storage& storage);
    
    // Legacy combined method (for backward compatibility)
    void handleLine(Storage& storage, const char* line);
};
```

**Test Usage:**

```cpp
void test_parse_dump_command() {
    UsbCommandHandler handler;
    auto cmd = handler.parseCommand("DUMP 5 10");
    
    ASSERT_EQ(cmd.type, CommandType::Dump);
    ASSERT_EQ(cmd.offset, 5);
    ASSERT_EQ(cmd.count, 10);
}
```

---

## Implementation Priority

### Phase 1: Migration Blockers (Do First)

| Task | Files | Effort |
|------|-------|--------|
| 1. Create `board_config.h` | New file + update main.cpp | 1 hour |
| 2. Abstract device ID | platform_device.h/cpp + device_id.cpp | 2 hours |
| 6. Remove Arduino.h from tap_link_hal | tap_link_hal_arduino.cpp | 30 min |

### Phase 2: Testability (Do Second)

| Task | Files | Effort |
|------|-------|--------|
| 4. Extract Application class | New application.h/cpp + main.cpp | 3 hours |
| 5. Add IStorage interface | i_storage.h + storage.h | 1 hour |
| 7. Separate command parsing | usb_serial.h/cpp | 2 hours |

### Phase 3: Code Quality (Do When Needed)

| Task | Files | Effort |
|------|-------|--------|
| 3. Strategy pattern for modes | tap_link_mode.h + implementations | 4-6 hours |

---

## Testing Strategy After Refactoring

### Unit Tests Enabled

| Component | What Can Be Tested |
|-----------|-------------------|
| `UsbCommandHandler` | Command parsing, validation |
| `Storage` (with mock platform) | Save/load logic, link management |
| `StatusDisplay` (with mock GPIO) | Pattern sequencing, LED states |
| `TapLink` (with mock HAL) | State machine, negotiation logic |

### Integration Tests

| Test | Setup |
|------|-------|
| Full command flow | MockStorage + real UsbCommandHandler |
| Tap link protocol | MockOneWireHal + real TapLink |
| Status display patterns | MockGPIO + real StatusDisplay |

### Hardware Tests (On Device)

| Test | Description |
|------|-------------|
| Two-board tap test | Full end-to-end tap link |
| USB serial commands | Real USB CDC communication |
| Flash persistence | Power cycle + verify data |

---

## File Changes Summary

### New Files

```
include/
  board_config.h          # Pin definitions
  platform_device.h       # Device ID abstraction
  i_storage.h             # Storage interface (optional)
  application.h           # App orchestration (optional)
  
src/
  platform_device_arduino.cpp  # Arduino device ID
  application.cpp              # App implementation (optional)
```

### Modified Files

```
main.cpp                  # Remove Arduino.h, use board_config.h
device_id.cpp             # Use platform_device abstraction
tap_link_hal_arduino.cpp  # Remove Arduino.h, use board_config.h
```

### Unchanged Files

```
storage.cpp, usb_serial.cpp, tap_link.cpp, status_display.cpp
# Already use platform abstractions correctly
```

---

*Last Updated: January 2026*

