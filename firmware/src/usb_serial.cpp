#include "usb_serial.h"
#include "fw_config.h"
#include "platform_serial.h"
#include "platform_timing.h"
#include "mbedtls/md.h"
#include <cstdlib>  // for atoi

// ========= util =========
bool UsbCommandHandler::hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
    size_t len = strlen(hex);
    if (len != outLen * 2) return false;

    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        return -1;
    };

    for (size_t i = 0; i < outLen; ++i) {
        int hi = hexVal(hex[2*i]);
        int lo = hexVal(hex[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (hi << 4) | lo;
    }
    return true;
}

void UsbCommandHandler::bytesToHex(const uint8_t* in, size_t len, char* out) {
    auto hexDigit = [](uint8_t v) -> char {
        return (v < 10) ? ('0' + v) : ('A' + (v - 10));
    };
    for (size_t i = 0; i < len; ++i) {
        out[2*i]   = hexDigit((in[i] >> 4) & 0x0F);
        out[2*i+1] = hexDigit(in[i] & 0x0F);
    }
    out[2*len] = '\0';
}

void UsbCommandHandler::printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        platform_serial_print_hex(data[i]);
    }
}

void UsbCommandHandler::begin(unsigned long baud)
{
    platform_serial_begin(baud);
    
    // On STM32 USB Serial (CDC), we need to wait for the USB connection to be fully established
    // and flush any initialization noise from the buffer. This prevents the first command
    // from being lost or corrupted.
    
    // Wait for USB Serial to be ready (STM32 CDC needs time to enumerate)
    // Give it time to establish the USB connection
    platform_delay_ms(500);
    
    // Flush any initialization noise or leftover data from the input buffer
    // This is critical - without this, the first command can be corrupted or lost
    // Some STM32 USB CDC implementations may have initialization data or noise
    uint32_t flushStart = platform_millis();
    while (platform_serial_available() > 0 && (platform_millis() - flushStart < 100)) {
        platform_serial_read();
        platform_delay_ms(1);  // Small delay to allow buffer to refill if needed
    }
    
    // Additional delay to ensure USB CDC is fully ready after flushing
    platform_delay_ms(100);
    
    // Reset command buffer state
    _len = 0;
}

// Call inside loop()
void UsbCommandHandler::poll(Storage &storage)
{
    while (platform_serial_available() > 0)
    {
        int c = platform_serial_read();
        if (c < 0) break;  // No data available

        // Support \r\n / \n as line endings
        if (c == '\r')
            continue;

        if (c == '\n')
        {
            _buf[_len] = '\0';
            if (_len > 0)
            {
                handleLine(storage, _buf);
            }
            _len = 0;
        }
        else
        {
            if (_len < CMD_BUF_SIZE - 1)
            {
                _buf[_len++] = c;
            }
            else
            {
                // Too long — discard this line
                _len = 0;
            }
        }
    }
}

void UsbCommandHandler::handleLine(Storage &storage, const char *line)
{
    // Simple split by spaces to recognize commands
    // Support (case-insensitive):
    //  HELLO
    //  GET_STATE
    //  CLEAR
    //  DUMP <offset> <count>

    // Skip leading whitespace
    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == '\0')
        return;

    // For simplicity, copy into a modifiable buffer
    char tmp[CMD_BUF_SIZE];
    strncpy(tmp, line, CMD_BUF_SIZE - 1);
    tmp[CMD_BUF_SIZE - 1] = '\0';

    // First token: command
    char *cmd = strtok(tmp, " \t");
    if (!cmd)
        return;

    // Convert to uppercase
    for (char *p = cmd; *p; ++p)
    {
        if (*p >= 'a' && *p <= 'z')
            *p = *p - 'a' + 'A';
    }

    if (strcmp(cmd, "HELLO") == 0)
    {
        cmdHello(storage);
    }
    else if (strcmp(cmd, "GET_STATE") == 0)
    {
        cmdGetState(storage);
    }
    else if (strcmp(cmd, "CLEAR") == 0)
    {
        cmdClear(storage);
    }
    else if (strcmp(cmd, "DUMP") == 0)
    {
        char *tokOffset = strtok(nullptr, " \t");
        char *tokCount = strtok(nullptr, " \t");
        int offset = 0;
        int count = 10;
        if (tokOffset)
            offset = atoi(tokOffset);
        if (tokCount)
            count = atoi(tokCount);
        cmdDump(storage, offset, count);
    }
     else if (strcmp(cmd, "PROVISION_KEY") == 0) {
        char* tokVer = strtok(nullptr, " \t");
        char* tokKey = strtok(nullptr, " \t");
        if (!tokVer || !tokKey) {
            platform_serial_println("{\"event\":\"error\",\"msg\":\"PROVISION_KEY args\"}");
            platform_serial_flush();
            return;
        }
        int ver = atoi(tokVer);
        cmdProvisionKey(storage, ver, tokKey);
    } else if (strcmp(cmd, "SIGN_STATE") == 0) {
        char* tokNonce = strtok(nullptr, " \t");
        if (!tokNonce) {
            platform_serial_println("{\"event\":\"error\",\"msg\":\"SIGN_STATE args\"}");
            platform_serial_flush();
            return;
        }
        cmdSignState(storage, tokNonce);
#ifdef ENABLE_TEST_COMMANDS
    } else if (strcmp(cmd, "GET_KEY") == 0) {
        cmdGetKey(storage);
#endif
    }
    else
    {
        platform_serial_print("{\"event\":\"error\",\"msg\":\"unknown command: ");
        platform_serial_print(cmd);
        platform_serial_println("\"}");
        platform_serial_flush();
    }
}

