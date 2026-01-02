 #pragma once
 #include <stdint.h>
 #include <stddef.h>
 #include <stdbool.h>
 
 #include "platform_gpio.h"
 #include "platform_timing.h"
 
 // =====================================================
 // Status Display (LED) Abstraction
 // =====================================================
 // Drives one or more LEDs with platform-agnostic GPIO/timing
 // to represent device readiness, handshake progress, and role.
 // Patterns are defined as reusable blink sequences so new states
 // or additional LEDs can be added without touching the driver.
 // =====================================================
 
 // Default pins (override via build flags if board differs)
 #ifndef STATUS_LED0_PIN
 #define STATUS_LED0_PIN 13  // Common built-in LED
 #endif
 
 #ifndef STATUS_LED1_PIN
 #define STATUS_LED1_PIN 12  // Second LED placeholder
 #endif
 
 class StatusDisplay {
 public:
    // Ready/handshake indication shown on LED 0
    enum class ReadyPattern {
        Booting,
        Idle,
        Detecting,
        Negotiating,
        WaitingAck,
        Exchanging,
        Success,
        PeerReady,  // Peer responded to CHECK_READY - distinct pattern
        Error
    };
 
    // Role indication shown on LED 1
    enum class RolePattern {
        None,       // Steady OFF (not connected)
        Unknown,    // Blinking (negotiating)
        Master,     // Steady ON
        Slave       // Slow blink
    };
 
     StatusDisplay();
 
     // Configure LEDs (pins array length = ledCount)
     void begin(const uint32_t* ledPins, size_t ledCount);
 
     // Update desired patterns (safe to call every loop)
     void setReadyPattern(ReadyPattern pattern);
     void setRolePattern(RolePattern pattern);
 
     // Run pattern timers; call from main loop
     void loop();
 
    // Pattern building blocks (exposed so platform-agnostic
    // pattern tables in the .cpp can reference them)
    struct BlinkStep {
        uint16_t durationMs;
        bool levelHigh;
    };

    struct LedPattern {
        const BlinkStep* steps;
        size_t stepCount;
        bool isSteady;
        bool steadyLevel;
    };

    struct LedState {
        const LedPattern* pattern;
        size_t stepIndex;
        uint32_t lastChangeMs;
        bool activeLevel;
    };

 private:
     static const LedPattern& patternFor(ReadyPattern pattern);
     static const LedPattern& patternFor(RolePattern pattern);
 
     void applyPattern(size_t ledIndex, const LedPattern& pattern);
     void driveLed(size_t ledIndex, bool level);
 
 private:
     static constexpr size_t MAX_LEDS = 4;
 
     uint32_t _pins[MAX_LEDS];
     size_t _ledCount;
     LedState _states[MAX_LEDS];
     bool _initialized;
 };
