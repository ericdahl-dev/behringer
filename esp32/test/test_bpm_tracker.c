#include "unity.h"
#include "bpm_tracker.h"

void setUp(void)    {}
void tearDown(void) {}

void test_no_change_below_threshold(void) {
    bpm_tracker_init(120.0f, 0.5f);
    TEST_ASSERT_FALSE(bpm_tracker_update(120.3f));
}

void test_change_at_exact_threshold(void) {
    bpm_tracker_init(120.0f, 0.5f);
    TEST_ASSERT_TRUE(bpm_tracker_update(120.5f));
}

void test_change_above_threshold(void) {
    bpm_tracker_init(120.0f, 0.5f);
    TEST_ASSERT_TRUE(bpm_tracker_update(130.0f));
}

void test_change_on_decrease(void) {
    bpm_tracker_init(120.0f, 0.5f);
    TEST_ASSERT_TRUE(bpm_tracker_update(119.0f));
}

void test_get_returns_updated_bpm(void) {
    bpm_tracker_init(120.0f, 0.5f);
    bpm_tracker_update(130.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 130.0f, bpm_tracker_get());
}

void test_get_unchanged_when_below_threshold(void) {
    bpm_tracker_init(120.0f, 0.5f);
    bpm_tracker_update(120.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 120.0f, bpm_tracker_get());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_change_below_threshold);
    RUN_TEST(test_change_at_exact_threshold);
    RUN_TEST(test_change_above_threshold);
    RUN_TEST(test_change_on_decrease);
    RUN_TEST(test_get_returns_updated_bpm);
    RUN_TEST(test_get_unchanged_when_below_threshold);
    return UNITY_END();
}
