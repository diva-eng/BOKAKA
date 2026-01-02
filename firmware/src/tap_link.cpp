#include "tap_link.h"
#include <string.h>

TapLink::TapLink(IOneWireHal* hal)
    : _hal(hal)
    , _roleKnown(false)
    , _isMaster(false)
#ifdef EVAL_BOARD_TEST
    , _state(DetectionState::NoConnection)
    , _stateStartTime(0)
    , _lastLineChangeTime(0)
    , _lastLineState(true)
    , _connectionJustDetected(false)
    , _negotiationJustCompleted(false)
    , _lastPulseTime(0)
    , _isPulsing(false)
    , _pulseStartTime(0)
    , _negotiationBitIndex(0)
    , _bitSlotStartTime(0)
    , _waitingForSync(false)
    , _syncSent(false)
    , _randomSeed(0)
    , _peerReady(false)
    , _lastCommandTime(0)
    , _commandFailures(0)
#else
    , _state(DetectionState::Sleeping)
    , _stateStartTime(0)
    , _lastWakeTime(0)
    , _connectionJustEstablished(false)
    , _connectionJustLost(false)
    , _wasConnected(false)
#endif
{
    // Get our device UID for negotiation
    getDeviceUidRaw(_selfId);

#ifdef EVAL_BOARD_TEST
    // Initialize with current line state for continuous monitoring
    _lastLineState = _hal->readLine();
    _lastLineChangeTime = _hal->micros();
    _lastPulseTime = _hal->micros();

    // Initialize random seed from UID and micros for tie-breaker
    _randomSeed = _hal->micros();
    for (int i = 0; i < DEVICE_UID_LEN; i++) {
        _randomSeed ^= (_selfId[i] << (i % 4) * 8);
    }
#endif
    // Battery mode starts in sleeping state, no initialization needed
}

void TapLink::poll() {
    uint32_t now = _hal->micros();

#ifdef EVAL_BOARD_TEST
    // EVAL BOARD MODE: Continuous monitoring with presence pulses

    // Handle ongoing pulse transmission
    if (_isPulsing) {
        uint32_t pulseElapsed = elapsedMicros(_pulseStartTime);
        if (pulseElapsed >= PRESENCE_PULSE_US) {
            // Pulse complete, release line
            _hal->driveLow(false);
            _isPulsing = false;
            _lastPulseTime = now;
        }
        // While pulsing, we can't detect (we're driving the line ourselves)
        return;
    }

    // Read line state
    bool lineState = _hal->readLine();

    // Detect line state changes for connection detection
    if (lineState != _lastLineState) {
        _lastLineChangeTime = now;
        _lastLineState = lineState;
    }

    // Detection and negotiation state machine
    switch (_state) {
        case DetectionState::NoConnection:
            // Line should be HIGH (pull-up) when no device connected
            if (!lineState) {  // Line went LOW - detected peer's pulse!
                _state = DetectionState::Detecting;
                _stateStartTime = now;
            } else {
                // Send periodic presence pulses when not connected
                uint32_t sincePulse = elapsedMicros(_lastPulseTime);
                if (sincePulse >= PULSE_INTERVAL_US) {
                    sendPresencePulse();
                }
            }
            break;

        case DetectionState::Detecting:
            // Wait for stable LOW state (debounce)
            if (lineState) {
                // Line went back HIGH - was just a pulse, that's good!
                // This means peer is present - start negotiation
                _connectionJustDetected = true;
                startNegotiation();
            } else {
                // Line is still LOW, check debounce time
                uint32_t elapsed = elapsedMicros(_stateStartTime);
                if (elapsed >= DEBOUNCE_TIME_US) {
                    // Line stayed LOW for a while - peer is connected and holding
                    _connectionJustDetected = true;
                    startNegotiation();
                }
            }
            break;

        case DetectionState::Negotiating:
            // Handle bit-by-bit UID negotiation
            pollNegotiation();
            break;

        case DetectionState::Connected:
            // In Connected state, we use the command protocol instead of presence pulses.
            // Master sends CHECK_READY commands periodically.
            // Slave listens for commands and responds.
            // Disconnect is detected via command timeout, not presence pulses.
            //
            // poll() does nothing special here - command handling is done in main.cpp
            // via masterSendCommand() and slaveReceiveCommand().
            break;
    }
#else
    // BATTERY MODE: Sleep/wake state machine (for future CubeIDE implementation)
    switch (_state) {
        case DetectionState::Sleeping:
            // In sleep mode - this shouldn't be called unless we're waking up
            // The wake-up interrupt should have called handleWakeUp() first
            break;

        case DetectionState::Waking:
            // Just woken up, validate the connection is real and stable
            if (validateConnection()) {
                uint32_t elapsed = elapsedMicros(_lastWakeTime);
                if (elapsed >= VALIDATION_TIME_US) {
                    _state = DetectionState::Connected;
                    _connectionJustEstablished = true;
                    _wasConnected = true;
                }
            } else {
                // Connection not stable, go back to sleep
                _state = DetectionState::Sleeping;
            }
            break;

        case DetectionState::Connected:
            // Monitor for disconnection
            if (!validateConnection()) {
                // Start disconnect debounce
                if (_wasConnected) {
                    _stateStartTime = now;
                    _wasConnected = false;
                } else {
                    uint32_t elapsed = elapsedMicros(_stateStartTime);
                    if (elapsed >= DISCONNECT_DEBOUNCE_US) {
                        _state = DetectionState::Disconnected;
                        _connectionJustLost = true;
                    }
                }
            } else {
                _wasConnected = true;
            }
            break;

        case DetectionState::Disconnected:
            // Connection lost - could transition back to sleep or handle disconnect
            break;
    }
#endif
}

