// Platform serial implementation for Arduino framework
#include "platform_serial.h"
#include <Arduino.h>
#include <stdio.h>

void platform_serial_begin(uint32_t baud) {
    Serial.begin(baud);
}

int platform_serial_available() {
    return Serial.available();
}

int platform_serial_read() {
    if (Serial.available() > 0) {
        return Serial.read();
    }
    return -1;
}

void platform_serial_print(const char* str) {
    Serial.print(str);
}

void platform_serial_print(int num) {
    Serial.print(num);
}

void platform_serial_print(uint32_t num) {
    Serial.print(num);
}

void platform_serial_print_hex(uint8_t byte) {
    if (byte < 0x10) {
        Serial.print("0");
    }
    Serial.print(byte, HEX);
}

void platform_serial_println(const char* str) {
    Serial.println(str);
}

void platform_serial_flush() {
    Serial.flush();
}
