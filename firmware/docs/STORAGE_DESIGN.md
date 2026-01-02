# Storage Design

## Overview

The storage module provides persistent data storage using the STM32's internal EEPROM emulation. It handles device identity, tap counts, peer links, and secret keys with CRC32 integrity checking.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Application                             │
├─────────────────────────────────────────────────────────────┤
│                    IStorage Interface                        │
├─────────────────────────────────────────────────────────────┤
│                   Storage Implementation                     │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ CRC32 Check │  │ Dirty Flag   │  │ Delayed Write      │  │
│  └─────────────┘  └──────────────┘  └────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                  Platform Storage HAL                        │
│              (platform_storage_arduino.cpp)                  │
├─────────────────────────────────────────────────────────────┤
│                   STM32 EEPROM Library                       │
└─────────────────────────────────────────────────────────────┘
```

## Data Layout

### PersistImageV1 Structure (896 bytes total)

```
Offset  Size    Field
------  ----    -----
0       4       magic (0x424F4B41 = "BOKA")
4       2       version (1)
6       2       length (sizeof payload)
8       4       crc32 (over payload only)
12      12      selfId (device UID)
24      4       totalTapCount
28      2       linkCount
30      1       keyVersion (0 = not provisioned)
31      1       reserved8
32      768     links[64] (12 bytes each)
800     32      secretKey
832     64      reserved32[16]
```

### CRC32 Calculation

- Uses STM32 hardware CRC peripheral for speed
- CRC calculated over payload only (not header)
- Payload must be 4-byte aligned for hardware CRC

## Flash Write Cycle Management

### The Problem

STM32L0 EEPROM emulation uses flash memory with limited write cycles:
- ~10,000 erase/write cycles per page
- Full 896-byte write takes 6-7 seconds (blocking)
- Each byte write can take 5-10ms

### Solutions Implemented

#### 1. Delayed Write (2 seconds)

```cpp
constexpr uint32_t STORAGE_DELAYED_WRITE_MS = 2000;
```

Multiple changes within 2 seconds are batched into a single write. The `loop()` function checks the dirty flag and elapsed time.

#### 2. Optimized Partial Saves

**saveTapCountOnly()** - 8 bytes instead of 896:
- Writes: totalTapCount (4 bytes) + crc32 (4 bytes)
- ~100x faster than full save

**saveLinkOnly()** - 18 bytes instead of 896:
- Writes: linkCount (2 bytes) + link entry (12 bytes) + crc32 (4 bytes)
- ~50x faster than full save

#### 3. Chunked Writes with Yield

Full writes are chunked with periodic 1ms delays to allow serial interrupts to be processed:

```cpp
const size_t CHUNK_SIZE = 32;
for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
    platform_storage_write(...);
    if ((i % CHUNK_SIZE) == (CHUNK_SIZE - 1)) {
        platform_delay_ms(1);
    }
}
```

## Link Management

### Duplicate Prevention

`hasLink()` scans existing links before adding:
- O(n) scan where n = linkCount
- Returns early on match

### Circular Buffer Wrap

When MAX_LINKS (64) is reached:
```cpp
idx = idx % PersistPayloadV1::MAX_LINKS;
```
Oldest links are overwritten. linkCount continues incrementing to track total unique peers.

## Secret Key Storage

- 32-byte key stored in `secretKey` field
- `keyVersion` indicates provisioning state (0 = not set)
- Immediate save on `setSecretKey()` (no delay for security-critical data)

## Initialization Flow

```
begin()
  ├── platform_storage_begin(STORAGE_EEPROM_SIZE)
  ├── loadFromNvm()
  │   ├── Read entire image
  │   ├── Validate magic, version, length
  │   ├── Verify CRC32
  │   └── Return success/failure
  ├── If load failed:
  │   ├── Zero-initialize payload
  │   ├── Set magic, version, length
  │   ├── Copy hardware UID to selfId
  │   ├── Calculate CRC32
  │   └── writeToNvm()
  └── If selfId is all zeros:
      ├── Copy hardware UID to selfId
      └── saveNow()
```

## Interface Methods

| Method | Description |
|--------|-------------|
| `begin()` | Initialize and load from NVM |
| `loop()` | Check for delayed save timeout |
| `saveNow()` | Force immediate full save |
| `markDirty()` | Flag data as modified |
| `clearAll()` | Reset links and counts (keeps selfId) |
| `addLink()` | Add peer ID if not duplicate |
| `hasLink()` | Check if peer exists |
| `incrementTapCount()` | Increment tap counter |
| `saveTapCountOnly()` | Optimized save for tap count |
| `saveLinkOnly()` | Optimized save for new link |
| `setSecretKey()` | Store provisioned key (immediate save) |

## Testing

MockStorage in `test/mock_storage.h` implements IStorage for unit testing without hardware. It tracks method calls for test assertions.