void UsbCommandHandler::cmdHello(Storage &storage)
{
    char hexId[DEVICE_UID_HEX_LEN + 1];
    getDeviceUidHex(hexId);

    platform_serial_print("{\"event\":\"hello\"");
    platform_serial_print(",\"device_id\":\"");
    platform_serial_print(hexId);

    platform_serial_print("\",\"fw\":\"");
    platform_serial_print(FW_VERSION_STRING);

    platform_serial_print("\",\"build\":\"");
    platform_serial_print(FW_BUILD_DATE);
    platform_serial_print(" ");
    platform_serial_print(FW_BUILD_TIME);

    platform_serial_print("\",\"hash\":\"");
    platform_serial_print(FW_BUILD_HASH);
    platform_serial_println("\"}");
    platform_serial_flush();
}

void UsbCommandHandler::cmdGetState(Storage &storage)
{
    auto &st = storage.state();

    platform_serial_print("{\"event\":\"state\"");
    platform_serial_print(",\"totalTapCount\":");
    platform_serial_print((uint32_t)st.totalTapCount);
    platform_serial_print(",\"linkCount\":");
    platform_serial_print((uint32_t)st.linkCount);
    platform_serial_println("}");
    platform_serial_flush();
}

void UsbCommandHandler::cmdClear(Storage &storage)
{
    // Acknowledge immediately before performing potentially blocking NVM writes
    platform_serial_println("{\"event\":\"ack\",\"cmd\":\"CLEAR\"}");
    platform_serial_flush();
    platform_delay_ms(10);

    storage.clearAll();
}

void UsbCommandHandler::cmdDump(Storage &storage, int offset, int count)
{
    auto &st = storage.state();

    if (offset < 0)
        offset = 0;
    if (count < 0)
        count = 0;

    if (offset >= (int)PersistPayloadV1::MAX_LINKS)
    {
        platform_serial_println("{\"event\":\"links\",\"items\":[]}");
        platform_serial_flush();
        return;
    }

    int maxAvailable = st.linkCount;
    if (maxAvailable > (int)PersistPayloadV1::MAX_LINKS)
    {
        maxAvailable = PersistPayloadV1::MAX_LINKS;
    }

    int end = offset + count;
    if (end > maxAvailable)
        end = maxAvailable;

    platform_serial_print("{\"event\":\"links\",\"offset\":");
    platform_serial_print(offset);
    platform_serial_print(",\"count\":");
    platform_serial_print(end - offset);
    platform_serial_print(",\"items\":[");
    bool first = true;

    for (int i = offset; i < end; ++i)
    {
        if (!first)
            platform_serial_print(",");
        first = false;

        platform_serial_print("{\"peer\":\"");
        printHex(st.links[i].peerId, DEVICE_UID_LEN);
        platform_serial_print("\"}");
    }

    platform_serial_println("]}");
    platform_serial_flush();
}

void UsbCommandHandler::cmdProvisionKey(Storage& storage, int version, const char* keyHex) {
        if (version <= 0 || version > 255) {
            platform_serial_println("{\"event\":\"error\",\"msg\":\"invalid keyVersion\"}");
            platform_serial_flush();
            return;
        }

    uint8_t key[32];
    if (!hexToBytes(keyHex, key, 32)) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"invalid key hex\"}");
        platform_serial_flush();
        return;
    }

    // Acknowledge the provision command before performing the EEPROM write
    platform_serial_print("{\"event\":\"ack\",\"cmd\":\"PROVISION_KEY\",\"keyVersion\":");
    platform_serial_print(version);
    platform_serial_println("}");
    platform_serial_flush();
    platform_delay_ms(10);

    storage.setSecretKey((uint8_t)version, key);
}

