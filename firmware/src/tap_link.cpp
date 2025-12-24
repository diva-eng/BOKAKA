#include "tap_link.h"
#include <string.h>

TapLink::TapLink(IOneWireHal* hal, Storage* storage)
    : _hal(hal)
    , _storage(storage)
    , _state(State::Idle)
    , _stateStartTime(0)
    , _lastLineChangeTime(0)
    , _lastLineState(true)
    , _negotiationBitIndex(0)
    , _roleKnown(false)
    , _isMaster(false)
    , _rxByteIndex(0)
    , _rxBitIndex(0)
    , _txByteIndex(0)
    , _txBitIndex(0)
    , _connectionComplete(false)
{
    getDeviceUidRaw(_selfId);
    memset(_peerId, 0, DEVICE_UID_LEN);
    memset(_rxBuffer, 0, DEVICE_UID_LEN);
    memset(_txBuffer, 0, DEVICE_UID_LEN);
}

void TapLink::poll() {
    uint32_t now = _hal->micros();
    bool lineState = _hal->readLine();
    
    // Detect line state changes for connection detection
    if (lineState != _lastLineState) {
        _lastLineChangeTime = now;
        _lastLineState = lineState;
    }
    
    // State machine
    switch (_state) {
        case State::Idle:
            handleIdle();
            break;
        case State::Detecting:
            handleDetecting();
            break;
        case State::Negotiating:
            handleNegotiating();
            break;
        case State::MasterWaitAck:
            handleMasterWaitAck();
            break;
        case State::MasterDone:
            handleMasterDone();
            break;
        case State::SlaveReceive:
            handleSlaveReceive();
            break;
        case State::SlaveSend:
            handleSlaveSend();
            break;
        case State::SlaveDone:
            handleSlaveDone();
            break;
        case State::Error:
            // Stay in error state until reset
            break;
    }
}

void TapLink::handleIdle() {
    // Check if line is low (connection detected)
    if (!_hal->readLine()) {
        uint32_t now = _hal->micros();
        uint32_t elapsed = elapsedMicros(_lastLineChangeTime);
        
        // Debounce: line must be low for at least CONNECTION_DEBOUNCE_US
        if (elapsed >= CONNECTION_DEBOUNCE_US) {
            _state = State::Detecting;
            _stateStartTime = now;
            _negotiationBitIndex = 0;
            _roleKnown = false;
            _isMaster = false;
        }
    }
}

void TapLink::handleDetecting() {
    // Wait a bit to ensure connection is stable, then start negotiation
    uint32_t elapsed = elapsedMicros(_stateStartTime);
    if (elapsed >= 10000) {  // 10ms delay before starting
        _state = State::Negotiating;
        _stateStartTime = _hal->micros();
        _negotiationBitIndex = 0;
        
        // Prepare to send our ID bit by bit
        memcpy(_txBuffer, _selfId, DEVICE_UID_LEN);
        _txByteIndex = 0;
        _txBitIndex = 0;
    }
}