#ifdef EVAL_BOARD_TEST
void TapLink::sendPresencePulse() {
    // Send a brief LOW pulse to signal our presence
    // Other device will detect this and know we're connected
    _hal->driveLow(true);
    _isPulsing = true;
    _pulseStartTime = _hal->micros();
    // Pulse will be released in poll() after PRESENCE_PULSE_US
}

void TapLink::startNegotiation() {
    _state = DetectionState::Negotiating;
    _negotiationBitIndex = 0;
    _roleKnown = false;
    _isMaster = false;

    // === ROBUST SYNCHRONIZATION HANDSHAKE ===
    // Goal: Get both boards to start bit exchange at the same time
    // Strategy: Use multiple sync pulses to align timing

    // Step 1: Release line and wait for HIGH
    _hal->driveLow(false);
    uint32_t waitStart = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(waitStart) > 100000) break;  // 100ms timeout
    }
    _hal->delayMicros(1000);  // Brief settle time

    // Step 2: First sync pulse - signal "I'm entering negotiation"
    _hal->driveLow(true);
    _hal->delayMicros(SYNC_PULSE_US);  // 10ms pulse
    _hal->driveLow(false);

    // Step 3: Wait for line to be HIGH (both released from first pulse)
    waitStart = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(waitStart) > 20000) break;
    }

    // Step 4: Wait for peer's sync pulse (line goes LOW)
    // This ensures peer has also entered negotiation
    waitStart = _hal->micros();
    bool sawPeerSync = false;
    while (elapsedMicros(waitStart) < 50000) {  // Wait up to 50ms for peer
        if (!_hal->readLine()) {
            sawPeerSync = true;
            break;
        }
    }

    // Step 5: If we saw peer's sync, wait for it to complete
    if (sawPeerSync) {
        waitStart = _hal->micros();
        while (!_hal->readLine()) {
            if (elapsedMicros(waitStart) > 20000) break;
        }
    }

    // Step 6: Second sync pulse - final alignment
    // Both boards should hit this at approximately the same time
    _hal->delayMicros(SYNC_WAIT_US);  // 5ms wait
    _hal->driveLow(true);
    _hal->delayMicros(SYNC_PULSE_US);  // 10ms pulse
    _hal->driveLow(false);

    // Step 7: Wait for line HIGH, then fixed delay
    waitStart = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(waitStart) > 20000) break;
    }
    _hal->delayMicros(SYNC_WAIT_US);  // 5ms final alignment

    // Now both boards should be synchronized within ~1-2ms
    _bitSlotStartTime = _hal->micros();
    _waitingForSync = false;
    _syncSent = true;
}

