#include <Arduino.h>
#include "storage.h"
#include "usb_serial.h"

Storage gStorage;
UsbCommandHandler gUsb;

void setup() {
    gStorage.begin();
    gUsb.begin(115200);
}

void loop() {
    gStorage.loop();
    gUsb.poll(gStorage);
    delay(5);
}
