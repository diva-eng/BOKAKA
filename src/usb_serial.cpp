#include "usb_serial.h"
#include "fw_config.h"

void UsbCommandHandler::begin(unsigned long baud) {
    Serial.begin(baud);
    // For some cores, need to wait for Serial to be ready
    // You can also do while(!Serial) {} outside
    _len = 0;
}

// Call inside loop()
void UsbCommandHandler::poll(Storage& storage) {
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Support \r\n / \n as line endings
        if (c == '\r') continue;

        if (c == '\n') {
            _buf[_len] = '\0';
            if (_len > 0) {
                handleLine(storage, _buf);
            }
            _len = 0;
        } else {
            if (_len < CMD_BUF_SIZE - 1) {
                _buf[_len++] = c;
            } else {
                // Too long — discard this line
                _len = 0;
            }
        }
    }
}

void UsbCommandHandler::handleLine(Storage& storage, const char* line) {
    // Simple split by spaces to recognize commands
    // Support (case-insensitive):
    //  HELLO
    //  GET_STATE
    //  CLEAR
    //  DUMP <offset> <count>

    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    // For simplicity, copy into a modifiable buffer
    char tmp[CMD_BUF_SIZE];
    strncpy(tmp, line, CMD_BUF_SIZE - 1);
    tmp[CMD_BUF_SIZE - 1] = '\0';

    // First token: command
    char* cmd = strtok(tmp, " \t");
    if (!cmd) return;

    // Convert to uppercase
    for (char* p = cmd; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p = *p - 'a' + 'A';
    }

    if (strcmp(cmd, "HELLO") == 0) {
        cmdHello(storage);
    } else if (strcmp(cmd, "GET_STATE") == 0) {
        cmdGetState(storage);
    } else if (strcmp(cmd, "CLEAR") == 0) {
        cmdClear(storage);
    } else if (strcmp(cmd, "DUMP") == 0) {
        char* tokOffset = strtok(nullptr, " \t");
        char* tokCount  = strtok(nullptr, " \t");
        int offset = 0;
        int count  = 10;
        if (tokOffset) offset = atoi(tokOffset);
        if (tokCount)  count  = atoi(tokCount);
        cmdDump(storage, offset, count);
    } else {
        Serial.print("{\"event\":\"error\",\"msg\":\"unknown command: ");
        Serial.print(cmd);
        Serial.println("\"}");
    }
}

void UsbCommandHandler::cmdHello(Storage& storage) {
    char hexId[DEVICE_UID_HEX_LEN + 1];
    getDeviceUidHex(hexId);

    Serial.print("{\"event\":\"hello\"");
    Serial.print(",\"device_id\":\"");
    Serial.print(hexId);
    
    Serial.print("\",\"fw\":\"");
    Serial.print(FW_VERSION_STRING);

    Serial.print("\",\"build\":\"");
    Serial.print(FW_BUILD_DATE);
    Serial.print(" ");
    Serial.print(FW_BUILD_TIME);

    Serial.print("\",\"hash\":\"");
    Serial.print(FW_BUILD_HASH);
    Serial.print("\"}");

    Serial.println("}");
}

void UsbCommandHandler::cmdGetState(Storage& storage) {
    auto &st = storage.state();

    Serial.print("{\"event\":\"state\"");
    Serial.print(",\"totalTapCount\":");
    Serial.print(st.totalTapCount);
    Serial.print(",\"linkCount\":");
    Serial.print(st.linkCount);
    Serial.println("}");
}

void UsbCommandHandler::cmdClear(Storage& storage) {
    storage.clearAll();

    Serial.println("{\"event\":\"ack\",\"cmd\":\"CLEAR\"}");
}

void UsbCommandHandler::cmdDump(Storage& storage, int offset, int count) {
    auto &st = storage.state();

    if (offset < 0) offset = 0;
    if (count  < 0) count  = 0;

    if (offset >= (int)PersistPayloadV1::MAX_LINKS) {
        Serial.println("{\"event\":\"links\",\"items\":[]}");
        return;
    }

    int maxAvailable = st.linkCount;
    if (maxAvailable > (int)PersistPayloadV1::MAX_LINKS) {
        maxAvailable = PersistPayloadV1::MAX_LINKS;
    }

    int end = offset + count;
    if (end > maxAvailable) end = maxAvailable;

    Serial.print("{\"event\":\"links\",\"offset\":");
    Serial.print(offset);
    Serial.print(",\"count\":");
    Serial.print(end - offset);
    Serial.print(",\"items\":[");
    bool first = true;

    for (int i = offset; i < end; ++i) {
        if (!first) Serial.print(",");
        first = false;

        Serial.print("{\"peer\":\"");
        printHex(st.links[i].peerId, DEVICE_UID_LEN);
        Serial.print("\"}");
    }

    Serial.println("]}");
}

void UsbCommandHandler::printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
    }
}
