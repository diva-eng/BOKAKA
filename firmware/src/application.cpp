#include "application.h"
#include "board_config.h"
#include "tap_link_hal.h"
#include "platform_timing.h"

// LED pin configuration
static const uint32_t STATUS_LED_PINS[] = { STATUS_LED0_PIN, STATUS_LED1_PIN };

// Buzzer timing constants
static constexpr uint32_t SUCCESS_TONE_DELAY_MS = 150;  // Delay before success tone

Application::Application()
    : _tapLink(nullptr)
    , _connectionDetectedTime(0)
    , _lastCommandTime(0)
{
}

Application::~Application() {
    if (_tapLink) {
        delete _tapLink;
        _tapLink = nullptr;
    }
}

void Application::init() {
    // Initialize platform abstractions
    platform_timing_init();

    // Initialize status display
    _statusDisplay.begin(STATUS_LED_PINS, 2);
    _statusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);

    // Initialize buzzer (HS-F02A on configurable pin)
    _buzzer.begin(BUZZER_PIN);

    // Initialize storage
    _storage.begin();

    // Initialize USB command handler
    _usb.begin(115200);

    // Initialize tap link with hardware abstraction
    IOneWireHal* hal = createOneWireHal();
    _tapLink = new TapLink(hal);
}

void Application::loop() {
    uint32_t nowMs = platform_millis();

    // Process storage (delayed writes)
    _storage.loop();

    // Process USB commands
    _usb.poll(_storage);

    // Process tap link
    if (_tapLink) {
        _tapLink->poll();
        updateStatusDisplay();

#ifdef EVAL_BOARD_TEST
        // Check if connection was just detected
        if (_tapLink->isConnectionDetected()) {
            // Play detection beep when devices touch
            _buzzer.playDetectionTone();
        }

        // Check if negotiation just completed
        if (_tapLink->isNegotiationComplete()) {
            onNegotiationComplete(nowMs);
        }

        // Command protocol handling
        if (_tapLink->getState() == TapLink::DetectionState::Connected &&
            _tapLink->hasRole()) {

            if (_tapLink->isMaster()) {
                handleMasterCommands(nowMs);
            } else {
                handleSlaveCommands();
            }
        }
#else
        handleBatteryMode(nowMs);
#endif
    }

    // Update LED patterns
    _statusDisplay.loop();

    // Update buzzer (handles melodies and scheduled tones)
    _buzzer.loop();

    // Fast polling needed to detect 2ms presence pulses
    platform_delay_ms(1);
}

// =====================================================
// Status Display
// =====================================================

StatusDisplay::ReadyPattern Application::readyPatternForState(TapLink::DetectionState state) {
#ifdef EVAL_BOARD_TEST
    switch (state) {
        case TapLink::DetectionState::NoConnection:
            return StatusDisplay::ReadyPattern::Idle;
        case TapLink::DetectionState::Detecting:
            return StatusDisplay::ReadyPattern::Detecting;
        case TapLink::DetectionState::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;
        case TapLink::DetectionState::Connected:
            return StatusDisplay::ReadyPattern::Success;
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
#else
    switch (state) {
        case TapLink::DetectionState::Sleeping:
            return StatusDisplay::ReadyPattern::Idle;
        case TapLink::DetectionState::Waking:
            return StatusDisplay::ReadyPattern::Detecting;
        case TapLink::DetectionState::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;
        case TapLink::DetectionState::Connected:
            return StatusDisplay::ReadyPattern::Success;
        case TapLink::DetectionState::Disconnected:
            return StatusDisplay::ReadyPattern::Error;
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
#endif
}

void Application::updateStatusDisplay() {
    if (!_tapLink) {
        return;
    }

    uint32_t now = platform_millis();
    bool showConnectionSuccess = (_connectionDetectedTime != 0) &&
                                 (now - _connectionDetectedTime < SUCCESS_DISPLAY_MS);

    TapLink::DetectionState currentState = _tapLink->getState();

#ifdef EVAL_BOARD_TEST
    // If master and peer is ready, show PeerReady pattern
    if (currentState == TapLink::DetectionState::Connected &&
        _tapLink->isMaster() && _tapLink->isPeerReady()) {
        _statusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::PeerReady);
    } else
#endif
    if (showConnectionSuccess) {
        _statusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);
    } else {
        _statusDisplay.setReadyPattern(readyPatternForState(currentState));
    }

    // LED1: Shows negotiated role (Master=ON, Slave=blink, None=OFF)
#ifdef EVAL_BOARD_TEST
    if (_tapLink->hasRole()) {
        if (_tapLink->isMaster()) {
            _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);
        } else {
            _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);
        }
    } else if (currentState == TapLink::DetectionState::Negotiating) {
        _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
    } else {
        _statusDisplay.setRolePattern(StatusDisplay::RolePattern::None);
    }
