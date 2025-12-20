#pragma once
#include <Arduino.h>
#include "storage.h"
#include "device_id.h"

// Simple USB serial command handler
// Usage:
//   UsbCommandHandler usb;
//   usb.begin();
//   Call usb.poll(gStorage) inside loop()
class UsbCommandHandler {
public:
    // Initialize serial (adjust baud if needed)
    void begin(unsigned long baud = 115200);

    // Call periodically to process input and output responses
    void poll(Storage& storage);

private:
    static constexpr size_t CMD_BUF_SIZE = 128;
    char _buf[CMD_BUF_SIZE];
    size_t _len = 0;

    void handleLine(Storage& storage, const char* line);
    
    // commands
    void cmdHello(Storage& storage);
    void cmdGetState(Storage& storage);
    void cmdClear(Storage& storage);
    void cmdDump(Storage& storage, int offset, int count);
    void cmdProvisionKey(Storage& storage, int version, const char* keyHex);
    void cmdSignState(Storage& storage, const char* nonceHex);
#ifdef ENABLE_TEST_COMMANDS
    void cmdGetKey(Storage& storage);
#endif

    // utils
    void printHex(const uint8_t* data, size_t len);
    bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);
    void bytesToHex(const uint8_t* in, size_t len, char* out);
};
