#include <Arduino.h>
#include "storage.h"
#include "device_id.h"

Storage gStorage;

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("Boot...");

    gStorage.begin();

    auto &st = gStorage.state();

    Serial.print("selfId (raw)    = ");
    for (int i = 0; i < DEVICE_UID_LEN; ++i) {
        if (st.selfId[i] < 0x10) Serial.print("0");
        Serial.print(st.selfId[i], HEX);
    }
    Serial.println();

    char hexId[DEVICE_UID_HEX_LEN + 1];
    getDeviceUidHex(hexId);
    Serial.print("UID (from HAL)  = ");
    Serial.println(hexId);

    Serial.print("totalTapCount   = ");
    Serial.println(st.totalTapCount);
}

void loop() {
    gStorage.loop();
    delay(10);
}