#else
    if (_tapLink->hasRole()) {
        if (_tapLink->isMaster()) {
            _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);
        } else {
            _statusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);
        }
    } else {
        _statusDisplay.setRolePattern(StatusDisplay::RolePattern::None);
    }
#endif
}

// =====================================================
// Eval Board Mode (USB-powered, continuous monitoring)
// =====================================================

#ifdef EVAL_BOARD_TEST

void Application::onNegotiationComplete(uint32_t nowMs) {
    _connectionDetectedTime = nowMs;
    _lastCommandTime = nowMs;

    // Increment tap count for both master and slave
    // Use optimized save (8 bytes vs 896) since tap can be very brief
    _storage.incrementTapCount();
    _storage.saveTapCountOnly();
}

void Application::handleMasterCommands(uint32_t nowMs) {
    // MASTER: Periodically send CHECK_READY, then do ID exchange
    if (nowMs - _lastCommandTime < COMMAND_INTERVAL_MS) {
        return;
    }

    if (!_tapLink->isPeerReady()) {
        _tapLink->masterSendCommand(TapCommand::CHECK_READY);
    } else if (!_tapLink->isIdExchangeComplete()) {
        uint8_t peerId[DEVICE_UID_LEN];
        if (_tapLink->masterRequestId(peerId)) {
            if (_tapLink->masterSendId()) {
                if (_storage.addLink(peerId)) {
                    _storage.saveLinkOnly();
                }
                // Schedule success tone with delay (ID exchange happens fast)
                _buzzer.scheduleSuccessTone(SUCCESS_TONE_DELAY_MS);
            }
        }
    } else {
        _tapLink->masterSendCommand(TapCommand::CHECK_READY);
    }

    _lastCommandTime = nowMs;
}

void Application::handleSlaveCommands() {
    // SLAVE: Check for incoming commands and respond
    if (!_tapLink->slaveHasCommand()) {
        return;
    }

    TapCommand cmd = _tapLink->slaveReceiveCommand();
    switch (cmd) {
        case TapCommand::CHECK_READY:
            _tapLink->slaveSendResponse(TapResponse::ACK);
            break;

        case TapCommand::REQUEST_ID:
            _tapLink->slaveHandleRequestId();
            break;

        case TapCommand::SEND_ID: {
            uint8_t peerId[DEVICE_UID_LEN];
            if (_tapLink->slaveHandleSendId(peerId)) {
                if (_storage.addLink(peerId)) {
                    _storage.saveLinkOnly();
                }
                // Schedule success tone with delay (ID exchange happens fast)
                _buzzer.scheduleSuccessTone(SUCCESS_TONE_DELAY_MS);
            }
            break;
        }

        case TapCommand::NONE:
            // Was just a presence pulse, ignore
            break;

        default:
            _tapLink->slaveSendResponse(TapResponse::NAK);
            break;
    }
}

#else

// =====================================================
// Battery Mode (sleep/wake with CR2032)
// =====================================================

void Application::handleBatteryMode(uint32_t nowMs) {
    // Check for connection events
    if (_tapLink->isConnectionEstablished()) {
        _connectionDetectedTime = nowMs;
        // Play detection beep when connection established
        _buzzer.playDetectionTone();
        // Schedule success tone (ID exchange is quick in battery mode)
        _buzzer.scheduleSuccessTone(SUCCESS_TONE_DELAY_MS);
    }

    if (_tapLink->isConnectionLost()) {
        // Handle connection lost event
    }

    TapLink::DetectionState currentState = _tapLink->getState();

    if (currentState == TapLink::DetectionState::Sleeping) {
        _tapLink->prepareForSleep();
        platform_delay_ms(100);

        // Placeholder for interrupt-driven wake-up
        static bool simulateWake = false;
        if (!simulateWake) {
            simulateWake = true;
            _tapLink->handleWakeUp();
        }

    } else if (currentState == TapLink::DetectionState::Disconnected) {
        platform_delay_ms(500);
        _tapLink->reset();
    }
}

#endif

