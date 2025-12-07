#include "storage.h"
#include <EEPROM.h>
#include "stm32l0xx_hal.h"


Storage::Storage() {}


bool Storage::begin() {
    if (!loadFromNvm()) {
        // no valid data -> initialize blank version 1
        memset(&_image, 0, sizeof(_image));

        _image.header.magic   = STORAGE_MAGIC;
        _image.header.version = STORAGE_VERSION;
        _image.header.length  = sizeof(PersistPayloadV1);

        // Initialize selfId: take a snapshot from the hardware UID
        getDeviceUidRaw(_image.payload.selfId);

        _image.header.crc32 = calcCrc32(
            reinterpret_cast<uint8_t*>(&_image.payload),
            sizeof(PersistPayloadV1)
        );

        writeToNvm();
        } else {
        // Data successfully read from NVM
        // If the old data's selfId is still uninitialized (all zeros), fill it in once
        if (isUidAllZero(_image.payload.selfId)) {
            getDeviceUidRaw(_image.payload.selfId);
            markDirty();
            saveNow();
        }
    }

    _dirty = false;
    _lastSaveMs = millis();
    return true;
}

// =====================================================
// Public API
// =====================================================

void Storage::markDirty() {
    _dirty = true;
}

void Storage::loop() {
    if (!_dirty) return;

    uint32_t now = millis();
    if (now - _lastSaveMs >= 5000) {   // 5s delayed write
        saveNow();
    }
}

bool Storage::saveNow() {
    _image.header.magic   = STORAGE_MAGIC;
    _image.header.version = STORAGE_VERSION;
    _image.header.length  = sizeof(PersistPayloadV1);
    _image.header.crc32   = calcCrc32(
        reinterpret_cast<uint8_t*>(&_image.payload),
        sizeof(PersistPayloadV1)
    );

    bool ok = writeToNvm();
    if (ok) {
        _dirty = false;
        _lastSaveMs = millis();
    }
    return ok;
}


void Storage::clearAll() {
    uint8_t selfId[DEVICE_UID_LEN];
    memcpy(selfId, _image.payload.selfId, DEVICE_UID_LEN);

    memset(&_image.payload, 0, sizeof(_image.payload));
    memcpy(_image.payload.selfId, selfId, DEVICE_UID_LEN);

    markDirty();
    saveNow();
}


void Storage::addLink(const uint8_t peerId[DEVICE_UID_LEN]) {
    PersistPayloadV1& p = _image.payload;

    uint16_t idx = p.linkCount;
    if (idx >= PersistPayloadV1::MAX_LINKS) {
        idx = idx % PersistPayloadV1::MAX_LINKS;
    } else {
        p.linkCount++;
    }

    memcpy(p.links[idx].peerId, peerId, DEVICE_UID_LEN);
    p.totalTapCount++;

    markDirty();
}


// =====================================================
// NVM I/O
// =====================================================

bool Storage::loadFromNvm() {
    if (sizeof(PersistImageV1) > STORAGE_EEPROM_SIZE) return false;

    PersistImageV1 temp;

    for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
        ((uint8_t*)&temp)[i] = EEPROM.read(STORAGE_EEPROM_BASE + i);
    }

    if (temp.header.magic != STORAGE_MAGIC) return false;
    if (temp.header.version != STORAGE_VERSION) return false;
    if (temp.header.length  != sizeof(PersistPayloadV1)) return false;

    uint32_t crc = calcCrc32(
        reinterpret_cast<uint8_t*>(&temp.payload),
        sizeof(PersistPayloadV1)
    );
    if (crc != temp.header.crc32) return false;

    _image = temp;
    return true;
}


bool Storage::writeToNvm() {
    for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
        EEPROM.write(STORAGE_EEPROM_BASE + i, ((uint8_t*)&_image)[i]);
    }

    return true;
}


// =====================================================
// Hardware CRC32
// =====================================================

uint32_t Storage::calcCrc32(const uint8_t* data, size_t len) {
    if (len % 4 != 0)
        return 0;  // should never happen due to static_assert

    CRC_HandleTypeDef hcrc;
    hcrc.Instance = CRC;

    __HAL_RCC_CRC_CLK_ENABLE();
    HAL_CRC_DeInit(&hcrc);
    if (HAL_CRC_Init(&hcrc) != HAL_OK) {
        return 0;
    }

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)data, len / 4);

    __HAL_RCC_CRC_CLK_DISABLE();
    return crc;
}
