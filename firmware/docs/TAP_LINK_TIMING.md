# Tap Link Timing Diagram

## Overview

This document describes the timing of the 1-wire tap link protocol used for detecting connections and negotiating master/slave roles between two devices.

## Protocol Phases

1. **Detection Phase** - Presence pulses to detect peer
2. **Synchronization Phase** - Align timing between devices
3. **Negotiation Phase** - Exchange UID bits to determine roles
4. **Connected Phase** - Command protocol takes over (no more presence pulses)
5. **ID Exchange Phase** - Master and slave exchange UIDs for storage
6. **Maintenance Phase** - Periodic CHECK_READY to maintain connection

---

## Phase 1: Detection (Presence Pulses)

```
        PULSE_INTERVAL (50ms)           PULSE_INTERVAL (50ms)
    ├─────────────────────────────┼─────────────────────────────┤

Board A:
         ┌──┐                           ┌──┐
    ─────┘  └───────────────────────────┘  └─────────────────────
         2ms                            2ms
         pulse                          pulse

Board B:
              ┌──┐                           ┌──┐
    ──────────┘  └───────────────────────────┘  └────────────────
              2ms                            2ms
              pulse                          pulse

Wire (wired-AND):
         ┌──┐ ┌──┐                      ┌──┐ ┌──┐
    ─────┘  └─┘  └──────────────────────┘  └─┘  └────────────────
         A    B                         A    B
         detects                        detects
         B's pulse                      B's pulse

Detection triggers when either board sees the other's pulse.
After detecting a LOW pulse (or stable LOW for 5ms debounce), 
both boards transition to the Synchronization Phase.
```

---

## Phase 2: Synchronization Handshake

The synchronization procedure uses a robust multi-step handshake to align both devices:

```
STEP-BY-STEP SYNCHRONIZATION:

Board A (enters first):
│
├── Step 1: Release line, wait for HIGH (100ms timeout)
│           ────────────────┐  ┌────────
│                           └──┘ wait
│
├── Step 2: Brief 1ms settle time
│           ────────────────────────────
│
├── Step 3: First sync pulse (10ms LOW)
│           ────┐          ┌────────────
│               └──────────┘ 10ms
│
├── Step 4: Wait for line HIGH (20ms timeout)
│           ────────────────────────────
│
├── Step 5: Wait for peer's sync pulse (up to 50ms)
│           ────┐ ┌──────── (sees B's pulse)
│               └─┘
│
├── Step 6: Wait for peer's pulse to complete (line HIGH, 20ms timeout)
│           ────────────────────────────
│
├── Step 7: 5ms wait
│           ────────────────────────────
│
├── Step 8: Second sync pulse (10ms LOW)
│           ────┐          ┌────────────
│               └──────────┘ 10ms
│
├── Step 9: Wait for line HIGH (20ms timeout)
│           ────────────────────────────
│
└── Step 10: 5ms final alignment delay
            ────────────────────────────
            START NEGOTIATION ──────────>


Board B (enters slightly later) follows the same sequence.
Both boards become synchronized within ~1-2ms of each other.
```

### Detailed Sync Timeline (Typical Case)

```
Timeline:     0      10ms    20ms    30ms    40ms    50ms    60ms    70ms
              │       │       │       │       │       │       │       │

Board A:      │       │       │       │       │       │       │       │
  Release+1ms ├─┤     │       │       │       │       │       │       │
  1st pulse   │ ├─────────────┤       │       │       │       │       │
              │ │   10ms      │       │       │       │       │       │
  Wait peer   │ │             ├───────┤       │       │       │       │
              │ │             │see B  │       │       │       │       │
  5ms wait    │ │             │       ├──────┤│       │       │       │
  2nd pulse   │ │             │       │      ├────────────────┤       │
              │ │             │       │      │     10ms       │       │
  5ms final   │ │             │       │      │                ├──────┤│
  START ──────┴─┴─────────────┴───────┴──────┴────────────────┴──────┴┼>

Board B:      │       │       │       │       │       │       │       │
  (starts ~5ms later, same sequence)                                  │
  START ──────────────────────────────────────────────────────────────┼>

Total sync phase: ~50-70ms depending on relative timing
```

---

## Phase 3: Bit Negotiation (per bit)

