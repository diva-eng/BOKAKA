#pragma once
#include <stdint.h>
#include <string.h>
#include "device_id.h"

// =====================================================
// Configuration
// =====================================================

constexpr uint32_t STORAGE_MAGIC   = 0x424F4B41;  // "BOKA"
constexpr uint16_t STORAGE_VERSION = 1;

constexpr size_t STORAGE_EEPROM_SIZE = 2048;
constexpr size_t STORAGE_EEPROM_BASE = 0;

// Flash write cycle management:
// - STM32L0 flash typically has ~10,000 erase/write cycles per page
// - EEPROM emulation uses flash pages and requires erase before write
// - Writing the entire structure (~896 bytes) counts as one write cycle
// - The delayed write mechanism (STORAGE_DELAYED_WRITE_MS) batches changes to minimize writes
// - Critical operations like setSecretKey() write immediately (rare, critical data)
// - Regular operations like addLink() are batched with 30s delay
//
// Lifetime estimates for typical usage (<100 links/event, multiple events/year):
// - Worst case (100 writes/event, all spread out): ~10 years at 10 events/year
// - Realistic (25 writes/event with batching): ~40 years at 10 events/year  
// - Best case (all taps batched): 1000+ years (not realistic for events)
// NOTE: These are theoretical limits. For production use, consider external flash
//       (100,000+ cycles) with encryption for better reliability and longevity.
constexpr uint32_t STORAGE_DELAYED_WRITE_MS = 30000;  // 30 seconds - batch changes to reduce flash wear


// =====================================================
// Persist Structures (Version 1)
// =====================================================

struct LinkRecordV1 {
    uint8_t peerId[DEVICE_UID_LEN];
};

// Must be 4-byte aligned to use hardware CRC
struct __attribute__((aligned(4))) PersistPayloadV1 {
    uint8_t selfId[DEVICE_UID_LEN];  // 12
    uint32_t totalTapCount;          // 4
    uint16_t linkCount;              // 2
    uint8_t  keyVersion;             // 1  (0 = key not provisioned)
    uint8_t  reserved8;              // 1  (alignment padding)

    static const size_t MAX_LINKS = 64;
    LinkRecordV1 links[MAX_LINKS];

    // Per-device secret key (written after server generates it)
    uint8_t secretKey[32];

    // Reserved space
    uint32_t reserved32[16];
};


static_assert(sizeof(PersistPayloadV1) % 4 == 0, "PersistPayloadV1 must be 4-byte aligned");


// Header + Payload (V1)
struct PersistHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t crc32;
};

struct PersistImageV1 {
    PersistHeader header;
    PersistPayloadV1 payload;
};


// =====================================================
// Storage Class
// =====================================================

class Storage {
public:
    Storage();

    bool begin();       // load or initialize
    void loop();        // for delayed save
    bool saveNow();     // force save
    void markDirty();   // mark structure modified

    PersistPayloadV1& state() { return _image.payload; }

    // per-device key
    bool hasSecretKey() const;
    const uint8_t* getSecretKey() const;              // 32 bytes
    uint8_t getKeyVersion() const;
    void setSecretKey(uint8_t version, const uint8_t key[32]);

    void clearAll();    // reset counters & links but keep selfId
    
    // Add a link if it doesn't already exist
    // Returns true if link was new and added, false if it already existed
    bool addLink(const uint8_t peerId[DEVICE_UID_LEN]);
    
    // Check if a peer ID already exists in links
    bool hasLink(const uint8_t peerId[DEVICE_UID_LEN]) const;
    
    void incrementTapCount();      // increment totalTapCount by 1
    void saveTapCountOnly();       // fast save - only writes tapCount + CRC (8 bytes vs 896)
    void saveLinkOnly();           // fast save - only writes latest link + linkCount + CRC (~18 bytes vs 896)

private:
    bool loadFromNvm();
    bool writeToNvm();
    uint32_t calcCrc32(const uint8_t* data, size_t len);

private:
    PersistImageV1 _image{};
    bool _dirty = false;
    uint32_t _lastSaveMs = 0;
    uint16_t _lastLinkIndex = 0;       // Track last modified link for optimized save
    bool _linkCountChanged = false;    // Track if linkCount was incremented
};
