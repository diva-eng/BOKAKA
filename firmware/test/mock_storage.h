#pragma once
// =====================================================
// Mock Storage for Unit Testing
// =====================================================
// Implements IStorage interface without real hardware.
// Use this in unit tests to verify command handler logic,
// storage operations, etc. without needing actual flash.
// =====================================================

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// =====================================================
// Minimal Definitions for Native Testing
// =====================================================
// When testing on native platform, we don't have the full
// firmware headers. Define minimal versions here.

#ifndef DEVICE_UID_LEN
#define DEVICE_UID_LEN 12
#endif

// Minimal PersistPayloadV1 for testing
struct LinkRecordV1 {
    uint8_t peerId[DEVICE_UID_LEN];
};

struct PersistPayloadV1 {
    uint8_t selfId[DEVICE_UID_LEN];
    uint32_t totalTapCount;
    uint16_t linkCount;
    uint8_t keyVersion;
    uint8_t reserved8;
    
    static const size_t MAX_LINKS = 64;
    LinkRecordV1 links[MAX_LINKS];
    
    uint8_t secretKey[32];
    uint32_t reserved32[16];
};

// Minimal IStorage interface for testing
class IStorage {
public:
    virtual ~IStorage() = default;
    virtual bool begin() = 0;
    virtual void loop() = 0;
    virtual bool saveNow() = 0;
    virtual void markDirty() = 0;
    virtual PersistPayloadV1& state() = 0;
    virtual bool hasSecretKey() const = 0;
    virtual const uint8_t* getSecretKey() const = 0;
    virtual uint8_t getKeyVersion() const = 0;
    virtual void setSecretKey(uint8_t version, const uint8_t key[32]) = 0;
    virtual void clearAll() = 0;
    virtual bool addLink(const uint8_t peerId[DEVICE_UID_LEN]) = 0;
    virtual bool hasLink(const uint8_t peerId[DEVICE_UID_LEN]) const = 0;
    virtual void incrementTapCount() = 0;
    virtual void saveTapCountOnly() = 0;
    virtual void saveLinkOnly() = 0;
};

// =====================================================
// MockStorage Implementation
// =====================================================

class MockStorage : public IStorage {
public:
    MockStorage() {
        reset();
    }

    // Reset all state and call counters
    void reset() {
        memset(&_payload, 0, sizeof(_payload));
        memset(_secretKey, 0, sizeof(_secretKey));
        _keyVersion = 0;
        _dirty = false;
        
        // Reset call counters
        beginCalled = false;
        beginResult = true;
        loopCallCount = 0;
        saveNowCallCount = 0;
        saveNowResult = true;
        clearAllCalled = false;
        incrementTapCountCalled = false;
        saveTapCountOnlyCalled = false;
        saveLinkOnlyCalled = false;
    }

    // =====================================================
    // IStorage Implementation
    // =====================================================

    bool begin() override {
        beginCalled = true;
        return beginResult;
    }

    void loop() override {
        loopCallCount++;
    }

    bool saveNow() override {
        saveNowCallCount++;
        _dirty = false;
        return saveNowResult;
    }

    void markDirty() override {
        _dirty = true;
    }

    PersistPayloadV1& state() override {
        return _payload;
    }

    bool hasSecretKey() const override {
        if (_keyVersion == 0) return false;
        for (size_t i = 0; i < 32; ++i) {
            if (_secretKey[i] != 0) return true;
        }
        return false;
    }

    const uint8_t* getSecretKey() const override {
        return _secretKey;
    }

    uint8_t getKeyVersion() const override {
        return _keyVersion;
    }

    void setSecretKey(uint8_t version, const uint8_t key[32]) override {
        _keyVersion = version;
        memcpy(_secretKey, key, 32);
        _dirty = true;
    }

    void clearAll() override {
        clearAllCalled = true;
        uint8_t selfId[DEVICE_UID_LEN];
        memcpy(selfId, _payload.selfId, DEVICE_UID_LEN);
        memset(&_payload, 0, sizeof(_payload));
        memcpy(_payload.selfId, selfId, DEVICE_UID_LEN);
        _dirty = true;
    }

    bool addLink(const uint8_t peerId[DEVICE_UID_LEN]) override {
        // Check for duplicate
        if (hasLink(peerId)) {
            return false;
        }

        uint16_t idx = _payload.linkCount;
        if (idx >= PersistPayloadV1::MAX_LINKS) {
            idx = idx % PersistPayloadV1::MAX_LINKS;
        } else {
            _payload.linkCount++;
        }

        memcpy(_payload.links[idx].peerId, peerId, DEVICE_UID_LEN);
        _dirty = true;
        return true;
    }

    bool hasLink(const uint8_t peerId[DEVICE_UID_LEN]) const override {
        uint16_t count = _payload.linkCount;
        if (count > PersistPayloadV1::MAX_LINKS) {
            count = PersistPayloadV1::MAX_LINKS;
        }

        for (uint16_t i = 0; i < count; i++) {
            if (memcmp(_payload.links[i].peerId, peerId, DEVICE_UID_LEN) == 0) {
                return true;
            }
        }
        return false;
    }

    void incrementTapCount() override {
        incrementTapCountCalled = true;
        _payload.totalTapCount++;
        _dirty = true;
    }

    void saveTapCountOnly() override {
        saveTapCountOnlyCalled = true;
        _dirty = false;
    }

    void saveLinkOnly() override {
        saveLinkOnlyCalled = true;
        _dirty = false;
    }

    // =====================================================
    // Test Helpers
    // =====================================================

    bool isDirty() const { return _dirty; }

    // Set self ID for testing
    void setSelfId(const uint8_t id[DEVICE_UID_LEN]) {
        memcpy(_payload.selfId, id, DEVICE_UID_LEN);
    }

    // =====================================================
    // Call Tracking (for test assertions)
    // =====================================================

    bool beginCalled;
    bool beginResult;
    int loopCallCount;
    int saveNowCallCount;
    bool saveNowResult;
    bool clearAllCalled;
    bool incrementTapCountCalled;
    bool saveTapCountOnlyCalled;
    bool saveLinkOnlyCalled;

private:
    PersistPayloadV1 _payload;
    uint8_t _secretKey[32];
    uint8_t _keyVersion;
    bool _dirty;
};
