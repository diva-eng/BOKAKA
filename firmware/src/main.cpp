#include <Arduino.h>
#include "storage.h"
#include "usb_serial.h"
#include "tap_link.h"
#include "tap_link_hal.h"
#include "platform_timing.h"
#include "status_display.h"

Storage gStorage;
UsbCommandHandler gUsb;
TapLink* gTapLink = nullptr;
StatusDisplay gStatusDisplay;

static const uint32_t STATUS_LED_PINS[] = { PA5, STATUS_LED1_PIN };
static uint32_t g_connectionDetectedTime = 0;
static uint32_t g_lastCommandTime = 0;
static const uint32_t COMMAND_INTERVAL_MS = 500;  // Send CHECK_READY every 500ms

static StatusDisplay::ReadyPattern readyPatternForState(TapLink::DetectionState state) {
#ifdef EVAL_BOARD_TEST
    switch (state) {
        case TapLink::DetectionState::NoConnection:
            return StatusDisplay::ReadyPattern::Idle;
        case TapLink::DetectionState::Detecting:
            return StatusDisplay::ReadyPattern::Detecting;
        case TapLink::DetectionState::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;  // Fast blink during negotiation
        case TapLink::DetectionState::Connected:
            return StatusDisplay::ReadyPattern::Success;
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
#else
    switch (state) {
        case TapLink::DetectionState::Sleeping:
            return StatusDisplay::ReadyPattern::Idle;  // Slow blink to show sleeping
        case TapLink::DetectionState::Waking:
            return StatusDisplay::ReadyPattern::Detecting;  // Fast blink during validation
        case TapLink::DetectionState::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;  // Fast blink during negotiation
        case TapLink::DetectionState::Connected:
            return StatusDisplay::ReadyPattern::Success;  // Steady on when connected
        case TapLink::DetectionState::Disconnected:
            return StatusDisplay::ReadyPattern::Error;  // Error pattern when disconnected
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
#endif
}

static void updateStatusDisplay() {
    if (!gTapLink) {
        return;
    }

    uint32_t now = platform_millis();
    bool showConnectionSuccess = (g_connectionDetectedTime != 0) &&
                                 (now - g_connectionDetectedTime < 2000); // Show success for 2 seconds

    TapLink::DetectionState currentState = gTapLink->getState();

#ifdef EVAL_BOARD_TEST
    // If master and peer is ready, show PeerReady pattern
    if (currentState == TapLink::DetectionState::Connected && 
        gTapLink->isMaster() && gTapLink->isPeerReady()) {
        gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::PeerReady);
    } else 
#endif
    if (showConnectionSuccess) {
        gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);
    } else {
        gStatusDisplay.setReadyPattern(readyPatternForState(currentState));
    }

    // LED1: Shows negotiated role (Master=ON, Slave=blink, None=OFF)
#ifdef EVAL_BOARD_TEST
    if (gTapLink->hasRole()) {
        if (gTapLink->isMaster()) {
            gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);  // Steady ON
        } else {
            gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);   // Slow blink
        }
    } else if (currentState == TapLink::DetectionState::Negotiating) {
        // Blink during negotiation
        gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
    } else {
        gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::None);   // Steady OFF when not connected
    }
#else
    if (gTapLink->hasRole()) {
        if (gTapLink->isMaster()) {
            gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Master);  // Steady ON
        } else {
            gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Slave);   // Slow blink
        }
    } else {
        gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::None);   // Steady OFF when not connected
    }
#endif
}

void setup() {
    // Initialize platform abstractions
    platform_timing_init();

    gStatusDisplay.begin(STATUS_LED_PINS, 2);
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);

    gStorage.begin();
    gUsb.begin(115200);

    // Initialize simple 1-wire tap link detection
    IOneWireHal* hal = createOneWireHal();
    gTapLink = new TapLink(hal);
}

