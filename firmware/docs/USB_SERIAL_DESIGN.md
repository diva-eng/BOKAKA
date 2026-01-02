# USB Serial Command Handler Design

## Overview

The USB serial module provides a JSON-based command interface over USB CDC for device management, debugging, and provisioning.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Host PC (Python/etc)                      │
├─────────────────────────────────────────────────────────────┤
│                      USB CDC Serial                          │
├─────────────────────────────────────────────────────────────┤
│                  UsbCommandHandler                           │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ Line Buffer │  │ Command Parse│  │ JSON Response      │  │
│  └─────────────┘  └──────────────┘  └────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                  Platform Serial HAL                         │
│              (platform_serial_arduino.cpp)                   │
├─────────────────────────────────────────────────────────────┤
│                   Arduino Serial (USB CDC)                   │
└─────────────────────────────────────────────────────────────┘
```

## Initialization

### USB CDC Enumeration

STM32 USB CDC requires time to enumerate after power-on:

```cpp
void begin(unsigned long baud) {
    platform_serial_begin(baud);
    platform_delay_ms(500);        // Wait for USB enumeration
    
    // Flush stale data from input buffer
    uint32_t flushStart = platform_millis();
    while (platform_serial_available() > 0 && 
           (platform_millis() - flushStart < 100)) {
        platform_serial_read();
        platform_delay_ms(1);
    }
    
    platform_delay_ms(100);        // Final settling time
}
```

Without this flush, the first command can be corrupted by initialization noise.

## Command Protocol

### Line Format

- Commands are newline-terminated (`\n` or `\r\n`)
- Case-insensitive (converted to uppercase internally)
- Arguments separated by spaces

### Response Format

All responses are JSON objects with an `event` field:

```json
{"event":"hello","device_id":"...","fw":"1.0.0"}
{"event":"state","totalTapCount":42,"linkCount":5}
{"event":"error","msg":"unknown command: FOO"}
```

## Commands

### HELLO

Returns device identification and firmware info.

```
Request:  HELLO
Response: {"event":"hello","device_id":"<24-char hex>","fw":"1.0.0","build":"2026-01-03T12:00:00Z","hash":"abc123"}
```

### GET_STATE

Returns current tap statistics.

```
Request:  GET_STATE
Response: {"event":"state","totalTapCount":42,"linkCount":5}
```

### CLEAR / RESET_TAPS

Clears all links and tap count (preserves selfId and secret key).

```
Request:  CLEAR
Response: {"event":"ack","cmd":"CLEAR"}
```

Note: ACK is sent before the blocking EEPROM write to prevent serial timeout.

### DUMP [offset] [count]

Returns stored peer links with pagination.

```
Request:  DUMP 0 10
Response: {"event":"links","offset":0,"count":3,"items":[{"peer":"A1B2C3..."},{"peer":"D4E5F6..."}]}
```

Default: offset=0, count=10

### PROVISION_KEY version key_hex

Provisions a 32-byte secret key for HMAC signing.

```
Request:  PROVISION_KEY 1 <64-char hex key>
Response: {"event":"ack","cmd":"PROVISION_KEY","keyVersion":1}
```

Note: ACK sent before EEPROM write. Key is saved immediately (no delay).

### SIGN_STATE nonce_hex

Signs current state with HMAC-SHA256 for server verification.

```
Request:  SIGN_STATE <2-64 char hex nonce>
Response: {"event":"SIGNED_STATE","device_id":"...","nonce":"...","totalTapCount":42,"linkCount":5,"keyVersion":1,"hmac":"<64-char hex>"}
```

HMAC message structure:
```
selfId (12 bytes) + nonce (N bytes) + totalTapCount (4 LE) + linkCount (2 LE) + [peerId × linkCount]
```

### GET_KEY (test builds only)

Returns the stored secret key. Only available when `ENABLE_TEST_COMMANDS` is defined.

```
Request:  GET_KEY
Response: {"event":"key","keyVersion":1,"key":"<64-char hex>"}
```

## Input Processing

### Line Buffer

```cpp
static constexpr size_t CMD_BUF_SIZE = 128;
char _buf[CMD_BUF_SIZE];
size_t _len = 0;
```

Characters are accumulated until newline. Buffer overflow discards the line.

### Polling Model

```cpp
void poll(IStorage& storage) {
    while (platform_serial_available() > 0) {
        int c = platform_serial_read();
        if (c == '\r') continue;      // Ignore CR
        if (c == '\n') {
            _buf[_len] = '\0';
            handleLine(storage, _buf);
            _len = 0;
        } else {
            if (_len < CMD_BUF_SIZE - 1) {
                _buf[_len++] = c;
            } else {
                _len = 0;             // Overflow: discard
            }
        }
    }
}
```

## Utility Functions

### Hex Conversion

```cpp
bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);
void bytesToHex(const uint8_t* in, size_t len, char* out);
void printHex(const uint8_t* data, size_t len);
```

Used for key provisioning, device ID display, and HMAC output.

## Error Handling

Errors return JSON with descriptive messages:

```json
{"event":"error","msg":"invalid key hex"}
{"event":"error","msg":"no_key"}
{"event":"error","msg":"PROVISION_KEY args"}
```

## Threading Considerations

- All operations are blocking
- Long EEPROM writes (CLEAR, PROVISION_KEY) send ACK first
- Polling model works with cooperative multitasking in `loop()`

