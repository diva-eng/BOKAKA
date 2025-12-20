#include <Arduino.h>
#include "storage.h"
#include "usb_serial.h"
#include "tap_link.h"
#include "tap_link_hal.h"

Storage gStorage;
UsbCommandHandler gUsb;
TapLink* gTapLink = nullptr;

void setup() {
    gStorage.begin();
    gUsb.begin(115200);
    
    // Initialize 1-wire tap link interface
    IOneWireHal* hal = createOneWireHal();
    gTapLink = new TapLink(hal, &gStorage);
}

void loop() {
    gStorage.loop();
    gUsb.poll(gStorage);
    
    // Poll tap link state machine
    if (gTapLink) {
        gTapLink->poll();
        
        // Check if connection was completed
        if (gTapLink->isConnectionComplete()) {
            // Connection successful - peer ID stored and counter incremented
            // Reset state machine for next connection
            gTapLink->reset();
        }
    }
    
    delay(5);
}