void TapLink::pollNegotiation() {
    // Use blocking negotiation for reliable timing
    // CRITICAL: Drive period is 5ms, sample at 2.5ms
    // This ensures even with 2ms sync error, both boards are driving when we sample

    while (_negotiationBitIndex < NEGOTIATION_BITS && !_roleKnown) {
        // Get my bit for this position
        uint8_t byteIdx = _negotiationBitIndex / 8;
        uint8_t bitIdx = 7 - (_negotiationBitIndex % 8);  // MSB first
        bool myBit = (_selfId[byteIdx] >> bitIdx) & 1;

        // === BIT SLOT START ===
        // For '0' bit, drive low; for '1' bit, release (high via pull-up)
        if (!myBit) {
            _hal->driveLow(true);   // Send '0' by driving low
        } else {
            _hal->driveLow(false);  // Send '1' by releasing (high)
        }

        // === WAIT UNTIL SAMPLE POINT ===
        // Wait 2.5ms into the 5ms drive period
        // At this point, even with 2ms sync error, peer should also be in their drive phase
        _hal->delayMicros(BIT_SAMPLE_US);

        // === SAMPLE THE LINE (multiple times for reliability) ===
        // Take 3 samples with small delays to ensure stable reading
        bool sample1 = _hal->readLine();
        _hal->delayMicros(100);
        bool sample2 = _hal->readLine();
        _hal->delayMicros(100);
        bool sample3 = _hal->readLine();

        // Use majority voting
        int lowCount = (!sample1 ? 1 : 0) + (!sample2 ? 1 : 0) + (!sample3 ? 1 : 0);
        bool lineIsLow = (lowCount >= 2);

        // === CONTINUE DRIVING UNTIL END OF DRIVE PERIOD ===
        // This ensures we're still driving when peer samples
        _hal->delayMicros(BIT_DRIVE_US - BIT_SAMPLE_US - 200);

        // === RELEASE LINE ===
        _hal->driveLow(false);

        // === EVALUATE RESULT ===
        // Wired-AND truth table:
        // My bit | Peer bit | Line | My conclusion
        // -------|----------|------|---------------
        //   1    |    1     | HIGH | Both sent 1, continue
        //   1    |    0     | LOW  | I sent 1, line LOW → I'm MASTER
        //   0    |    1     | LOW  | I sent 0, can't tell → continue (peer will detect)
        //   0    |    0     | LOW  | Both sent 0, continue

        if (myBit && lineIsLow) {
            // I released (sent '1') but line is LOW → peer is driving LOW (sent '0')
            // My bit '1' > peer bit '0' → I have HIGHER UID → I'm MASTER
            _isMaster = true;
            _roleKnown = true;
        }
        // Note: If I sent '0', I'm driving LOW, so line is always LOW
        // I can't determine peer's bit, but if peer sent '1' (released),
        // peer will see line is LOW and know THEY are master (I'm slave)

        // === RECOVERY PERIOD ===
        _hal->delayMicros(BIT_RECOVERY_US);

        _negotiationBitIndex++;
    }

    // Negotiation complete
    if (!_roleKnown) {
        // All 32 bits compared equal (extremely unlikely with STM32 UIDs)
        // Use random tie-breaker with same protocol
        _randomSeed = _randomSeed * 1103515245 + 12345;  // LCG random
        bool myTieBreaker = (_randomSeed >> 16) & 1;

        // Exchange tie-breaker bit
        if (!myTieBreaker) {
            _hal->driveLow(true);   // Send '0'
        } else {
            _hal->driveLow(false);  // Send '1'
        }

        _hal->delayMicros(BIT_SAMPLE_US);
        bool peerTieBreaker = _hal->readLine();  // HIGH means peer sent '1'
        _hal->delayMicros(BIT_DRIVE_US - BIT_SAMPLE_US);
        _hal->driveLow(false);

        if (myTieBreaker && !peerTieBreaker) {
            _isMaster = true;
        } else if (!myTieBreaker && peerTieBreaker) {
            _isMaster = false;
        } else {
            // Still tied - use UID sum as final fallback
            uint32_t sum = 0;
            for (int i = 0; i < DEVICE_UID_LEN; i++) {
                sum += _selfId[i];
            }
            _isMaster = (sum & 1);  // Odd sum = master
        }
        _roleKnown = true;
    }

    _state = DetectionState::Connected;
    _negotiationJustCompleted = true;
    _lastPulseTime = _hal->micros();  // Reset pulse timer
}

bool TapLink::sendBit(bool bit) {
    if (!bit) {
        _hal->driveLow(true);  // Drive low for '0'
        _hal->delayMicros(BIT_DRIVE_US);
        _hal->driveLow(false);
    } else {
        _hal->driveLow(false); // Release for '1'
        _hal->delayMicros(BIT_DRIVE_US);
    }
    return true;
}