```
BIT SLOT TIMING (one bit):
                          
    │◄────────────────────── 7ms total ──────────────────────►│
    │                                                          │
    │◄──────── 5ms DRIVE ────────►│◄─── 2ms RECOVERY ────►│
    │                              │                           │
    │      ▼ sample                │                           │
    │      │ point                 │                           │
    │◄ 2.5ms ►                     │                           │
    │      │                       │                           │
    │      ├─ 3x samples ─┤        │                           │
    │      │ (majority vote)       │                           │


Case 1: Board A sends '1', Board B sends '0' → A is MASTER
═══════════════════════════════════════════════════════════

Board A (bit=1, releases):
                   ┌───────────────────────────────────────────┐
    ───────────────┘                                           └───
               release            sample × 3                keep high
               (send '1')         at 2.5ms                  (recovery)
                                  majority vote
                                  sees LOW!
                                  → "I'm MASTER"

Board B (bit=0, drives LOW):
    ───────────────┐                                           ┌───
                   └───────────────────────────────────────────┘
               drive LOW          sample × 3                release
               (send '0')         at 2.5ms                  (recovery)
                                  sees LOW
                                  → can't determine, continue

Wire (wired-AND result):
    ───────────────┐                                           ┌───
                   └───────────────────────────────────────────┘
               LOW (B driving)    Both sample here          HIGH
                                  Line is LOW!


Case 2: Both send '1' → Continue to next bit
═════════════════════════════════════════════

Board A (bit=1, releases):
                   ┌───────────────────────────────────────────┐
    ───────────────┘                                           └───
               release            sample                   
               (send '1')         sees HIGH
                                  → both sent '1', continue

Board B (bit=1, releases):
                   ┌───────────────────────────────────────────┐
    ───────────────┘                                           └───
               release            sample                   
               (send '1')         sees HIGH
                                  → both sent '1', continue

Wire (wired-AND result):
                   ┌───────────────────────────────────────────┐
    ───────────────┘                                           └───
               HIGH (both released, pull-up wins)


Case 3: Both send '0' → Continue to next bit
═════════════════════════════════════════════

Board A (bit=0, drives LOW):
    ───────────────┐                                           ┌───
                   └───────────────────────────────────────────┘
               drive LOW          sample                   release
               (send '0')         sees LOW
                                  → I sent 0, can't tell peer's bit

Board B (bit=0, drives LOW):
    ───────────────┐                                           ┌───
                   └───────────────────────────────────────────┘
               drive LOW          sample                   release
               (send '0')         sees LOW
                                  → I sent 0, can't tell peer's bit

Wire (wired-AND result):
    ───────────────┐                                           ┌───
                   └───────────────────────────────────────────┘
               LOW (both driving)                          HIGH
```

### Multi-Sample Voting

The implementation takes 3 samples with 100μs delays for reliability:

```
    Sample Window (at 2.5ms mark):
    │◄───── ~200μs ─────►│
    
    ─────┬─────┬─────┬─────
         │     │     │
         S1    S2    S3
         │     │     │
         100μs 100μs
         delay delay
    
    Result = majority of (S1, S2, S3)
    Line is LOW if 2+ samples are LOW
```

---

## Full Negotiation Sequence (32 bits)

```
    │  Sync Phase   │  Bit 0  │  Bit 1  │  Bit 2  │ ... │  Bit 31 │ Connected │
    │               │         │         │         │     │         │           │
    ├───────────────┼─────────┼─────────┼─────────┼─────┼─────────┼───────────┤
    │   50-70ms     │   7ms   │   7ms   │   7ms   │     │   7ms   │           │
    │               │         │         │         │     │         │           │
    
    Total negotiation time: ~50-70ms sync + (32 × 7ms) = ~275-295ms

    During negotiation, role can be determined at ANY bit where:
    - I send '1' AND line is LOW → I'm MASTER (stop early)
    
    If all 32 bits match → use random tie-breaker with same bit protocol
    If tie-breaker also matches → use UID sum (odd sum = master)
```

### Tie-Breaker Logic

When all 32 UID bits match (extremely unlikely with STM32 unique IDs):

1. Generate pseudo-random bit using LCG: `seed = seed * 1103515245 + 12345`
2. Exchange single tie-breaker bit using same protocol
3. If still tied, use UID byte sum: odd sum becomes master

---

