#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =====================================================
// Platform Storage Abstraction
// =====================================================
// This abstraction layer allows easy migration between
// Arduino EEPROM and STM32 EEPROM emulation.
//
// Usage:
//   - Include this header instead of EEPROM.h
//   - Call platform_storage_begin() once at startup
//   - Use platform_storage_read/write instead of EEPROM.read/write
// =====================================================

// Initialize storage system
// size: total storage size in bytes
// Returns: true if successful, false on error
bool platform_storage_begin(size_t size);

// Read a single byte from storage
// address: byte address (0-based)
// Returns: byte value (0-255)
uint8_t platform_storage_read(size_t address);

// Write a single byte to storage
// address: byte address (0-based)
// value: byte value to write (0-255)
void platform_storage_write(size_t address, uint8_t value);

// Commit buffered writes to persistent storage
// Returns: true if successful, false on error
bool platform_storage_commit();
