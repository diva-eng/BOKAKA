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
        // Data successfully read from NVM
        // If the old data's selfId is still uninitialized (all zeros), fill it in once
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
    // Use delayed write to batch multiple changes and reduce flash write cycles
    // This significantly extends flash memory lifetime by reducing write frequency
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
    // Clear is a user command that should be persisted immediately
    // This ensures the clear operation is not lost if power is lost
    // However, this does consume one flash write cycle
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
    // CRITICAL: Write immediately for secret key (provisioning operation)
    // This is intentionally NOT delayed because:
    // 1. It's critical security data that must be persisted immediately
    // 2. It happens rarely (once per device during provisioning)
    // 3. The immediate write ensures the key is saved even if power is lost
    // This one-time immediate write is acceptable for flash wear concerns
    saveNow();
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
    // FLASH WRITE CYCLE MANAGEMENT:
    // - Each call to writeToNvm() triggers a full page erase/write cycle
    // - STM32L0 flash has ~10,000 erase/write cycles per page
    // - Writing ~896 bytes counts as one write cycle regardless of how many bytes change
    // - We minimize writes by:
    //   1. Delayed writes (30s) to batch multiple changes
    //   2. Only writing when data actually changes (dirty flag)
    //   3. Immediate writes only for critical data (secret key provision)
    
    // Write all bytes - on STM32, each EEPROM.write() can take 5-10ms
    // Writing ~896 bytes can block for 6-7 seconds, which blocks serial processing!
    // This is why commands like CLEAR and PROVISION_KEY acknowledge BEFORE calling saveNow()
    
    // Write in chunks with periodic yields to allow serial interrupts to be processed
    const size_t CHUNK_SIZE = 32;  // Write 32 bytes at a time
    for (size_t i = 0; i < sizeof(PersistImageV1); ++i) {
        platform_storage_write(STORAGE_EEPROM_BASE + i, ((uint8_t*)&_image)[i]);
        
        // Every CHUNK_SIZE bytes, yield to allow serial processing
        // This prevents missing serial commands during long storage writes
        if ((i % CHUNK_SIZE) == (CHUNK_SIZE - 1)) {
            // Yield briefly - this allows serial RX interrupts to be processed
            // Serial.poll() can then read incoming commands
            platform_delay_ms(1);
        }
    }
    
    // Commit writes to persistent storage
    platform_storage_commit();
    
    // TODO: Consider implementing write cycle counting to track flash wear
    // TODO: For production, consider wear leveling across multiple flash pages
    
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