void TapLink::handleNegotiating() {
    uint32_t now = _hal->micros();
    uint32_t elapsed = elapsedMicros(_stateStartTime);
    
    // Timeout check
    if (elapsed > TIMEOUT_US) {
        _state = State::Error;
        return;
    }
    
    // Send and receive one bit at a time for negotiation
    // Both devices send simultaneously on 1-wire (open-drain)
    // On 1-wire: line is high only if BOTH devices release (send 1)
    //            line is low if EITHER device drives low (sends 0)
    // 
    // Negotiation strategy:
    // - Both devices send their ID bit-by-bit simultaneously
    // - Device with higher ID will first encounter a bit where it sends 1 and peer sends 0
    // - That device detects line is low (because peer drove it) and becomes master
    // - Device with lower ID will continue until it times out or receives master's start pulse
    // - We negotiate for up to 32 bits (first 4 bytes) to keep it fast
    constexpr uint8_t NEGOTIATION_BITS = 32;  // Negotiate first 4 bytes only
    
    if (_negotiationBitIndex < NEGOTIATION_BITS) {
        uint8_t byteIdx = _negotiationBitIndex / 8;
        uint8_t bitIdx = _negotiationBitIndex % 8;
        bool myBit = (_selfId[byteIdx] >> (7 - bitIdx)) & 1;
        
        // Send our bit
        if (myBit) {
            // Send '1': release line (high)
            _hal->driveLow(false);
        } else {
            // Send '0': pull line low
            _hal->driveLow(true);
        }
        
        // Wait for bit time
        _hal->delayMicros(BIT_TIME_US / 2);
        
        // Read the line state (wired-AND result)
        bool lineState = _hal->readLine();
        
        // Complete the bit time
        _hal->delayMicros(BIT_TIME_US / 2);
        
        // Release line if we were driving it
        if (!myBit) {
            _hal->driveLow(false);
        }
        
        // Determine master/slave based on bit comparison
        // On 1-wire (open-drain): line is high only if BOTH send 1
        //                        line is low if EITHER sends 0
        if (myBit && !lineState) {
            // I sent 1 but line is low -> peer sent 0 -> I'm master (higher ID)
            _isMaster = true;
            _roleKnown = true;
            _negotiationBitIndex = NEGOTIATION_BITS;  // Done negotiating
        } else if (myBit && lineState) {
            // Both sent 1 -> bits match, continue
            _negotiationBitIndex++;
        } else if (!myBit) {
            // I sent 0 -> line is always low, can't tell what peer sent
            // But if peer sent 1, peer will detect it's master
            // Continue negotiating
            _negotiationBitIndex++;
        }
    }
    
    // Negotiation complete (after NEGOTIATION_BITS or early exit if master determined)
    if (_negotiationBitIndex >= NEGOTIATION_BITS) {
        // If we completed all negotiation bits without determining master,
        // it means IDs matched for first 32 bits (very unlikely with STM32 UIDs)
        // In this case, wait a bit to see if peer (who might be master) sends start pulse
        // If no start pulse after timeout, assume we're master as tie-breaker
        if (!_isMaster) {
            // Wait a short time to see if master sends start pulse
            uint32_t waitStart = _hal->micros();
            while (elapsedMicros(waitStart) < 10000) {  // Wait up to 10ms
                if (!_hal->readLine()) {
                    // Line went low, might be start pulse from master
                    _state = State::SlaveReceive;
                    _stateStartTime = _hal->micros();
                    _rxByteIndex = 0;
                    _rxBitIndex = 0;
                    return;
                }
            }
            // No start pulse received, assume we're master (tie-breaker)
            _isMaster = true;
            _roleKnown = true;
        }
        
        if (_isMaster) {
            // Master: send start pulse, then send our ID
            if (sendStartPulse()) {
                memcpy(_txBuffer, _selfId, DEVICE_UID_LEN);
                _txByteIndex = 0;
                _txBitIndex = 0;
                _state = State::MasterWaitAck;
                _stateStartTime = now;
            } else {
                _state = State::Error;
            }
        } else {
            // Slave: wait for start pulse, then receive master's ID
            _state = State::SlaveReceive;
            _roleKnown = true;
            _stateStartTime = now;
            _rxByteIndex = 0;
            _rxBitIndex = 0;
        }
    }
}

void TapLink::handleMasterWaitAck() {
    // Send our device ID byte by byte
    if (_txByteIndex < DEVICE_UID_LEN) {
        if (!sendByte(_txBuffer[_txByteIndex])) {
            _state = State::Error;
            return;
        }
        _txByteIndex++;
        _stateStartTime = _hal->micros();  // Reset timeout for next byte
    } else {
        // All bytes sent, wait for ACK
        if (waitForAck()) {
            // ACK received, now wait for slave's ID
            _rxByteIndex = 0;
            _rxBitIndex = 0;
            _state = State::MasterDone;
            _stateStartTime = _hal->micros();
        } else {
            uint32_t elapsed = elapsedMicros(_stateStartTime);
            if (elapsed > TIMEOUT_US) {
                _state = State::Error;
            }
        }
    }
}