## Phase 4: Connected (Command Protocol Takes Over)

```
After negotiation completes:

    ┌──────────────────────────────────────────────────────────────────┐
    │                                                                  │
    │   Presence pulses STOP - Command protocol takes over             │
    │                                                                  │
    │   Master: Sends CHECK_READY commands periodically (every 500ms)  │
    │   Slave:  Listens for commands and responds                      │
    │                                                                  │
    │   Disconnect detection:                                          │
    │   - Master: 3 consecutive command failures → NoConnection        │
    │   - Slave:  No commands received for 2 seconds → NoConnection    │
    │                                                                  │
    │   Both return to Idle state (LED0 slow blink) and wait for       │
    │   new connection via presence pulses.                            │
    │                                                                  │
    └──────────────────────────────────────────────────────────────────┘
```

---

## Timing Constants Summary

| Constant | Value | Purpose |
|----------|-------|---------|
| `DEBOUNCE_TIME_US` | 5ms | Debounce time for connection detection |
| `PRESENCE_PULSE_US` | 2ms | Presence detection pulse width |
| `PULSE_INTERVAL_US` | 50ms | Time between presence pulses |
| `SYNC_PULSE_US` | 10ms | Synchronization pulse width |
| `SYNC_WAIT_US` | 5ms | Wait time after sync operations |
| `BIT_DRIVE_US` | 5ms | How long to drive/release for each bit |
| `BIT_SAMPLE_US` | 2.5ms | When to sample within bit slot |
| `BIT_RECOVERY_US` | 2ms | Recovery time between bits |
| `NEGOTIATION_BITS` | 32 | Number of UID bits to compare |

---

## Wired-AND Truth Table

| My Bit | Peer Bit | Line State | My Conclusion |
|--------|----------|------------|---------------|
| 1 (release) | 1 (release) | HIGH | Both sent '1', continue |
| 1 (release) | 0 (drive) | **LOW** | **I'm MASTER** (my bit > peer bit) |
| 0 (drive) | 1 (release) | LOW | Can't tell, continue (peer detects master) |
| 0 (drive) | 0 (drive) | LOW | Both sent '0', continue |

---

## Why Sync Matters

```
WITHOUT proper sync (both boards start at different times):

Board A: [sample]─────────────────────────
Board B: ─────────────────[sample]────────
                    ↑
         A samples when B hasn't driven yet!
         A sees HIGH, thinks both sent '1' → WRONG!

WITH proper sync (both boards aligned):

Board A: ──────────[sample]───────────────
Board B: ──────────[sample]───────────────
                    ↑
         Both sample during overlap period
         Correct wired-AND result!
```

### Sync Error Tolerance

The 5ms drive period with 2.5ms sample point provides tolerance for sync errors:

```
    │◄──────── 5ms DRIVE ────────►│
    │                              │
    │      ▼ 2.5ms sample          │
    │◄ 2.5ms ►◄───── 2.5ms ───────►│
    │         │                    │
    ├─────────┤ Safe overlap zone  │
    │ ±2ms sync error tolerance    │
```

Even with ~2ms sync error, both boards are driving when sampling occurs.

---

---

## Phase 5: Command Protocol (Connected State)

After negotiation, the master controls all communication. The slave only transmits when commanded.

### Command Transaction Flow

```
MASTER-INITIATED COMMAND/RESPONSE (Basic - 1 byte each):

    │◄─── 5ms ───►│◄─2ms►│◄────── 56ms (8 bits) ───────►│◄─2ms►│◄────── 56ms (8 bits) ───────►│
    │             │      │                              │      │                              │
    │   START     │ turn │      COMMAND BYTE            │ turn │      RESPONSE BYTE           │
    │   PULSE     │around│      (master sends)          │around│      (slave sends)           │
    │             │      │                              │      │                              │
    ┌─────────────┐      ┌──────────────────────────────┐      ┌──────────────────────────────┐
────┘             └──────┘  bit7 bit6 bit5 ... bit0     └──────┘  bit7 bit6 bit5 ... bit0     └──
    │  5ms LOW    │      │  MSB first, 7ms per bit      │      │  MSB first, 7ms per bit      │

Basic command transaction: ~5ms + 2ms + 56ms + 2ms + 56ms ≈ 121ms
```

### Byte Transmission (8 bits)

