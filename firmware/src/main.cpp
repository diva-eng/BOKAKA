// =====================================================
// Main Entry Point
// =====================================================
// Minimal Arduino entry point that delegates to Application class.
// All business logic is encapsulated in Application for testability.
// =====================================================

#include "board_config.h"  // Includes Arduino.h for setup/loop signatures
#include "application.h"

// Single global Application instance
static Application gApp;

void setup() {
    gApp.init();
}

void loop() {
    gApp.loop();
}