void TapLink::handleMasterDone() {
    // Master is done sending, wait for slave to send its ID
    if (_rxByteIndex < DEVICE_UID_LEN) {
        uint8_t byte;
        if (receiveByte(byte)) {
            _rxBuffer[_rxByteIndex] = byte;
            _rxByteIndex++;
            _stateStartTime = _hal->micros();  // Reset timeout for next byte
        } else {
            uint32_t elapsed = elapsedMicros(_stateStartTime);
            if (elapsed > TIMEOUT_US) {
                _state = State::Error;
            }
        }
    } else {
        // Received slave's ID
        memcpy(_peerId, _rxBuffer, DEVICE_UID_LEN);
        
        // Store peer ID and increment counter
        _storage->addLink(_peerId);
        
        _connectionComplete = true;
        _state = State::Idle;  // Return to idle
    }
}

void TapLink::handleSlaveReceive() {
    // First wait for start pulse from master
    if (_rxByteIndex == 0 && _rxBitIndex == 0) {
        if (!waitForStartPulse()) {
            uint32_t elapsed = elapsedMicros(_stateStartTime);
            if (elapsed > TIMEOUT_US) {
                _state = State::Error;
            }
            return;
        }
        // Start pulse received, now receive bytes
        _stateStartTime = _hal->micros();
    }
    
    // Receive master's device ID
    if (_rxByteIndex < DEVICE_UID_LEN) {
        uint8_t byte;
        if (receiveByte(byte)) {
            _rxBuffer[_rxByteIndex] = byte;
            _rxByteIndex++;
            _stateStartTime = _hal->micros();  // Reset timeout for next byte
        } else {
            uint32_t elapsed = elapsedMicros(_stateStartTime);
            if (elapsed > TIMEOUT_US) {
                _state = State::Error;
            }
        }
    } else {
        // Received master's ID, send ACK
        memcpy(_peerId, _rxBuffer, DEVICE_UID_LEN);
        
        if (sendAck()) {
            // Now send our own ID
            memcpy(_txBuffer, _selfId, DEVICE_UID_LEN);
            _txByteIndex = 0;
            _txBitIndex = 0;
            _state = State::SlaveSend;
            _stateStartTime = _hal->micros();
        } else {
            _state = State::Error;
        }
    }
}

void TapLink::handleSlaveSend() {
    // Send our device ID byte by byte
    if (_txByteIndex < DEVICE_UID_LEN) {
        if (!sendByte(_txBuffer[_txByteIndex])) {
            _state = State::Error;
            return;
        }
        _txByteIndex++;
        _stateStartTime = _hal->micros();  // Reset timeout for next byte
    } else {
        // All bytes sent
        _state = State::SlaveDone;
        _stateStartTime = _hal->micros();
    }
}

void TapLink::handleSlaveDone() {
    // Store peer ID and increment counter
    _storage->addLink(_peerId);
    
    _connectionComplete = true;
    _state = State::Idle;  // Return to idle
}

// Low-level protocol functions

bool TapLink::sendBit(bool bit) {
    if (bit) {
        // Send '1': release line (high)
        _hal->driveLow(false);
        _hal->delayMicros(BIT_TIME_US);
    } else {
        // Send '0': pull line low
        _hal->driveLow(true);
        _hal->delayMicros(BIT_TIME_US);
        _hal->driveLow(false);
    }
    return true;
}

bool TapLink::receiveBit(bool& bit) {
    // Wait for bit time, then read line
    _hal->delayMicros(BIT_TIME_US / 2);
    bit = _hal->readLine();
    _hal->delayMicros(BIT_TIME_US / 2);
    return true;
}