#ifdef ENABLE_TEST_COMMANDS
void UsbCommandHandler::cmdGetKey(Storage& storage) {
    if (!storage.hasSecretKey()) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"no_key\"}");
        platform_serial_flush();
        return;
    }
    const uint8_t* key = storage.getSecretKey();
    char hex[65];
    bytesToHex(key, 32, hex);
    platform_serial_print("{\"event\":\"key\",\"keyVersion\":");
    platform_serial_print((uint32_t)storage.getKeyVersion());
    platform_serial_print(",\"key\":\"");
    platform_serial_print(hex);
    platform_serial_println("\"}");
    platform_serial_flush();
}
#endif

void UsbCommandHandler::cmdSignState(Storage& storage, const char* nonceHex) {
    if (!storage.hasSecretKey()) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"no_key\"}");
        platform_serial_flush();
        return;
    }

    // Parse nonce (variable length, must be even and no more than 32 bytes)
    size_t nonceHexLen = strlen(nonceHex);
    if (nonceHexLen == 0 || (nonceHexLen % 2) != 0 || nonceHexLen > 64) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"invalid nonce\"}");
        platform_serial_flush();
        return;
    }
    size_t nonceLen = nonceHexLen / 2;

    uint8_t nonce[32];
    if (!hexToBytes(nonceHex, nonce, nonceLen)) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"invalid nonce hex\"}");
        platform_serial_flush();
        return;
    }

    auto &st = storage.state();
    const uint8_t* key = storage.getSecretKey();
    uint8_t keyVersion = storage.getKeyVersion();

    // Construct the message to be signed:
    // msg = selfId(12) + nonce(N) + totalTapCount(4 LE) + linkCount(2 LE) + each peerId(12)
    const size_t maxMsgLen =
        DEVICE_UID_LEN          // selfId
        + 32                    // max nonce
        + 4                     // totalTapCount
        + 2                     // linkCount
        + (PersistPayloadV1::MAX_LINKS * DEVICE_UID_LEN); // links

    uint8_t msg[maxMsgLen];
    size_t pos = 0;

    // selfId
    memcpy(msg + pos, st.selfId, DEVICE_UID_LEN);
    pos += DEVICE_UID_LEN;

    // nonce
    memcpy(msg + pos, nonce, nonceLen);
    pos += nonceLen;

    // totalTapCount (little-endian)
    uint32_t t = st.totalTapCount;
    msg[pos++] = (uint8_t)(t & 0xFF);
    msg[pos++] = (uint8_t)((t >> 8) & 0xFF);
    msg[pos++] = (uint8_t)((t >> 16) & 0xFF);
    msg[pos++] = (uint8_t)((t >> 24) & 0xFF);

    // linkCount (little-endian, 2 bytes)
    uint16_t lc = st.linkCount;
    if (lc > PersistPayloadV1::MAX_LINKS) lc = PersistPayloadV1::MAX_LINKS;
    msg[pos++] = (uint8_t)(lc & 0xFF);
    msg[pos++] = (uint8_t)((lc >> 8) & 0xFF);

    // links peerId
    for (uint16_t i = 0; i < lc; ++i) {
        memcpy(msg + pos, st.links[i].peerId, DEVICE_UID_LEN);
        pos += DEVICE_UID_LEN;
    }

    // HMAC-SHA256
    uint8_t hmac[32];
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"md_info\"}");
        platform_serial_flush();
        return;
    }
    int ret = mbedtls_md_hmac(info, key, 32, msg, pos, hmac);
    if (ret != 0) {
        platform_serial_println("{\"event\":\"error\",\"msg\":\"hmac_failed\"}");
        platform_serial_flush();
        return;
    }

    // Output in hex form
    char hmacHex[64 + 1];
    bytesToHex(hmac, 32, hmacHex);

    char devHex[DEVICE_UID_HEX_LEN + 1];
    // Note: here we use the snapshot's selfId for hex conversion or directly use hardware UID — both sides must agree
    // For simplicity, use selfId:
    bytesToHex(st.selfId, DEVICE_UID_LEN, devHex);

    platform_serial_print("{\"event\":\"SIGNED_STATE\"");
    platform_serial_print(",\"device_id\":\"");
    platform_serial_print(devHex);
    platform_serial_print("\",\"nonce\":\"");
    platform_serial_print(nonceHex);
    platform_serial_print("\",\"totalTapCount\":");
    platform_serial_print((uint32_t)st.totalTapCount);
    platform_serial_print(",\"linkCount\":");
    platform_serial_print((uint32_t)lc);
    platform_serial_print(",\"keyVersion\":");
    platform_serial_print((uint32_t)keyVersion);
    platform_serial_print(",\"hmac\":\"");
    platform_serial_print(hmacHex);
    platform_serial_println("\"}");
    platform_serial_flush();
}