bool TapLink::readBit() {
    return _hal->readLine();  // HIGH = '1', LOW = '0'
}

bool TapLink::isConnectionDetected() {
    if (_connectionJustDetected) {
        _connectionJustDetected = false;
        return true;
    }
    return false;
}

bool TapLink::isNegotiationComplete() {
    if (_negotiationJustCompleted) {
        _negotiationJustCompleted = false;
        return true;
    }
    return false;
}

void TapLink::reset() {
    _state = DetectionState::NoConnection;
    _connectionJustDetected = false;
    _negotiationJustCompleted = false;
    _isPulsing = false;
    _roleKnown = false;
    _isMaster = false;
    _negotiationBitIndex = 0;
    _peerReady = false;
    _lastCommandTime = 0;
    _commandFailures = 0;
    _hal->driveLow(false);  // Make sure line is released
}

// === Command Protocol Implementation ===

void TapLink::sendStartPulse() {
    _hal->driveLow(true);
    _hal->delayMicros(CMD_START_PULSE_US);
    _hal->driveLow(false);
}

bool TapLink::waitForLineHigh(uint32_t timeoutUs) {
    uint32_t startTime = _hal->micros();
    while (!_hal->readLine()) {
        if (elapsedMicros(startTime) > timeoutUs) {
            return false;
        }
    }
    return true;
}

void TapLink::sendByte(uint8_t byte) {
    // Send 8 bits, MSB first
    for (int i = 7; i >= 0; i--) {
        bool bit = (byte >> i) & 1;
        if (!bit) {
            _hal->driveLow(true);   // Send '0' by driving LOW
        } else {
            _hal->driveLow(false);  // Send '1' by releasing (HIGH via pull-up)
        }
        _hal->delayMicros(CMD_BIT_DRIVE_US);
        _hal->driveLow(false);  // Release line
        _hal->delayMicros(CMD_BIT_RECOVERY_US);
    }
}

bool TapLink::receiveByte(uint8_t* byte, uint32_t timeoutUs) {
    uint8_t result = 0;
    
    // Receive 8 bits, MSB first
    for (int i = 7; i >= 0; i--) {
        // Wait for sample point
        _hal->delayMicros(CMD_BIT_SAMPLE_US);
        
        // Sample the line (take 3 samples for reliability)
        bool sample1 = _hal->readLine();
        _hal->delayMicros(100);
        bool sample2 = _hal->readLine();
        _hal->delayMicros(100);
        bool sample3 = _hal->readLine();
        
        // Majority voting
        int highCount = (sample1 ? 1 : 0) + (sample2 ? 1 : 0) + (sample3 ? 1 : 0);
        bool bit = (highCount >= 2);
        
        result |= (bit ? 1 : 0) << i;
        
        // Wait for rest of bit slot
        _hal->delayMicros(CMD_BIT_DRIVE_US - CMD_BIT_SAMPLE_US - 200 + CMD_BIT_RECOVERY_US);
    }
    
    *byte = result;
    return true;
}

TapResponse TapLink::masterSendCommand(TapCommand cmd) {
    if (!_roleKnown || !_isMaster) return TapResponse::NONE;
    if (_state != DetectionState::Connected) return TapResponse::NONE;
    
    // Send START pulse to signal command coming
    sendStartPulse();
    
    // Brief turnaround for slave to recognize START
    _hal->delayMicros(CMD_TURNAROUND_US);
    
    // Send command byte
    sendByte(static_cast<uint8_t>(cmd));
    
    // Turnaround for slave to prepare response
    _hal->delayMicros(CMD_TURNAROUND_US);
    
    // Receive response byte
    uint8_t response;
    if (!receiveByte(&response, CMD_TIMEOUT_US)) {
        // No response - peer may have disconnected
        _commandFailures++;
        if (_commandFailures >= MAX_COMMAND_FAILURES) {
            // Too many failures - consider disconnected
            _state = DetectionState::NoConnection;
            _roleKnown = false;
            _peerReady = false;
        }
        return TapResponse::NONE;
    }
    
    // Successful response - reset failure counter
    _commandFailures = 0;
    
    // Update peer ready state for CHECK_READY
    if (cmd == TapCommand::CHECK_READY) {
        _peerReady = (response == static_cast<uint8_t>(TapResponse::ACK));
    }
    
    _lastCommandTime = _hal->micros();
    return static_cast<TapResponse>(response);
}

