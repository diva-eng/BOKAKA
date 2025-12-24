# Tap Interface Wiring Guide

## Overview

This document describes how to wire two Nucleo boards (or STM32L053R8-based boards) together to test the tap link interface. The tap link uses a 1-wire open-drain protocol for device-to-device communication.

## Required Components

- **2x Nucleo-L053R8 boards** (or compatible STM32L053R8 boards)
- **Jumper wires** (at least 3 wires: GND, 3.3V, and tap link signal)
- **USB cables** (2x) for power and USB CDC communication
- **Optional: External LEDs** (2x per board) for status indication

## Connection Diagram

```
Board A (Device 1)          Board B (Device 2)
┌─────────────────┐        ┌─────────────────┐
│                 │        │                 │
│  GND ───────────┼────────┼── GND          │
│                 │        │                 │
│  3.3V ──────────┼────────┼── 3.3V         │
│                 │        │                 │
│  PA9 (D8) ──────┼────────┼── PA9 (D8)      │
│  (Tap Link)     │        │  (Tap Link)     │
│                 │        │                 │
│  PA5 ────[LED]──┤        │  PA5 ────[LED]──┤
│  (Status LED0)  │        │  (Status LED0)  │
│                 │        │                 │
│  PA6 ────[LED]──┤        │  PA6 ────[LED]──┤
│  (Status LED1)  │        │  (Status LED1)  │
│                 │        │                 │
└─────────────────┘        └─────────────────┘
```

## Pin Assignments

### Default Pin Configuration

| Function | Pin | Arduino Name | STM32 Pin | Notes |
|----------|-----|--------------|-----------|-------|
| **Tap Link** | PA9 | `PA9` (D8) | GPIO Port A, Pin 9 | 1-wire open-drain interface, CN5 digital header |
| **Status LED 0** | PA5 | `PA5` (D13) | GPIO Port A, Pin 5 | Device readiness/handshake status, CN5 digital header |
| **Status LED 1** | PA6 | `PA6` (D12) | GPIO Port A, Pin 6 | Master/Slave role indication, CN5 digital header |

