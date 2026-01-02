#pragma once
// =====================================================
// Storage Interface
// =====================================================
// Abstract interface for storage operations.
// Enables mocking storage for unit tests without
// requiring real EEPROM/flash hardware.
// =====================================================

#include <stdint.h>
#include <stddef.h>
#include "device_id.h"

// Forward declaration of payload structure
struct PersistPayloadV1;

class IStorage {
public:
    virtual ~IStorage() = default;

    // =====================================================
    // Lifecycle
    // =====================================================
    
    // Initialize storage (load from NVM or create default)
    virtual bool begin() = 0;
    
    // Called from main loop for delayed write handling
    virtual void loop() = 0;
    
    // Force immediate save to NVM
    virtual bool saveNow() = 0;
    
    // Mark data as modified (triggers delayed save)
    virtual void markDirty() = 0;

    // =====================================================
    // State Access
    // =====================================================
    
    // Get reference to persistent state
    virtual PersistPayloadV1& state() = 0;

    // =====================================================
    // Secret Key Management
    // =====================================================
    
    virtual bool hasSecretKey() const = 0;
    virtual const uint8_t* getSecretKey() const = 0;
    virtual uint8_t getKeyVersion() const = 0;
    virtual void setSecretKey(uint8_t version, const uint8_t key[32]) = 0;

    // =====================================================
    // Link Management
    // =====================================================
    
    // Clear all links and tap count (keeps selfId)
    virtual void clearAll() = 0;
    
    // Add a new link if it doesn't already exist
    // Returns true if link was new and added
    virtual bool addLink(const uint8_t peerId[DEVICE_UID_LEN]) = 0;
    
    // Check if a peer ID already exists
    virtual bool hasLink(const uint8_t peerId[DEVICE_UID_LEN]) const = 0;
    
    // Increment tap count
    virtual void incrementTapCount() = 0;
    
    // Optimized partial saves
    virtual void saveTapCountOnly() = 0;
    virtual void saveLinkOnly() = 0;
};