bool TapLink::slaveHasCommand() {
    if (!_roleKnown || _isMaster) return false;
    if (_state != DetectionState::Connected) return false;
    
    // Check if line is LOW (potential START pulse)
    return !_hal->readLine();
}

TapCommand TapLink::slaveReceiveCommand() {
    if (!_roleKnown || _isMaster) return TapCommand::NONE;
    if (_state != DetectionState::Connected) return TapCommand::NONE;
    
    // Measure how long line has been LOW (to distinguish from presence pulse)
    uint32_t startTime = _hal->micros();
    
    // Wait for line to go HIGH (end of START pulse)
    while (!_hal->readLine()) {
        if (elapsedMicros(startTime) > CMD_TIMEOUT_US) {
            return TapCommand::NONE;  // Timeout waiting for START to end
        }
    }
    
    uint32_t pulseLength = elapsedMicros(startTime);
    
    // Check if it was a real START pulse (>3ms) vs presence pulse (~2ms)
    if (pulseLength < 3000) {
        // Too short - was just a presence pulse, not a command
        return TapCommand::NONE;
    }
    
    // Wait turnaround time (same as master)
    _hal->delayMicros(CMD_TURNAROUND_US);
    
    // Receive command byte
    uint8_t cmd;
    if (!receiveByte(&cmd, CMD_TIMEOUT_US)) {
        return TapCommand::NONE;
    }
    
    return static_cast<TapCommand>(cmd);
}

void TapLink::slaveSendResponse(TapResponse response) {
    if (!_roleKnown || _isMaster) return;
    if (_state != DetectionState::Connected) return;
    
    // Wait turnaround time before sending response
    _hal->delayMicros(CMD_TURNAROUND_US);
    
    // Send response byte
    sendByte(static_cast<uint8_t>(response));
}
#else
bool TapLink::isConnectionEstablished() {
    if (_connectionJustEstablished) {
        _connectionJustEstablished = false;
        return true;
    }
    return false;
}

bool TapLink::isConnectionLost() {
    if (_connectionJustLost) {
        _connectionJustLost = false;
        return true;
    }
    return false;
}

void TapLink::prepareForSleep() {
    // Configure GPIO for sleep mode
    // In Arduino framework, this is limited, but we can at least
    // ensure the pin is ready for wake-up
    _state = DetectionState::Sleeping;
    _connectionJustEstablished = false;
    _connectionJustLost = false;
}

void TapLink::handleWakeUp() {
    // Called when MCU wakes from sleep due to tap interrupt
    // Reconfigure GPIO for active mode and start validation
    _lastWakeTime = _hal->micros();
    _state = DetectionState::Waking;
    _stateStartTime = _lastWakeTime;
}

void TapLink::reset() {
    _state = DetectionState::Sleeping;
    _connectionJustEstablished = false;
    _connectionJustLost = false;
    _wasConnected = false;
}

bool TapLink::validateConnection() {
    // Validate that the tap connection is real and stable
    // For battery-powered devices, we need to be careful about:
    // 1. Both devices having compatible voltage levels
    // 2. Ground connection being established
    // 3. Signal line being properly connected

    // Read line multiple times to check stability
    bool readings[5];
    for (int i = 0; i < 5; i++) {
        readings[i] = _hal->readLine();
        _hal->delayMicros(100);  // Small delay between readings
    }

    // For a valid tap connection, we expect the line to be driven
    // by the other device. With two batteries, the behavior depends
    // on which device has higher voltage and who's driving the line.

    // Check if readings are consistent (not floating/changing randomly)
    bool firstReading = readings[0];
    bool consistent = true;
    for (int i = 1; i < 5; i++) {
        if (readings[i] != firstReading) {
            consistent = false;
            break;
        }
    }

    return consistent;  // Connection is valid if line state is stable
}
#endif

uint32_t TapLink::elapsedMicros(uint32_t startTime) {
    uint32_t now = _hal->micros();
    if (now >= startTime) {
        return now - startTime;
    } else {
        // Handle micros() overflow (happens every ~71 minutes on 32-bit)
        return (UINT32_MAX - startTime) + now + 1;
    }
}