bool TapLink::sendByte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        bool bit = (byte >> i) & 1;
        if (!sendBit(bit)) {
            return false;
        }
    }
    return true;
}

bool TapLink::receiveByte(uint8_t& byte) {
    byte = 0;
    for (int i = 7; i >= 0; i--) {
        bool bit;
        if (!receiveBit(bit)) {
            return false;
        }
        if (bit) {
            byte |= (1 << i);
        }
    }
    return true;
}

bool TapLink::sendStartPulse() {
    _hal->driveLow(true);
    _hal->delayMicros(START_PULSE_US);
    _hal->driveLow(false);
    _hal->delayMicros(BIT_TIME_US);  // Recovery time
    return true;
}

bool TapLink::waitForStartPulse() {
    uint32_t startTime = _hal->micros();
    while (_hal->readLine()) {
        if (elapsedMicros(startTime) > TIMEOUT_US) {
            return false;
        }
    }
    
    // Line went low, measure pulse duration
    uint32_t pulseStart = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(pulseStart) > TIMEOUT_US) {
            return false;
        }
    }
    
    uint32_t pulseDuration = elapsedMicros(pulseStart);
    // Check if it's approximately a start pulse (allow some tolerance)
    return (pulseDuration >= (START_PULSE_US - 500)) && 
           (pulseDuration <= (START_PULSE_US + 500));
}

bool TapLink::sendAck() {
    _hal->driveLow(true);
    _hal->delayMicros(ACK_PULSE_US);
    _hal->driveLow(false);
    _hal->delayMicros(BIT_TIME_US);  // Recovery time
    return true;
}

bool TapLink::waitForAck() {
    uint32_t startTime = _hal->micros();
    while (_hal->readLine()) {
        if (elapsedMicros(startTime) > TIMEOUT_US) {
            return false;
        }
    }
    
    // Line went low, measure pulse duration
    uint32_t pulseStart = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(pulseStart) > TIMEOUT_US) {
            return false;
        }
    }
    
    uint32_t pulseDuration = elapsedMicros(pulseStart);
    // Check if it's approximately an ACK pulse
    return (pulseDuration >= (ACK_PULSE_US - 500)) && 
           (pulseDuration <= (ACK_PULSE_US + 500));
}

bool TapLink::waitForLine(bool expectedState, uint32_t timeoutUs) {
    uint32_t startTime = _hal->micros();
    while (_hal->readLine() != expectedState) {
        if (elapsedMicros(startTime) > timeoutUs) {
            return false;
        }
    }
    return true;
}

int TapLink::compareDeviceIds(const uint8_t id1[DEVICE_UID_LEN], 
                              const uint8_t id2[DEVICE_UID_LEN]) {
    for (size_t i = 0; i < DEVICE_UID_LEN; i++) {
        if (id1[i] < id2[i]) return -1;
        if (id1[i] > id2[i]) return 1;
    }
    return 0;
}

uint32_t TapLink::elapsedMicros(uint32_t startTime) {
    uint32_t now = _hal->micros();
    if (now >= startTime) {
        return now - startTime;
    } else {
        // Handle micros() overflow (happens every ~71 minutes on 32-bit)
        return (UINT32_MAX - startTime) + now + 1;
    }
}

bool TapLink::isConnectionComplete() const {
    return _connectionComplete;
}

void TapLink::reset() {
    _state = State::Idle;
    _connectionComplete = false;
    _negotiationBitIndex = 0;
    _roleKnown = false;
    _rxByteIndex = 0;
    _rxBitIndex = 0;
    _txByteIndex = 0;
    _txBitIndex = 0;
    memset(_peerId, 0, DEVICE_UID_LEN);
    memset(_rxBuffer, 0, DEVICE_UID_LEN);
    memset(_txBuffer, 0, DEVICE_UID_LEN);
}