```
ONE BYTE TRANSMISSION (8 bits × 7ms = 56ms):

    │◄─ 7ms ─►│◄─ 7ms ─►│◄─ 7ms ─►│     │◄─ 7ms ─►│
    │  bit 7  │  bit 6  │  bit 5  │ ... │  bit 0  │
    │         │         │         │     │         │
    
Each bit slot:
    │◄────── 5ms DRIVE ───────►│◄── 2ms RECOVERY ──►│
    │                          │                    │
    │  '0' = drive LOW         │  release line      │
    │  '1' = release (HIGH)    │                    │
    │                          │                    │
    ├──────── sample ──────────┤                    │
    │         at 2.5ms         │                    │
```

### Command Codes

| Command | Code | Description | Payload | Response |
|---------|------|-------------|---------|----------|
| `NONE` | 0x00 | No command / invalid | - | - |
| `CHECK_READY` | 0x01 | Check if slave is ready | - | ACK/NAK |
| `REQUEST_ID` | 0x02 | Request slave's device ID | - | ACK + 12 bytes UID |
| `SEND_ID` | 0x03 | Master sending its ID to slave | 12 bytes UID | ACK/NAK |

### Response Codes

| Response | Code | Description |
|----------|------|-------------|
| `NONE` | 0x00 | No response / timeout |
| `ACK` | 0x06 | Acknowledged / Ready |
| `NAK` | 0x15 | Not acknowledged / Not ready |

---

## Phase 6: ID Exchange Protocol

After CHECK_READY succeeds, master initiates a two-command ID exchange sequence.

### Complete Connection Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        COMPLETE TAP LINK FLOW                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. DETECTION (~50ms)                                                       │
│     ├── Both send presence pulses (2ms LOW every 50ms)                      │
│     └── Either detects other's pulse → start negotiation                    │
│                                                                             │
│  2. NEGOTIATION (~275-295ms)                                                │
│     ├── Sync handshake (~50-70ms)                                           │
│     ├── 32-bit UID comparison (32 × 7ms = 224ms)                            │
│     ├── Higher UID = MASTER, Lower UID = SLAVE                              │
│     └── Both increment tap count and save to flash                          │
│                                                                             │
│  3. CONNECTED STATE - Command Protocol                                      │
│     │                                                                       │
│     │  Step 1: CHECK_READY (~121ms)                                         │
│     │  ┌─────────────────────────────────────────────────────────┐          │
│     │  │ Master ──── CHECK_READY ────────────────────────► Slave │          │
│     │  │ Master ◄─────────────── ACK ─────────────────────  Slave│          │
│     │  └─────────────────────────────────────────────────────────┘          │
│     │                                                                       │
│     │  Step 2: REQUEST_ID (~205ms)                                          │
│     │  ┌─────────────────────────────────────────────────────────┐          │
│     │  │ Master ──── REQUEST_ID ─────────────────────────► Slave │          │
│     │  │ Master ◄─────────────── ACK ─────────────────────  Slave│          │
│     │  │ Master ◄─────────────── UID (12 bytes) ──────────  Slave│          │
│     │  │         └─► Master stores slave's UID                   │          │
│     │  └─────────────────────────────────────────────────────────┘          │
│     │                                                                       │
│     │  Step 3: SEND_ID (~205ms)                                             │
│     │  ┌─────────────────────────────────────────────────────────┐          │
│     │  │ Master ──── SEND_ID ────────────────────────────► Slave │          │
│     │  │ Master ──── UID (12 bytes) ─────────────────────► Slave │          │
│     │  │                         Slave stores master's UID ◄─┘   │          │
│     │  │ Master ◄─────────────── ACK ─────────────────────  Slave│          │
│     │  └─────────────────────────────────────────────────────────┘          │
│     │                                                                       │
│     │  Step 4: MAINTENANCE (ongoing, every 500ms)                           │
│     │  ┌─────────────────────────────────────────────────────────┐          │
│     │  │ Master ──── CHECK_READY ────────────────────────► Slave │          │
│     │  │ Master ◄─────────────── ACK ─────────────────────  Slave│          │
│     │  └─────────────────────────────────────────────────────────┘          │
│     │                                                                       │
│  4. DISCONNECT DETECTION                                                    │
│     ├── Master: 3 consecutive invalid responses → NoConnection              │
│     └── Slave:  No commands for 2 seconds → NoConnection                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### REQUEST_ID Command Detail

