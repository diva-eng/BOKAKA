#pragma once
#include <stdint.h>
#include "tap_link_hal.h"
#include "device_id.h"

// =====================================================
// Tap Link Detection and Negotiation
// =====================================================
//
// Two operation modes:
// 1. EVAL_BOARD_TEST: USB-powered continuous monitoring (PlatformIO)
// 2. BATTERY_MODE: Sleep/wake with CR2032 batteries (CubeIDE)
//
// Protocol:
// 1. Detection: Presence pulses to detect peer
// 2. Negotiation: Exchange UID bits to determine master/slave
// 3. Connected: Master (higher UID) and Slave (lower UID) roles assigned
// 4. Command Phase: Master sends commands, Slave responds
//
// Electrical principle:
// - GPIO pin configured as input with pull-up (normally HIGH)
// - When another device connects and drives the line LOW, we detect it
// - Uses debouncing to avoid false positives from noise
//

// =====================================================
// Command Protocol (for Connected state)
// =====================================================
// Master-initiated command/response protocol.
// Master sends START pulse (5ms) followed by command byte.
// Slave responds with response byte(s).
//
// Expandable command set - add new commands as needed.

enum class TapCommand : uint8_t {
    NONE = 0x00,           // No command / invalid
    CHECK_READY = 0x01,    // Check if slave is ready (response: ACK/NAK)
    REQUEST_ID = 0x02,     // Request slave to send its device ID (response: ACK + 12 bytes UID)
    SEND_ID = 0x03,        // Master sending its ID to slave (payload: 12 bytes UID, response: ACK/NAK)
    // Add more commands as needed (0x04-0xFF available)
};

enum class TapResponse : uint8_t {
    NONE = 0x00,           // No response / timeout
    ACK = 0x06,            // Acknowledged / Ready
    NAK = 0x15,            // Not acknowledged / Not ready
    // Add more responses as needed
};

class TapLink {
public:
    enum class DetectionState {
#ifdef EVAL_BOARD_TEST
        NoConnection,   // Line is high (no other device connected)
        Detecting,      // Line went low, debouncing
        Negotiating,    // Exchanging UID bits to determine master/slave
        Connected       // Connection confirmed, roles assigned
#else
        Sleeping,       // MCU asleep, waiting for tap wake-up
        Waking,         // Just woken by tap, validating connection
        Negotiating,    // Exchanging UID bits to determine master/slave
        Connected,      // Valid connection confirmed
        Disconnected    // Connection lost
#endif
    };

    TapLink(IOneWireHal* hal);

    // Main detection logic - call this in loop()
    void poll();

#ifdef EVAL_BOARD_TEST
    // Send a presence pulse to signal we're here (call periodically)
    void sendPresencePulse();
#endif

    // Get current detection state
    DetectionState getState() const { return _state; }

    // Role information (valid after negotiation complete)
    bool hasRole() const { return _roleKnown; }
    bool isMaster() const { return _isMaster; }

#ifdef EVAL_BOARD_TEST
    // Check if connection was just detected (call once per detection)
    bool isConnectionDetected();

    // Check if negotiation just completed
    bool isNegotiationComplete();

    // Reset detection state (after handling connection)
    void reset();

    // === Command Protocol (for Connected state) ===
    
    // Master: Send a command and receive response
    // Returns the response, or TapResponse::NONE on timeout/error
    TapResponse masterSendCommand(TapCommand cmd);
    
    // Slave: Check for incoming command (non-blocking)
    // Returns true if a START pulse is detected (command incoming)
    bool slaveHasCommand();
    
    // Slave: Receive the command (blocking, call after slaveHasCommand() returns true)
    // Returns the command, or TapCommand::NONE on timeout/error
    TapCommand slaveReceiveCommand();
    
    // Slave: Send response to master
    void slaveSendResponse(TapResponse response);
    
    // Check if peer is ready (set when master receives ACK from CHECK_READY)
    bool isPeerReady() const { return _peerReady; }
    
    // Clear peer ready flag (e.g., when starting new exchange)
    void clearPeerReady() { _peerReady = false; }
    
    // === ID Exchange Commands ===
    
    // Master: Request slave's UID (sends REQUEST_ID, receives ACK + 12 bytes)
    // Returns true on success, fills peerIdOut with slave's UID
    bool masterRequestId(uint8_t peerIdOut[DEVICE_UID_LEN]);
    
    // Master: Send master's UID to slave (sends SEND_ID + 12 bytes, receives ACK)
    // Returns true if slave acknowledged
    bool masterSendId();
    
    // Slave: Handle REQUEST_ID command (sends ACK + own UID)
    void slaveHandleRequestId();
    
    // Slave: Handle SEND_ID command (receives 12 bytes UID, sends ACK)
    // Returns true on success, fills peerIdOut with master's UID
    bool slaveHandleSendId(uint8_t peerIdOut[DEVICE_UID_LEN]);
    
    // Check if ID exchange has been completed for this connection
    bool isIdExchangeComplete() const { return _idExchangeComplete; }
    