**Note:** Pins can be reconfigured via build flags. See [Configuration](#configuration) section.

## Wiring Instructions

### Step 1: Power Connections

**⚠️ IMPORTANT: Connect GND first, then power!**

1. **Connect Ground (GND)**
   - Connect **GND** pin on Board A to **GND** pin on Board B
   - Use a jumper wire between the GND pins on both boards
   - **This is critical** - both boards must share a common ground reference

2. **Connect Power (3.3V)**
   - Connect **3.3V** pin on Board A to **3.3V** pin on Board B
   - Use a jumper wire between the 3.3V pins on both boards
   - **Note:** Only one board needs to be powered via USB. The 3.3V connection allows both boards to share power.

**Power Options:**
- **Option A (Recommended):** Power both boards via USB, connect 3.3V together for redundancy
- **Option B:** Power only one board via USB, connect 3.3V to power the second board
  - Ensure the powered board can supply enough current for both boards
  - Check board specifications for maximum current draw

### Step 2: Tap Link Signal Connection

1. **Connect Tap Link Signal**
   - Connect **PA9 (D8)** pin on Board A to **PA9 (D8)** pin on Board B
   - Use a jumper wire between D8 pins on both boards (CN5 digital header)
   - This is the 1-wire open-drain communication line

**How it works:**
- Both boards configure PA9 as **input with pull-up** when idle (line is HIGH)
- When a board wants to send data, it switches PA9 to **output LOW** (pulls line LOW)
- The open-drain configuration allows both boards to drive the line LOW without conflict
- The internal pull-up resistors keep the line HIGH when neither board is driving it

**Why PA9 (D8)?**
- Located on **CN5 digital header** for easy access (better than PA0 on analog header)
- Clearly labeled as **D8** on the board
- Supports open-drain mode required for 1-wire communication
- Not used by USB CDC (we use USB, not USART1 which could use PA9)

### Step 3: Status LED Connections (Optional)

For visual feedback during testing, connect external LEDs:

1. **Status LED 0 (PA5)** - Device readiness/handshake status
   - Connect LED anode (long leg) to PA5 via a 220Ω resistor
   - Connect LED cathode (short leg) to GND
   - Repeat for both boards

2. **Status LED 1 (PA6)** - Master/Slave role indication
   - Connect LED anode (long leg) to PA6 via a 220Ω resistor
   - Connect LED cathode (short leg) to GND
   - Repeat for both boards

**LED Circuit:**
```
PA5/PA6 ──[220Ω]──[LED]── GND
         (resistor) (anode→cathode)
```

**Note:** If using built-in LEDs on the Nucleo board, check your board's pinout. The default configuration assumes external LEDs.

## Complete Wiring Checklist

- [ ] **GND**: Board A GND → Board B GND
- [ ] **3.3V**: Board A 3.3V → Board B 3.3V
- [ ] **PA9 (D8)**: Board A D8 → Board B D8 (Tap Link, CN5 digital header)
- [ ] **PA5 LED** (optional): Board A PA5 → LED → GND
- [ ] **PA6 LED** (optional): Board A PA6 → LED → GND
- [ ] **PA5 LED** (optional): Board B PA5 → LED → GND
- [ ] **PA6 LED** (optional): Board B PA6 → LED → GND
- [ ] **USB Power**: Connect USB cables to both boards (or just one if sharing power)

## Configuration

### Changing Pin Assignments

If you need to use different pins, configure them via build flags in `platformio.ini`:

```ini
build_flags =
    -DTAP_LINK_PIN=PA9        # Change tap link pin (default: PA9/D8)
    -DSTATUS_LED0_PIN=PA5     # Change status LED 0 pin (default: PA5/D13)
    -DSTATUS_LED1_PIN=PA6     # Change status LED 1 pin (default: PA6/D12)
```

For STM32CubeIDE, define in project settings or `main.h`:

```c
#define TAP_LINK_PIN GPIO_PIN_9    // PA9 (D8 on CN5)
#define TAP_LINK_PORT GPIOA
#define STATUS_LED0_PIN GPIO_PIN_5  // PA5 (D13 on CN5)
#define STATUS_LED1_PIN GPIO_PIN_6  // PA6 (D12 on CN5)
```

## Testing the Connection

### 1. Initial Setup

1. **Flash firmware** to both boards
2. **Connect USB cables** to both boards for power and USB CDC communication
3. **Verify wiring** using the checklist above

### 2. Power-On Test

1. **Power on both boards**
2. **Observe Status LEDs:**
   - LED 0 should show **Booting** pattern (120ms ON, 380ms OFF)
   - LED 1 should show **Unknown** pattern (90ms ON, 910ms OFF - very slow blink)
3. **After boot**, LED 0 should switch to **Idle** pattern (120ms ON, 880ms OFF)

### 3. Connection Test

1. **Ensure both boards are in Idle state** (LED 0 showing idle pattern)
2. **Connect the tap link wire** (PA9/D8 to PA9/D8) if not already connected
3. **Observe LED patterns:**
   - LED 0 should transition through:
     - **Detecting** (double blink: 120ms ON, 120ms OFF, 120ms ON, 640ms OFF)
     - **Negotiating** (fast blink: 150ms ON, 150ms OFF)
     - **Exchanging** (medium blink: 220ms ON, 220ms OFF)
     - **Success** (slow blink: 500ms ON, 500ms OFF)
   - LED 1 should show:
     - **Master** (steady ON) on one board
     - **Slave** (steady OFF) on the other board

### 4. USB Serial Monitoring

Connect to both boards via USB CDC (serial monitor) to observe:

```bash
# Board A (COM port may vary)
# Board B (COM port may vary)
```

Send commands to verify connection:
- `hello` - Check device is responding
- `get_state` - View connection count and links
- `dump` - View stored peer IDs

## Troubleshooting

### No Connection Detected

**Symptoms:** LED 0 stays in Idle pattern, no handshake occurs

**Possible Causes:**
1. **Missing GND connection** - Most common issue!
   - Verify GND is connected between both boards
   - Check continuity with multimeter
2. **Wrong pin** - Verify PA9 (D8) is connected on both boards (CN5 digital header)
3. **Firmware not running** - Check USB serial for boot messages
4. **Pin configuration** - Verify pin assignments match firmware

**Solutions:**
- Double-check all connections
- Use multimeter to verify continuity
- Check USB serial output for error messages
- Verify firmware is flashed correctly

### Connection Fails During Handshake

**Symptoms:** LED 0 shows Detecting/Negotiating but never reaches Success

**Possible Causes:**
1. **Timing issues** - Line noise or poor connections
2. **Power issues** - Insufficient power supply
3. **Pin damage** - PA9 (D8) may be damaged

**Solutions:**
- Check wire connections are secure
- Try shorter wires (reduce noise)
- Verify power supply is adequate
- Test with different pins if available

### LEDs Not Working

**Symptoms:** Status LEDs don't light up or show wrong patterns

**Possible Causes:**
1. **Wrong pins** - LED pins don't match configuration
2. **LED polarity** - LED connected backwards
3. **Missing resistor** - LED may be damaged without current limiting
4. **Built-in LED** - Using wrong pin for board's built-in LED

**Solutions:**
- Verify LED pin assignments in firmware
- Check LED polarity (anode to pin, cathode to GND)
- Add 220Ω resistor if missing
- Check board documentation for built-in LED pin

### Both Boards Show as Master

**Symptoms:** Both LED 1s are ON (both think they're master)

**Possible Causes:**
1. **Negotiation failure** - IDs may be identical (very unlikely with STM32 UIDs)
2. **Timing issue** - Handshake didn't complete properly

**Solutions:**
- Check USB serial for error messages
- Reset both boards and try again
- Verify firmware is correct version on both boards

## Safety Considerations

### Power Supply

- **Maximum current:** Ensure power supply can handle both boards
- **Voltage:** Use 3.3V only - do not connect 5V to 3.3V pins!
- **Reverse polarity:** Double-check connections before powering on

### ESD Protection

- **Static discharge:** Handle boards carefully to avoid ESD damage
- **Work surface:** Use anti-static mat if available

### Short Circuits

- **Check connections:** Verify no accidental shorts before powering on
- **Multimeter:** Use continuity test to verify connections

## Advanced Configuration

### Using Different Pins

If PA9, PA5, or PA6 are not available:

1. **Choose alternative pins** that support:
   - GPIO input/output
   - Internal pull-up resistors (for tap link)
   - Open-drain mode (for tap link, if hardware supports it)

2. **Update firmware configuration:**
   ```ini
   build_flags =
       -DTAP_LINK_PIN=PA0        # Example: Use PA0 (A0) instead
       -DSTATUS_LED0_PIN=PB1     # Example: Use PB1 instead
       -DSTATUS_LED1_PIN=PB2     # Example: Use PB2 instead
   ```

3. **Update wiring** to match new pin assignments

### Multiple Device Testing

To test with more than 2 devices:

1. **Star topology:** Connect all tap link pins (PA9/D8) together
2. **Common GND:** Connect all GND pins together
3. **Common 3.3V:** Connect all 3.3V pins together (ensure adequate power)
4. **Protocol behavior:** The protocol supports 1-to-1 connections. Multiple devices will negotiate in pairs.

## Pin Location Reference

### Nucleo-L053R8 Pinout

```
CN5 (Digital Header - Right Side):
  ┌─────────┬─────────┐
  │  D8      │  D9     │  ← D8 (PA9) for tap link
  │  D10     │  D11    │
  │  D12     │  D13    │  ← D12 (PA6) for LED 1, D13 (PA5) for LED 0
  │  GND     │  AREF   │
  │  D14     │  D15    │
  └─────────┴─────────┘

CN6 (Power Header - Left Side):
  ┌─────────┬─────────┐
  │  VIN    │  GND    │
  │  5V     │  GND    │
  │  3.3V   │  GND    │  ← Use this 3.3V and GND
  │  IOREF  │  RESET  │
  └─────────┴─────────┘

CN8 (Analog Header - Left Side):
  ┌─────────┬─────────┐
  │  A0     │  A1     │  ← A0 is PA0 (not used for tap link)
  │  A2     │  A3     │
  │  A4     │  A5     │
  └─────────┴─────────┘
```

**Note:** Pin locations may vary by board revision. Check your board's documentation.

## See Also

- `STATUS_DISPLAY_README.md` - Status LED pattern documentation
- `include/tap_link.h` - Tap link protocol documentation
- `MIGRATION_PLAN.md` - STM32 HAL migration guide

---

**Last Updated:** December 2025
