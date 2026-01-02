#pragma once
// =====================================================
// TapLink Interface
// =====================================================
// Abstract interface for tap link detection.
// Enables different implementations for eval board vs battery mode.
//
// Implementations:
// - TapLinkEval: USB-powered continuous monitoring
// - TapLinkBattery: Sleep/wake with CR2032 batteries
// =====================================================

#include <stdint.h>
#include "tap_link_hal.h"
#include "device_id.h"

// =====================================================
// Common Enums
// =====================================================

enum class TapCommand : uint8_t {
    NONE = 0x00,
    CHECK_READY = 0x01,
    REQUEST_ID = 0x02,
    SEND_ID = 0x03,
};

enum class TapResponse : uint8_t {
    NONE = 0x00,
    ACK = 0x06,
    NAK = 0x15,
};

// =====================================================
// TapLink Interface
// =====================================================

class ITapLink {
public:
    virtual ~ITapLink() = default;

    // =====================================================
    // Core API (both modes)
    // =====================================================

    // Main detection logic - call this in loop()
    virtual void poll() = 0;

    // Role information (valid after negotiation complete)
    virtual bool hasRole() const = 0;
    virtual bool isMaster() const = 0;

    // Get own UID
    virtual const uint8_t* getSelfId() const = 0;

    // Reset detection state
    virtual void reset() = 0;

    // =====================================================
    // State Query (mode-specific states, but common queries)
    // =====================================================

    // Is there an active connection?
    virtual bool isConnected() const = 0;

    // Is negotiation in progress?
    virtual bool isNegotiating() const = 0;

    // Is the device idle/sleeping?
    virtual bool isIdle() const = 0;
};

// =====================================================
// Eval Board Mode Interface
// =====================================================
// Extended interface for USB-powered eval board testing

class ITapLinkEval : public ITapLink {
public:
    // Send presence pulse (call periodically when not connected)
    virtual void sendPresencePulse() = 0;

    // One-shot event queries (return true once per event)
    virtual bool isConnectionDetected() = 0;
    virtual bool isNegotiationComplete() = 0;

    // Command protocol (master)
    virtual TapResponse masterSendCommand(TapCommand cmd) = 0;
    virtual bool masterRequestId(uint8_t peerIdOut[DEVICE_UID_LEN]) = 0;
    virtual bool masterSendId() = 0;

    // Command protocol (slave)
    virtual bool slaveHasCommand() = 0;
    virtual TapCommand slaveReceiveCommand() = 0;
    virtual void slaveSendResponse(TapResponse response) = 0;
    virtual void slaveHandleRequestId() = 0;
    virtual bool slaveHandleSendId(uint8_t peerIdOut[DEVICE_UID_LEN]) = 0;

    // Peer state
    virtual bool isPeerReady() const = 0;
    virtual void clearPeerReady() = 0;
    virtual bool isIdExchangeComplete() const = 0;
};

// =====================================================
// Battery Mode Interface
// =====================================================
// Extended interface for CR2032 battery-powered devices

class ITapLinkBattery : public ITapLink {
public:
    // One-shot event queries
    virtual bool isConnectionEstablished() = 0;
    virtual bool isConnectionLost() = 0;

    // Sleep/wake management
    virtual void prepareForSleep() = 0;
    virtual void handleWakeUp() = 0;
};

// =====================================================
// Factory Functions
// =====================================================

// Create the appropriate TapLink for current build mode
ITapLink* createTapLink(IOneWireHal* hal);

// Create specific implementations (for testing)
ITapLinkEval* createTapLinkEval(IOneWireHal* hal);
ITapLinkBattery* createTapLinkBattery(IOneWireHal* hal);

