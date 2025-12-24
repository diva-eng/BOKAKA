#pragma once
#include <stdint.h>
#include <stddef.h>

// =====================================================
// Platform Serial/USB Abstraction
// =====================================================
// This abstraction layer allows easy migration between
// Arduino Serial and STM32 USB CDC.
//
// Usage:
//   - Include this header instead of Arduino.h for serial
//   - Call platform_serial_begin() once at startup
//   - Use platform_serial_* functions instead of Serial.*
// =====================================================

// Initialize serial/USB communication
// baud: baud rate (ignored for USB CDC, kept for compatibility)
void platform_serial_begin(uint32_t baud);

// Check if data is available to read
// Returns: number of bytes available (0 if none)
int platform_serial_available();

// Read a single byte from serial buffer
// Returns: byte value (0-255) or -1 if no data available
int platform_serial_read();

// Print a string (no newline)
void platform_serial_print(const char* str);

// Print a number (no newline)
void platform_serial_print(int num);
void platform_serial_print(uint32_t num);
void platform_serial_print_hex(uint8_t byte);

// Print a string with newline (\r\n)
void platform_serial_println(const char* str);

// Flush output buffer (wait for transmission to complete)
void platform_serial_flush();
