// =====================================================
// Storage Unit Tests
// =====================================================
// Example unit tests using MockStorage.
// These tests can run on a host machine (not embedded).
//
// To run with PlatformIO:
//   pio test -e native
//
// Requires adding a [env:native] section to platformio.ini.
// =====================================================

#ifdef UNIT_TEST  // Only compile when running tests

#include <unity.h>
#include "mock_storage.h"

// =====================================================
// Test: MockStorage Basic Operations
// =====================================================

void test_mock_storage_initial_state() {
    MockStorage storage;
    
    TEST_ASSERT_FALSE(storage.hasSecretKey());
    TEST_ASSERT_EQUAL(0, storage.getKeyVersion());
    TEST_ASSERT_EQUAL(0, storage.state().totalTapCount);
    TEST_ASSERT_EQUAL(0, storage.state().linkCount);
}

void test_mock_storage_add_link() {
    MockStorage storage;
    
    uint8_t peerId1[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t peerId2[12] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    
    // First add should succeed
    TEST_ASSERT_TRUE(storage.addLink(peerId1));
    TEST_ASSERT_EQUAL(1, storage.state().linkCount);
    TEST_ASSERT_TRUE(storage.hasLink(peerId1));
    
    // Adding same link again should fail (duplicate)
    TEST_ASSERT_FALSE(storage.addLink(peerId1));
    TEST_ASSERT_EQUAL(1, storage.state().linkCount);
    
    // Adding different link should succeed
    TEST_ASSERT_TRUE(storage.addLink(peerId2));
    TEST_ASSERT_EQUAL(2, storage.state().linkCount);
    TEST_ASSERT_TRUE(storage.hasLink(peerId2));
}

void test_mock_storage_increment_tap_count() {
    MockStorage storage;
    
    TEST_ASSERT_EQUAL(0, storage.state().totalTapCount);
    TEST_ASSERT_FALSE(storage.incrementTapCountCalled);
    
    storage.incrementTapCount();
    
    TEST_ASSERT_EQUAL(1, storage.state().totalTapCount);
    TEST_ASSERT_TRUE(storage.incrementTapCountCalled);
    TEST_ASSERT_TRUE(storage.isDirty());
}

void test_mock_storage_set_secret_key() {
    MockStorage storage;
    
    uint8_t key[32];
    for (int i = 0; i < 32; i++) key[i] = i + 1;
    
    TEST_ASSERT_FALSE(storage.hasSecretKey());
    
    storage.setSecretKey(1, key);
    
    TEST_ASSERT_TRUE(storage.hasSecretKey());
    TEST_ASSERT_EQUAL(1, storage.getKeyVersion());
    TEST_ASSERT_EQUAL_MEMORY(key, storage.getSecretKey(), 32);
}

void test_mock_storage_clear_all() {
    MockStorage storage;
    
    // Add some data
    uint8_t peerId[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t selfId[12] = {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111};
    
    storage.setSelfId(selfId);
    storage.addLink(peerId);
    storage.incrementTapCount();
    
    TEST_ASSERT_EQUAL(1, storage.state().linkCount);
    TEST_ASSERT_EQUAL(1, storage.state().totalTapCount);
    
    // Clear all
    storage.clearAll();
    
    // Links and tap count should be reset
    TEST_ASSERT_EQUAL(0, storage.state().linkCount);
    TEST_ASSERT_EQUAL(0, storage.state().totalTapCount);
    TEST_ASSERT_TRUE(storage.clearAllCalled);
    
    // Self ID should be preserved
    TEST_ASSERT_EQUAL_MEMORY(selfId, storage.state().selfId, 12);
}

void test_mock_storage_save_tracking() {
    MockStorage storage;
    
    TEST_ASSERT_EQUAL(0, storage.saveNowCallCount);
    TEST_ASSERT_FALSE(storage.saveTapCountOnlyCalled);
    TEST_ASSERT_FALSE(storage.saveLinkOnlyCalled);
    
    storage.saveTapCountOnly();
    TEST_ASSERT_TRUE(storage.saveTapCountOnlyCalled);
    
    storage.saveLinkOnly();
    TEST_ASSERT_TRUE(storage.saveLinkOnlyCalled);
    
    storage.saveNow();
    TEST_ASSERT_EQUAL(1, storage.saveNowCallCount);
}

// =====================================================
// Test Runner
// =====================================================

void run_storage_tests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_mock_storage_initial_state);
    RUN_TEST(test_mock_storage_add_link);
    RUN_TEST(test_mock_storage_increment_tap_count);
    RUN_TEST(test_mock_storage_set_secret_key);
    RUN_TEST(test_mock_storage_clear_all);
    RUN_TEST(test_mock_storage_save_tracking);
    
    UNITY_END();
}

#endif // UNIT_TEST

