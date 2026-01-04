# Tap Link Design

## Overview

TapLink implements device-to-device communication over a single GPIO wire using an open-drain protocol. It handles connection detection, role negotiation, and bidirectional command exchange.

## Operation Modes

### Eval Board Mode (EVAL_BOARD_TEST)

USB-powered continuous monitoring for development:
- Sends periodic presence pulses
- Automatic role negotiation on connection
- Command protocol for peer communication

### Battery Mode (default)

CR2032-powered with sleep/wake:
- MCU sleeps until tap wake-up interrupt
- Validates connection stability
- Minimal power consumption

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Application                         │
├─────────────────────────────────────────────────────────────┤
│         ITapLink / ITapLinkEval / ITapLinkBattery           │
├─────────────────────────────────────────────────────────────┤
│                           TapLink                           │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ Detection   │  │ Negotiation  │  │ Command Protocol   │  │
│  │ State       │  │ (UID bits)   │  │ (Master/Slave)     │  │
│  └─────────────┘  └──────────────┘  └────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                         IOneWireHal                         │
│                 (tap_link_hal_arduino.cpp)                  │
├─────────────────────────────────────────────────────────────┤
│                      Platform GPIO HAL                      │
└─────────────────────────────────────────────────────────────┘
```

## Electrical Interface

### Open-Drain Protocol

```
          Device A                    Device B
             │                           │
   ┌─────────┴─────────┐       ┌─────────┴─────────┐
   │ GPIO (Open-Drain) │───────│ GPIO (Open-Drain) │
   │ + Internal Pull-up│       │ + Internal Pull-up│
   └───────────────────┘       └───────────────────┘
```

- Idle state: HIGH (via internal pull-ups)
- Either device can pull LOW
- Wired-AND: line is HIGH only if both devices release
- No bus contention possible

### GPIO Configuration

| Mode | Config | Description |
|------|--------|-------------|
| Release | INPUT_PULLUP | High-Z with pull-up (line goes HIGH) |
| Drive LOW | OUTPUT + LOW | Actively pulls line to ground |

## Detection State Machine (Eval Mode)

```
┌──────────────┐
│ NoConnection │◄────────────────────────────────┐
└──────┬───────┘                                 │
       │ Line goes LOW (peer pulse)              │
       ▼                                         │
┌──────────────┐                                 │
│  Detecting   │                                 │
└──────┬───────┘                                 │
       │ Line goes HIGH or debounce timeout      │
       ▼                                         │
┌──────────────┐                                 │
│ Negotiating  │                                 │
└──────┬───────┘                                 │
       │ Role determined                         │
       ▼                                         │
┌──────────────┐                                 │
│  Connected   │─────────────────────────────────┘
└──────────────┘   Timeout or repeated failures
```

## Presence Pulse Detection

When not connected, both devices send periodic presence pulses:

```
Timing:
- Pulse width: 2ms (PRESENCE_PULSE_US)
- Pulse interval: 50ms (PULSE_INTERVAL_US)
- Debounce time: 5ms (DEBOUNCE_TIME_US)

       Device A sends pulse                  Device B detects
              │                                    │
    HIGH ─────┴────┐          ┌────────────────────┴───────── HIGH
                   │          │
    LOW            └──────────┘
                   ◄──────────► 2ms
```

## Role Negotiation Protocol

### Synchronization Handshake

Both devices must start bit exchange simultaneously:

```
1. Release line, wait for HIGH
2. Send 10ms sync pulse
3. Wait for line HIGH
4. Wait for peer's sync pulse (up to 50ms)
5. Wait for peer's pulse to complete
6. 5ms delay
7. Send second 10ms sync pulse
8. Wait for line HIGH
9. 5ms final alignment delay
10. Begin bit exchange
```

This achieves ~1-2ms synchronization accuracy.

### UID Bit Exchange

Higher UID becomes Master. First 32 bits of device UID are compared:

```
Timing per bit:
- Drive period: 5ms (BIT_DRIVE_US)
- Sample point: 2.5ms (BIT_SAMPLE_US)
- Recovery: 2ms (BIT_RECOVERY_US)

For each bit position (MSB first):
1. Drive line based on my bit (0 = LOW, 1 = release)
2. Wait 2.5ms (sample point)
3. Sample line 3 times with majority voting
4. Continue driving until 5ms total
5. Release line
6. 2ms recovery
7. Next bit

