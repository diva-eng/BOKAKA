#include "storage.h"
#include "platform_storage.h"
#include "platform_timing.h"
#include "stm32l0xx_hal.h"


Storage::Storage() {}


bool Storage::begin() {
    // Initialize platform storage
    if (!platform_storage_begin(STORAGE_EEPROM_SIZE)) {
        return false;
    }
    
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
        // If selfId is uninitialized (all zeros), fill it in
        if (isUidAllZero(_image.payload.selfId)) {
            getDeviceUidRaw(_image.payload.selfId);
            markDirty();
            saveNow();
        }
    }

    _dirty = false;
    _lastSaveMs = platform_millis();
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

    uint32_t now = platform_millis();
    if (now - _lastSaveMs >= STORAGE_DELAYED_WRITE_MS) {
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
        _lastSaveMs = platform_millis();
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


bool Storage::hasLink(const uint8_t peerId[DEVICE_UID_LEN]) const {
    const PersistPayloadV1& p = _image.payload;
    
    uint16_t count = p.linkCount;
    if (count > PersistPayloadV1::MAX_LINKS) {
        count = PersistPayloadV1::MAX_LINKS;
    }
    
    for (uint16_t i = 0; i < count; i++) {
        if (memcmp(p.links[i].peerId, peerId, DEVICE_UID_LEN) == 0) {
            return true;
        }
    }
    return false;
}

bool Storage::addLink(const uint8_t peerId[DEVICE_UID_LEN]) {
    if (hasLink(peerId)) {
        return false;
    }
    
    PersistPayloadV1& p = _image.payload;

    uint16_t idx = p.linkCount;
    if (idx >= PersistPayloadV1::MAX_LINKS) {
        idx = idx % PersistPayloadV1::MAX_LINKS;
        _linkCountChanged = false;
    } else {
        p.linkCount++;
        _linkCountChanged = true;
    }

    memcpy(p.links[idx].peerId, peerId, DEVICE_UID_LEN);
    _lastLinkIndex = idx;

    markDirty();
    return true;
}

void Storage::incrementTapCount() {
    _image.payload.totalTapCount++;
    markDirty();
}

void Storage::saveTapCountOnly() {
    // Recalculate CRC
    _image.header.crc32 = calcCrc32(
        reinterpret_cast<uint8_t*>(&_image.payload),
        sizeof(PersistPayloadV1)
    );
    
    constexpr size_t tapCountOffset = offsetof(PersistImageV1, payload) + 
                                      offsetof(PersistPayloadV1, totalTapCount);
    constexpr size_t crcOffset = offsetof(PersistImageV1, header) + 
                                 offsetof(PersistHeader, crc32);
    
    uint8_t* tapCountPtr = reinterpret_cast<uint8_t*>(&_image.payload.totalTapCount);
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        platform_storage_write(STORAGE_EEPROM_BASE + tapCountOffset + i, tapCountPtr[i]);
    }
    
    uint8_t* crcPtr = reinterpret_cast<uint8_t*>(&_image.header.crc32);
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        platform_storage_write(STORAGE_EEPROM_BASE + crcOffset + i, crcPtr[i]);
    }
    
    platform_storage_commit();
    
    _dirty = false;
    _lastSaveMs = platform_millis();
}

void Storage::saveLinkOnly() {
    // Recalculate CRC
    _image.header.crc32 = calcCrc32(
        reinterpret_cast<uint8_t*>(&_image.payload),
        sizeof(PersistPayloadV1)
    );
    
    constexpr size_t linkCountOffset = offsetof(PersistImageV1, payload) + 
                                       offsetof(PersistPayloadV1, linkCount);
    constexpr size_t linksArrayOffset = offsetof(PersistImageV1, payload) + 
                                        offsetof(PersistPayloadV1, links);
    constexpr size_t crcOffset = offsetof(PersistImageV1, header) + 
                                 offsetof(PersistHeader, crc32);
    
    size_t linkEntryOffset = linksArrayOffset + (_lastLinkIndex * sizeof(LinkRecordV1));
    
    if (_linkCountChanged) {
        uint8_t* linkCountPtr = reinterpret_cast<uint8_t*>(&_image.payload.linkCount);
        for (size_t i = 0; i < sizeof(uint16_t); i++) {
            platform_storage_write(STORAGE_EEPROM_BASE + linkCountOffset + i, linkCountPtr[i]);
        }
    }
    
    uint8_t* linkPtr = _image.payload.links[_lastLinkIndex].peerId;
    for (size_t i = 0; i < DEVICE_UID_LEN; i++) {
        platform_storage_write(STORAGE_EEPROM_BASE + linkEntryOffset + i, linkPtr[i]);
    }
    
    uint8_t* crcPtr = reinterpret_cast<uint8_t*>(&_image.header.crc32);
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        platform_storage_write(STORAGE_EEPROM_BASE + crcOffset + i, crcPtr[i]);
    }
    
    platform_storage_commit();
    
    _dirty = false;
    _linkCountChanged = false;
    _lastSaveMs = platform_millis();
}

// per-device key

bool Storage::hasSecretKey() const {
    if (_image.payload.keyVersion == 0) return false;
    for (size_t i = 0; i < 32; ++i) {
        if (_image.payload.secretKey[i] != 0) return true;
    }
    return false;
}

const uint8_t* Storage::getSecretKey() const {
    return _image.payload.secretKey;
}

uint8_t Storage::getKeyVersion() const {
    return _image.payload.keyVersion;
}

void Storage::setSecretKey(uint8_t version, const uint8_t key[32]) {
    _image.payload.keyVersion = version;
    memcpy(_image.payload.secretKey, key, 32);
    markDirty();
    saveNow();  // Immediate save for critical security data
}

// =====================================================
// NVM I/O
// =====================================================

bool Storage::loadFromNvm() {
    if (sizeof(PersistImageV1) > STORAGE_EEPROM_SIZE) return false;

    PersistImageV1 temp;

    for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
        ((uint8_t*)&temp)[i] = platform_storage_read(STORAGE_EEPROM_BASE + i);
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
    // Write in chunks with periodic yields
    const size_t CHUNK_SIZE = 32;
    for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
        platform_storage_write(STORAGE_EEPROM_BASE + i, ((uint8_t*)&_image)[i]);
        
        if ((i % CHUNK_SIZE) == (CHUNK_SIZE - 1)) {
            platform_delay_ms(1);
        }
    }
    
    platform_storage_commit();
    
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
