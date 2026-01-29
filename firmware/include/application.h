#pragma once
// =====================================================
// Application Class
// =====================================================
// Encapsulates the main application logic, reducing
// global state and improving testability.
//
// Usage:
//   Application app;
//   app.init();
//   while (true) { app.loop(); }
// =====================================================

#include <stdint.h>
#include "storage.h"
#include "usb_serial.h"
#include "tap_link.h"
#include "status_display.h"
#include "buzzer.h"

class Application {
public:
    Application();
    ~Application();

    // Initialize all components (call once at startup)
    void init();

    // Main loop iteration (call repeatedly)
    void loop();

    // =====================================================
    // Component Access (for testing/debugging)
    // =====================================================
    
    Storage& getStorage() { return _storage; }
    StatusDisplay& getStatusDisplay() { return _statusDisplay; }
    TapLink* getTapLink() { return _tapLink; }
    Buzzer& getBuzzer() { return _buzzer; }

private:
    // =====================================================
    // Internal Helpers
    // =====================================================
    
    void updateStatusDisplay();
    
#ifdef EVAL_BOARD_TEST
    void handleMasterCommands(uint32_t nowMs);
    void handleSlaveCommands();
    void onNegotiationComplete(uint32_t nowMs);
#else
    void handleBatteryMode(uint32_t nowMs);
#endif

    static StatusDisplay::ReadyPattern readyPatternForState(TapLink::DetectionState state);

    // =====================================================
    // Components
    // =====================================================
    
    Storage _storage;
    UsbCommandHandler _usb;
    TapLink* _tapLink;
    StatusDisplay _statusDisplay;
    Buzzer _buzzer;

    // =====================================================
    // State
    // =====================================================
    
    uint32_t _connectionDetectedTime;
    uint32_t _lastCommandTime;

    // Configuration
    static constexpr uint32_t COMMAND_INTERVAL_MS = 500;
    static constexpr uint32_t SUCCESS_DISPLAY_MS = 2000;
};

