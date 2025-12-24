#pragma once
#include <stdint.h>
#include "tap_link_hal.h"
#include "device_id.h"
#include "storage.h"

// =====================================================
// 1-Wire Tap Link Protocol
// =====================================================
//
// Protocol Overview:
// 1. Connection Detection: Line goes low when devices are connected
// 2. Master/Slave Negotiation: Both devices send their device ID simultaneously
//    - Device with higher ID becomes master
// 3. ID Exchange:
//    - Master sends its full device ID
//    - Master waits for ACK from slave
//    - Slave sends its full device ID
// 4. Storage: Both devices store peer UID and increment connection counter
//
// Timing (all times in microseconds):
// - Bit time: 1000us (1ms) per bit
// - Start pulse: 2000us (2ms) low
// - ACK pulse: 1500us (1.5ms) low
// - Timeout: 50000us (50ms) for any operation
//

class TapLink {
public:
    enum class State {
        Idle,           // Waiting for connection
        Detecting,      // Detected connection, starting negotiation
        Negotiating,    // Sending device ID for master/slave determination
        MasterWaitAck,  // Master: waiting for slave ACK after sending ID
        MasterDone,     // Master: exchange complete
        SlaveReceive,   // Slave: receiving master's ID
        SlaveSend,      // Slave: sending own ID
        SlaveDone,      // Slave: exchange complete
        Error           // Error state
    };

    TapLink(IOneWireHal* hal, Storage* storage);
    
    // Main state machine - call this in loop()
    void poll();
    
    // Get current state
    State getState() const { return _state; }
    
    // Role information
    bool hasRole() const { return _roleKnown; }
    bool isMaster() const { return _isMaster; }
    
    // Check if a connection was just completed
    bool isConnectionComplete() const;
    
    // Reset state machine (call after handling completed connection)
    void reset();

private:
    // Protocol timing constants (microseconds)
    static constexpr uint32_t BIT_TIME_US = 1000;        // 1ms per bit
    static constexpr uint32_t START_PULSE_US = 2000;     // 2ms start pulse
    static constexpr uint32_t ACK_PULSE_US = 1500;       // 1.5ms ACK pulse
    static constexpr uint32_t TIMEOUT_US = 50000;        // 50ms timeout
    
    // Connection detection
    static constexpr uint32_t CONNECTION_DEBOUNCE_US = 5000;  // 5ms debounce
    
    // State machine handlers
    void handleIdle();
    void handleDetecting();
    void handleNegotiating();
    void handleMasterWaitAck();
    void handleMasterDone();
    void handleSlaveReceive();
    void handleSlaveSend();
    void handleSlaveDone();
    
    // Low-level protocol functions
    bool sendByte(uint8_t byte);
    bool receiveByte(uint8_t& byte);
    bool sendStartPulse();
    bool waitForStartPulse();
    bool sendAck();
    bool waitForAck();
    bool sendBit(bool bit);
    bool receiveBit(bool& bit);
    
    // Helper functions
    int compareDeviceIds(const uint8_t id1[DEVICE_UID_LEN], 
                         const uint8_t id2[DEVICE_UID_LEN]);
    bool waitForLine(bool expectedState, uint32_t timeoutUs);
    uint32_t elapsedMicros(uint32_t startTime);
    
    IOneWireHal* _hal;
    Storage* _storage;
    
    State _state;
    uint32_t _stateStartTime;
    uint32_t _lastLineChangeTime;
    bool _lastLineState;
    
    // Negotiation state
    uint8_t _selfId[DEVICE_UID_LEN];
    uint8_t _peerId[DEVICE_UID_LEN];
    uint8_t _negotiationBitIndex;
    bool _roleKnown;
    bool _isMaster;
    
    // Reception state
    uint8_t _rxBuffer[DEVICE_UID_LEN];
    uint8_t _rxByteIndex;
    uint8_t _rxBitIndex;
    
    // Transmission state
    uint8_t _txBuffer[DEVICE_UID_LEN];
    uint8_t _txByteIndex;
    uint8_t _txBitIndex;
    
    bool _connectionComplete;
};