Decision logic:
- If I sent '1' (released) but line is LOW → peer sent '0' → I am MASTER
- If bits match, continue to next bit
- After 32 bits: use random tie-breaker, then UID sum parity
```

### Why 5ms Drive Period

The long drive period (5ms) with mid-point sampling (2.5ms) ensures reliable reading even with ~2ms sync error between devices. Both devices are guaranteed to be in their drive phase when sampling occurs.

## Command Protocol (Connected State)

### Packet Format

```
START pulse (5ms) → Turnaround (2ms) → Command byte → Turnaround (2ms) → Response byte
```

### Bit Timing

Same as negotiation: 5ms drive, 2.5ms sample, 2ms recovery.

### Commands

| Code | Command | Description |
|------|---------|-------------|
| 0x01 | CHECK_READY | Master polls slave availability |
| 0x02 | REQUEST_ID | Master requests slave's UID |
| 0x03 | SEND_ID | Master sends its UID to slave |

### Responses

| Code | Response | Description |
|------|----------|-------------|
| 0x06 | ACK | Command successful |
| 0x15 | NAK | Command rejected |

### ID Exchange Sequence

```
Master                              Slave
  │                                   │
  ├── START + REQUEST_ID ───────────►│
  │                                   │
  │◄───────────── ACK ────────────────┤
  │◄──────────── UID (12 bytes) ──────┤
  │                                   │
  ├── START + SEND_ID ───────────────►│
  ├── UID (12 bytes) ────────────────►│
  │                                   │
  │◄───────────── ACK ────────────────┤
  │                                   │
```

### Disconnect Detection

**Master side:**
- Tracks consecutive command failures
- After 3 failures (MAX_COMMAND_FAILURES): transition to NoConnection
- Invalid responses (0xFF from floating line) count as failures

**Slave side:**
- Tracks time since last command received
- After 2 seconds (SLAVE_IDLE_TIMEOUT_US): transition to NoConnection

## Timing Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| DEBOUNCE_TIME_US | 5,000 | Connection detection debounce |
| PRESENCE_PULSE_US | 2,000 | Presence pulse width |
| PULSE_INTERVAL_US | 50,000 | Time between presence pulses |
| BIT_DRIVE_US | 5,000 | Bit drive duration |
| BIT_SAMPLE_US | 2,500 | Sample point within bit slot |
| BIT_RECOVERY_US | 2,000 | Recovery between bits |
| SYNC_PULSE_US | 10,000 | Sync handshake pulse |
| SYNC_WAIT_US | 5,000 | Sync alignment delay |
| CMD_START_PULSE_US | 5,000 | Command start pulse |
| CMD_TURNAROUND_US | 2,000 | Send/receive turnaround |
| CMD_TIMEOUT_US | 100,000 | Command response timeout |
| SLAVE_IDLE_TIMEOUT_US | 2,000,000 | Slave disconnect timeout |

## Battery Mode State Machine

```
┌──────────────┐
│   Sleeping   │◄────────────────┐
└──────┬───────┘                 │
       │ Wake-up interrupt       │
       ▼                         │
┌──────────────┐                 │
│    Waking    │─────────────────┤ Connection not stable
└──────┬───────┘                 │
       │ Validation complete     │
       ▼                         │
┌──────────────┐                 │
│  Connected   │                 │
└──────┬───────┘                 │
       │ Disconnect detected     │
       ▼                         │
┌──────────────┐                 │
│ Disconnected │─────────────────┘
└──────────────┘
```

### Connection Validation

Multiple line readings confirm stable connection:

```cpp
bool readings[5];
for (int i = 0; i < 5; i++) {
    readings[i] = _hal->readLine();
    delayMicros(100);
}
// Connection valid if all readings are consistent
```

## HAL Interface

```cpp
struct IOneWireHal {
    virtual bool readLine() = 0;              // true = HIGH
    virtual void driveLow(bool enable) = 0;   // true = pull LOW
    virtual uint32_t micros() = 0;
    virtual void delayMicros(uint32_t us) = 0;
};
```

Platform implementation in `tap_link_hal_arduino.cpp` maps to GPIO and timing HAL.