```
Master requests slave's UID:

Master                                         Slave
   │                                              │
   ├── START pulse (5ms) ────────────────────────►│
   ├── Turnaround (2ms) ─────────────────────────►│
   ├── REQUEST_ID byte (0x02, 56ms) ─────────────►│
   │                                              │
   │◄──────────────────── Turnaround (2ms) ───────┤
   │◄──────────────────── ACK byte (0x06, 56ms) ──┤
   │◄──────────────────── UID byte 0 (56ms) ──────┤
   │◄──────────────────── UID byte 1 (56ms) ──────┤
   │◄──────────────────── ... ────────────────────┤
   │◄──────────────────── UID byte 11 (56ms) ─────┤
   │                                              │
   ▼                                              ▼
Master has slave's UID               Slave sent its UID

Total: 5ms + 2ms + 56ms + 2ms + 56ms + (12 × 56ms) = ~793ms
Simplified: ~205ms (with optimized byte timing)
```

### SEND_ID Command Detail

```
Master sends its UID to slave:

Master                                         Slave
   │                                              │
   ├── START pulse (5ms) ────────────────────────►│
   ├── Turnaround (2ms) ─────────────────────────►│
   ├── SEND_ID byte (0x03, 56ms) ────────────────►│
   ├── UID byte 0 (56ms) ────────────────────────►│
   ├── UID byte 1 (56ms) ────────────────────────►│
   ├── ... ──────────────────────────────────────►│
   ├── UID byte 11 (56ms) ───────────────────────►│
   │                                              │
   │◄──────────────────── Turnaround (2ms) ───────┤
   │◄──────────────────── ACK byte (0x06, 56ms) ──┤
   │                                              │
   ▼                                              ▼
Master sent its UID              Slave has master's UID

Total: ~205ms (with optimized byte timing)
```

### ID Exchange Timing Summary

| Phase | Duration |
|-------|----------|
| CHECK_READY | ~121ms |
| REQUEST_ID + 12 byte response | ~205ms |
| SEND_ID + 12 byte payload | ~205ms |
| **Total ID Exchange** | **~531ms** |
| Storage save (optimized) | ~40-80ms |

### State Tracking

```cpp
// Master tracks:
_peerReady           // true after CHECK_READY gets ACK
_idExchangeComplete  // true after both REQUEST_ID and SEND_ID succeed

// Slave tracks:
_idExchangeComplete  // true after receiving SEND_ID with master's UID
```

---

## Command Timing Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `CMD_START_PULSE_US` | 5ms | START pulse (longer than presence) |
| `CMD_TURNAROUND_US` | 2ms | Turnaround between send/receive |
| `CMD_TIMEOUT_US` | 100ms | Command timeout |
| `CMD_BIT_DRIVE_US` | 5ms | Bit drive period |
| `CMD_BIT_SAMPLE_US` | 2.5ms | Bit sample point |
| `CMD_BIT_RECOVERY_US` | 2ms | Recovery between bits |
| `MAX_COMMAND_FAILURES` | 3 | Master disconnects after N failures |
| `SLAVE_IDLE_TIMEOUT_US` | 2s | Slave disconnects if no commands |
| `COMMAND_INTERVAL_MS` | 500ms | Time between master commands |

---

## Distinguishing Commands from Presence Pulses

```
PRESENCE PULSE (2ms) vs START PULSE (5ms):

Presence (ignore):          Command START (process):
    ┌──┐                        ┌─────┐
────┘  └────                ────┘     └────
    2ms                         5ms
    < 3ms threshold             >= 3ms threshold
```

The slave measures the LOW pulse duration:
- If < 3ms: It's a presence pulse, ignore
- If >= 3ms: It's a command START, process incoming command

---

## Disconnect Detection

### Master Side
- Sends commands every 500ms
- Counts consecutive failures (invalid response or 0xFF)
- After 3 failures (~1.5 seconds): returns to NoConnection state

### Slave Side
- Tracks time since last valid command received
- After 2 seconds with no commands: returns to NoConnection state

### Recovery
Both devices return to Idle state (LED0 slow blink) and resume sending presence pulses to detect new connections.

---

*Last Updated: January 2026*