void loop() {
    uint32_t nowMs = platform_millis();

    gStorage.loop();
    gUsb.poll(gStorage);

    // Poll tap link detection
    if (gTapLink) {
        gTapLink->poll();
        updateStatusDisplay();

#ifdef EVAL_BOARD_TEST
        // Check if connection was just detected
        if (gTapLink->isConnectionDetected()) {
            // Connection detected, negotiation starting
        }

        // Check if negotiation just completed
        if (gTapLink->isNegotiationComplete()) {
            g_connectionDetectedTime = nowMs;
            g_lastCommandTime = nowMs;  // Reset command timer
            // Negotiation complete! Role is now known
            // Master = higher UID, Slave = lower UID
        }
        
        // === Command Protocol Handling ===
        if (gTapLink->getState() == TapLink::DetectionState::Connected && 
            gTapLink->hasRole()) {
            
            if (gTapLink->isMaster()) {
                // MASTER: Periodically send CHECK_READY command
                if (nowMs - g_lastCommandTime >= COMMAND_INTERVAL_MS) {
                    TapResponse response = gTapLink->masterSendCommand(TapCommand::CHECK_READY);
                    if (response == TapResponse::ACK) {
                        // Slave is ready! LED0 will show PeerReady pattern
                        // (handled in updateStatusDisplay)
                    }
                    g_lastCommandTime = nowMs;
                }
            } else {
                // SLAVE: Check for incoming commands and respond
                if (gTapLink->slaveHasCommand()) {
                    TapCommand cmd = gTapLink->slaveReceiveCommand();
                    switch (cmd) {
                        case TapCommand::CHECK_READY:
                            // Respond with ACK to indicate we're ready
                            gTapLink->slaveSendResponse(TapResponse::ACK);
                            break;
                        
                        case TapCommand::REQUEST_ID:
                            // TODO: Send our device ID
                            gTapLink->slaveSendResponse(TapResponse::ACK);
                            break;
                        
                        case TapCommand::NONE:
                            // Was just a presence pulse, ignore
                            break;
                        
                        default:
                            // Unknown command, respond NAK
                            gTapLink->slaveSendResponse(TapResponse::NAK);
                            break;
                    }
                }
            }
        }
#else
        // Check for connection events
        if (gTapLink->isConnectionEstablished()) {
            g_connectionDetectedTime = nowMs;
            // Connection established! Handle the tap event here
            // This is where you'd process the tap data exchange
        }

        if (gTapLink->isConnectionLost()) {
            // Connection lost - could log, update state, etc.
        }

        // Check current state for sleep/wake decisions
        TapLink::DetectionState currentState = gTapLink->getState();

        if (currentState == TapLink::DetectionState::Sleeping) {
            // Prepare to enter sleep mode
            // NOTE: Arduino framework doesn't provide easy access to STM32 sleep modes
            // For proper implementation, you'd need STM32 HAL:
            // - Configure GPIO interrupt for wake-up on tap pin
            // - Call HAL_PWR_EnterSLEEPMode() or HAL_PWR_EnterSTOPMode()
            // - Wake-up interrupt handler calls gTapLink->handleWakeUp()

            // For now, we'll simulate brief "sleep" with delay
            // In real implementation, replace this with proper sleep entry
            gTapLink->prepareForSleep();
            platform_delay_ms(100);  // Simulate sleep period

            // Simulate wake-up (in real implementation, this would be interrupt-driven)
            // This is just for testing - replace with actual wake-up interrupt
            static bool simulateWake = false;
            if (!simulateWake) {
                simulateWake = true;
                // Simulate tap wake-up
                gTapLink->handleWakeUp();
            }

        } else if (currentState == TapLink::DetectionState::Disconnected) {
            // After disconnection, prepare to go back to sleep
            platform_delay_ms(500);  // Brief delay before sleep
            gTapLink->reset();  // Reset to sleeping state
        }
#endif
    }

    gStatusDisplay.loop();

    // Fast polling needed to detect 2ms presence pulses
    platform_delay_ms(1);
}
