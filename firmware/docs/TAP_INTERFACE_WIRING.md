# Tap Interface Wiring Guide

## Overview

This document describes how to wire two Nucleo boards (or STM32L053R8-based boards) together to test the tap link interface. The tap link uses a 1-wire open-drain protocol for device-to-device communication.

---

## Required Components

- **2× Nucleo-L053R8 boards** (or compatible STM32L053R8 boards)
- **3× Jumper wires** (GND, 3.3V, and tap link signal)
- **2× USB cables** for power and USB CDC communication
- **Optional**: External LEDs (2× per board) for status indication

---

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

---

## Pin Assignments

| Function | Pin | Arduino Name | Notes |
|----------|-----|--------------|-------|
| **Tap Link** | PA9 | D8 | 1-wire open-drain, CN5 digital header |
| **Status LED 0** | PA5 | D13 | Readiness/handshake status |
| **Status LED 1** | PA6 | D12 | Master/Slave role indication |

---

## Wiring Instructions

### Step 1: Power Connections

⚠️ **Connect GND first, then power!**

1. **Ground (GND)**: Connect GND pins between both boards
2. **Power (3.3V)**: Connect 3.3V pins between both boards

### Step 2: Tap Link Signal

Connect **PA9 (D8)** on Board A to **PA9 (D8)** on Board B.

**How it works:**
- Both boards configure PA9 as **input with pull-up** when idle (line HIGH)
- When sending data, switch to **output LOW** (pulls line LOW)
- Open-drain allows both boards to drive LOW without conflict

### Step 3: Status LEDs (Optional)

```
PA5/PA6 ──[220Ω]──[LED]── GND
```

---

## Wiring Checklist

- [ ] **GND**: Board A GND → Board B GND
- [ ] **3.3V**: Board A 3.3V → Board B 3.3V  
- [ ] **PA9 (D8)**: Board A D8 → Board B D8 (Tap Link)
- [ ] **USB Power**: Connect USB cables to both boards

---

## Testing

### Power-On Test

1. Power on both boards
2. **LED 0** shows **Booting** pattern (120ms ON, 380ms OFF)
3. After boot, **LED 0** shows **Idle** pattern (120ms ON, 880ms OFF)

### Connection Test

1. Ensure both boards are in Idle state
2. Connect tap link wire (D8 to D8)
3. Observe LED patterns:
   - **LED 0**: Detecting → Negotiating → Success
   - **LED 1**: Master (ON) on one board, Slave (OFF) on other

### USB Serial Commands

| Command | Description |
|---------|-------------|
| `hello` | Check device is responding |
| `get_state` | View connection count and links |
| `dump` | View stored peer IDs |

---

## Troubleshooting

### No Connection Detected

| Cause | Solution |
|-------|----------|
| Missing GND connection | Verify GND is connected between boards |
| Wrong pin | Verify PA9 (D8) is connected on both boards |
| Firmware not running | Check USB serial for boot messages |

### Connection Fails During Handshake

| Cause | Solution |
|-------|----------|
| Timing issues | Check wire connections, use shorter wires |
| Power issues | Verify adequate power supply |

### LEDs Not Working

| Cause | Solution |
|-------|----------|
| Wrong pins | Verify LED pin assignments |
| LED polarity | Anode to pin, cathode to GND |
| Missing resistor | Add 220Ω current limiting resistor |

---

## Pin Location Reference (Nucleo-L053R8)

```
CN5 (Digital Header - Right Side):
  ┌─────────┬─────────┐
  │  D8      │  D9     │  ← D8 (PA9) for tap link
  │  D10     │  D11    │
  │  D12     │  D13    │  ← D12 (PA6) LED 1, D13 (PA5) LED 0
  │  GND     │  AREF   │
  └─────────┴─────────┘

CN6 (Power Header - Left Side):
  ┌─────────┬─────────┐
  │  3.3V   │  GND    │  ← Use this 3.3V and GND
  └─────────┴─────────┘
```

---

## See Also

- [STATUS_DISPLAY.md](STATUS_DISPLAY.md) - LED pattern documentation
- [TAP_LINK_TIMING.md](TAP_LINK_TIMING.md) - Protocol timing details

---

*Last Updated: January 2026*

