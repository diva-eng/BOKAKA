#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "i_storage.h"
#include "device_id.h"

// Forward declaration (full definition in storage.h)
struct PersistPayloadV1;

// =====================================================
// USB Serial Command Handler
// =====================================================
// Processes USB CDC serial commands and interacts with storage.
// Uses IStorage interface to enable testing with mock storage.
//
// Usage:
//   UsbCommandHandler usb;
//   usb.begin();
//   usb.poll(gStorage);  // in loop()
// =====================================================

class UsbCommandHandler {
public:
    // Initialize serial (baud rate ignored for USB CDC)
    void begin(unsigned long baud = 115200);

    // Call periodically to process input and output responses
    void poll(IStorage& storage);

private:
    static constexpr size_t CMD_BUF_SIZE = 128;
    char _buf[CMD_BUF_SIZE];
    size_t _len = 0;

    void handleLine(IStorage& storage, const char* line);
    
    // Command handlers
    void cmdHello(IStorage& storage);
    void cmdGetState(IStorage& storage);
    void cmdClear(IStorage& storage);
    void cmdDump(IStorage& storage, int offset, int count);
    void cmdProvisionKey(IStorage& storage, int version, const char* keyHex);
    void cmdSignState(IStorage& storage, const char* nonceHex);
#ifdef ENABLE_TEST_COMMANDS
    void cmdGetKey(IStorage& storage);
#endif

    // Utility functions
    void printHex(const uint8_t* data, size_t len);
    bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);
    void bytesToHex(const uint8_t* in, size_t len, char* out);
};
