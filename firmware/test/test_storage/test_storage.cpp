// =====================================================
// Storage Unit Tests
// =====================================================
// Unit tests for MockStorage and storage operations.
//
// Run with: pio test -e native
// =====================================================

#include <unity.h>
#include "../mock_storage.h"

// =====================================================
// Test Fixtures
// =====================================================

MockStorage storage;

void setUp() {
    // Reset storage before each test
    storage.reset();
}

void tearDown() {
    // Nothing to clean up
}

// =====================================================
// Test Cases
// =====================================================

void test_initial_state() {
    TEST_ASSERT_FALSE(storage.hasSecretKey());
    TEST_ASSERT_EQUAL(0, storage.getKeyVersion());
    TEST_ASSERT_EQUAL(0, storage.state().totalTapCount);
    TEST_ASSERT_EQUAL(0, storage.state().linkCount);
    TEST_ASSERT_FALSE(storage.isDirty());
}

void test_add_link_new() {
    uint8_t peerId[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    
    // First add should succeed
    TEST_ASSERT_TRUE(storage.addLink(peerId));
    TEST_ASSERT_EQUAL(1, storage.state().linkCount);
    TEST_ASSERT_TRUE(storage.hasLink(peerId));
    TEST_ASSERT_TRUE(storage.isDirty());
}

void test_add_link_duplicate() {
    uint8_t peerId[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    
    // First add succeeds
    TEST_ASSERT_TRUE(storage.addLink(peerId));
    storage.reset();  // Clear dirty flag but keep data
    storage.state().linkCount = 1;  // Restore link count
    memcpy(storage.state().links[0].peerId, peerId, 12);
    
    // Adding same link again should fail (duplicate)
    TEST_ASSERT_FALSE(storage.addLink(peerId));
}

void test_add_multiple_links() {
    uint8_t peerId1[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t peerId2[12] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};
    
    TEST_ASSERT_TRUE(storage.addLink(peerId1));
    TEST_ASSERT_TRUE(storage.addLink(peerId2));
    
    TEST_ASSERT_EQUAL(2, storage.state().linkCount);
    TEST_ASSERT_TRUE(storage.hasLink(peerId1));
    TEST_ASSERT_TRUE(storage.hasLink(peerId2));
}

void test_increment_tap_count() {
    TEST_ASSERT_EQUAL(0, storage.state().totalTapCount);
    TEST_ASSERT_FALSE(storage.incrementTapCountCalled);
    
    storage.incrementTapCount();
    
    TEST_ASSERT_EQUAL(1, storage.state().totalTapCount);
    TEST_ASSERT_TRUE(storage.incrementTapCountCalled);
    TEST_ASSERT_TRUE(storage.isDirty());
    
    storage.incrementTapCount();
    TEST_ASSERT_EQUAL(2, storage.state().totalTapCount);
}

void test_set_secret_key() {
    uint8_t key[32];
    for (int i = 0; i < 32; i++) key[i] = i + 1;
    
    TEST_ASSERT_FALSE(storage.hasSecretKey());
    
    storage.setSecretKey(1, key);
    
    TEST_ASSERT_TRUE(storage.hasSecretKey());
    TEST_ASSERT_EQUAL(1, storage.getKeyVersion());
    TEST_ASSERT_EQUAL_MEMORY(key, storage.getSecretKey(), 32);
    TEST_ASSERT_TRUE(storage.isDirty());
}

void test_clear_all_resets_data() {
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

void test_save_tracking() {
    TEST_ASSERT_EQUAL(0, storage.saveNowCallCount);
    TEST_ASSERT_FALSE(storage.saveTapCountOnlyCalled);
    TEST_ASSERT_FALSE(storage.saveLinkOnlyCalled);
    
    storage.markDirty();
    TEST_ASSERT_TRUE(storage.isDirty());
    
    storage.saveTapCountOnly();
    TEST_ASSERT_TRUE(storage.saveTapCountOnlyCalled);
    TEST_ASSERT_FALSE(storage.isDirty());  // Save clears dirty
    
    storage.markDirty();
    storage.saveLinkOnly();
    TEST_ASSERT_TRUE(storage.saveLinkOnlyCalled);
    
    storage.markDirty();
    storage.saveNow();
    TEST_ASSERT_EQUAL(1, storage.saveNowCallCount);
}

void test_begin_tracking() {
    TEST_ASSERT_FALSE(storage.beginCalled);
    
    bool result = storage.begin();
    
    TEST_ASSERT_TRUE(storage.beginCalled);
    TEST_ASSERT_TRUE(result);  // Default result is true
    
    // Test begin failure
    storage.reset();
    storage.beginResult = false;
    TEST_ASSERT_FALSE(storage.begin());
}

void test_loop_tracking() {
    TEST_ASSERT_EQUAL(0, storage.loopCallCount);
    
    storage.loop();
    TEST_ASSERT_EQUAL(1, storage.loopCallCount);
    
    storage.loop();
    storage.loop();
    TEST_ASSERT_EQUAL(3, storage.loopCallCount);
}

// =====================================================
// Test Runner
// =====================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_initial_state);
    RUN_TEST(test_add_link_new);
    RUN_TEST(test_add_link_duplicate);
    RUN_TEST(test_add_multiple_links);
    RUN_TEST(test_increment_tap_count);
    RUN_TEST(test_set_secret_key);
    RUN_TEST(test_clear_all_resets_data);
    RUN_TEST(test_save_tracking);
    RUN_TEST(test_begin_tracking);
    RUN_TEST(test_loop_tracking);
    
    return UNITY_END();
}

