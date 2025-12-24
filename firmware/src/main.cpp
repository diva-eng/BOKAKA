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
static uint32_t g_successHoldUntilMs = 0;

static StatusDisplay::ReadyPattern readyPatternForState(TapLink::State state) {
    switch (state) {
        case TapLink::State::Detecting:
            return StatusDisplay::ReadyPattern::Detecting;
        case TapLink::State::Negotiating:
            return StatusDisplay::ReadyPattern::Negotiating;
        case TapLink::State::MasterWaitAck:
            return StatusDisplay::ReadyPattern::WaitingAck;
        case TapLink::State::MasterDone:
        case TapLink::State::SlaveReceive:
        case TapLink::State::SlaveSend:
        case TapLink::State::SlaveDone:
            return StatusDisplay::ReadyPattern::Exchanging;
        case TapLink::State::Error:
            return StatusDisplay::ReadyPattern::Error;
        case TapLink::State::Idle:
        default:
            return StatusDisplay::ReadyPattern::Idle;
    }
}

static void updateStatusDisplay() {
    if (!gTapLink) {
        return;
    }

    uint32_t now = platform_millis();
    bool holdSuccess = (g_successHoldUntilMs != 0) && (int32_t)(g_successHoldUntilMs - now) > 0;

    if (!holdSuccess) {
        gStatusDisplay.setReadyPattern(readyPatternForState(gTapLink->getState()));
    }

    StatusDisplay::RolePattern role = StatusDisplay::RolePattern::Unknown;
    if (gTapLink->hasRole()) {
        role = gTapLink->isMaster() ? StatusDisplay::RolePattern::Master
                                    : StatusDisplay::RolePattern::Slave;
    }
    gStatusDisplay.setRolePattern(role);
}

void setup() {
    // Initialize platform abstractions
    platform_timing_init();

    gStatusDisplay.begin(STATUS_LED_PINS, 2);
    gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Booting);
    gStatusDisplay.setRolePattern(StatusDisplay::RolePattern::Unknown);
    
    gStorage.begin();
    gUsb.begin(115200);
    
    // Initialize 1-wire tap link interface
    IOneWireHal* hal = createOneWireHal();
    gTapLink = new TapLink(hal, &gStorage);
}

void loop() {
    uint32_t nowMs = platform_millis();
    if (g_successHoldUntilMs != 0 && (int32_t)(nowMs - g_successHoldUntilMs) >= 0) {
        g_successHoldUntilMs = 0;
    }

    gStorage.loop();
    gUsb.poll(gStorage);
    
    // Poll tap link state machine
    if (gTapLink) {
        gTapLink->poll();
        updateStatusDisplay();
        
        // Check if connection was completed
        if (gTapLink->isConnectionComplete()) {
            g_successHoldUntilMs = platform_millis() + 1000;  // show success for 1s
            gStatusDisplay.setReadyPattern(StatusDisplay::ReadyPattern::Success);
            // Connection successful - peer ID stored and counter incremented
            // Reset state machine for next connection
            gTapLink->reset();
        }
    }
    
    gStatusDisplay.loop();
    
    platform_delay_ms(5);
}