    // Get own UID (for reference)
    const uint8_t* getSelfId() const { return _selfId; }
#else
    // Check if connection was just established
    bool isConnectionEstablished();

    // Check if connection was just lost
    bool isConnectionLost();

    // Configure for sleep mode (call before entering sleep)
    void prepareForSleep();

    // Handle wake-up from sleep (call after wake-up)
    void handleWakeUp();

    // Reset detection state
    void reset();
#endif

private:
#ifdef EVAL_BOARD_TEST
    // Detection timing constants (microseconds)
    static constexpr uint32_t DEBOUNCE_TIME_US = 5000;   // 5ms debounce
    static constexpr uint32_t PRESENCE_PULSE_US = 2000;  // 2ms presence pulse
    static constexpr uint32_t PULSE_INTERVAL_US = 50000; // 50ms between pulses

    // Negotiation timing constants (microseconds)
    // CRITICAL: Drive period must be MUCH longer than any sync error
    // to ensure both boards are driving when sampling occurs
    static constexpr uint32_t BIT_DRIVE_US = 5000;       // 5ms drive period (long for overlap)
    static constexpr uint32_t BIT_SAMPLE_US = 2500;      // Sample at 2.5ms (middle of drive)
    static constexpr uint32_t BIT_RECOVERY_US = 2000;    // 2ms recovery between bits
    static constexpr uint32_t SYNC_PULSE_US = 10000;     // 10ms sync pulse
    static constexpr uint32_t SYNC_WAIT_US = 5000;       // 5ms wait after sync
    static constexpr uint32_t NEGOTIATION_BITS = 32;     // First 32 bits of UID for negotiation

    // Command protocol timing constants (microseconds)
    static constexpr uint32_t CMD_START_PULSE_US = 5000;   // 5ms START pulse (longer than presence)
    static constexpr uint32_t CMD_TURNAROUND_US = 2000;    // 2ms turnaround between send/receive
    static constexpr uint32_t CMD_TIMEOUT_US = 100000;     // 100ms command timeout
    static constexpr uint32_t CMD_BIT_DRIVE_US = 5000;     // 5ms bit drive (same as negotiation)
    static constexpr uint32_t CMD_BIT_SAMPLE_US = 2500;    // Sample at 2.5ms
    static constexpr uint32_t CMD_BIT_RECOVERY_US = 2000;  // 2ms recovery between bits
    static constexpr uint8_t MAX_COMMAND_FAILURES = 3;     // Disconnect after 3 failed commands
    static constexpr uint32_t SLAVE_IDLE_TIMEOUT_US = 2000000;  // 2 seconds - slave disconnects if no command
#else
    // Detection timing constants (microseconds)
    static constexpr uint32_t VALIDATION_TIME_US = 10000;  // 10ms validation after wake-up
    static constexpr uint32_t DISCONNECT_DEBOUNCE_US = 2000; // 2ms debounce for disconnect
#endif

    // Helper functions
    uint32_t elapsedMicros(uint32_t startTime);
#ifdef EVAL_BOARD_TEST
    void startNegotiation();
    void pollNegotiation();
    bool sendBit(bool bit);
    bool readBit();
    
    // Command protocol helpers
    void sendByte(uint8_t byte);
    bool receiveByte(uint8_t* byte, uint32_t timeoutUs);
    void sendBytes(const uint8_t* data, size_t len);
    bool receiveBytes(uint8_t* data, size_t len);
    void sendStartPulse();
    bool waitForLineHigh(uint32_t timeoutUs);
#else
    bool validateConnection();  // Check if tap connection is stable
#endif

    IOneWireHal* _hal;

    DetectionState _state;
    uint32_t _stateStartTime;

    // Role negotiation
    uint8_t _selfId[DEVICE_UID_LEN];
    bool _roleKnown;
    bool _isMaster;

#ifdef EVAL_BOARD_TEST
    uint32_t _lastLineChangeTime;
    bool _lastLineState;
    bool _connectionJustDetected;
    bool _negotiationJustCompleted;
    uint32_t _lastPulseTime;       // When we last sent a presence pulse
    bool _isPulsing;               // Are we currently sending a pulse?
    uint32_t _pulseStartTime;      // When current pulse started

    // Negotiation state
    uint8_t _negotiationBitIndex;
    uint32_t _bitSlotStartTime;
    bool _waitingForSync;
    bool _syncSent;
    uint32_t _randomSeed;          // For tie-breaker
    
    // Command protocol state
    bool _peerReady;               // True when peer responded ACK to CHECK_READY
    uint32_t _lastCommandTime;     // For master: command rate limiting; for slave: last command received
    uint8_t _commandFailures;      // Count of consecutive command failures (master only)
    bool _idExchangeComplete;      // True when both REQUEST_ID and SEND_ID have succeeded
#else
    uint32_t _lastWakeTime;
    bool _connectionJustEstablished;
    bool _connectionJustLost;
    bool _wasConnected;
#endif
};
