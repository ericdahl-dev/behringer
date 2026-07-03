#include "unity.h"
#include "wifi_down_blink.h"

void setUp(void)    {}
void tearDown(void) {}

// Connected -> never due, regardless of elapsed time.
void test_not_due_when_connected(void) {
    TEST_ASSERT_FALSE(wifi_down_blink_due(10000, 0, 4000, true));
    TEST_ASSERT_FALSE(wifi_down_blink_due(99999, 0, 4000, true));
}

// Disconnected but interval hasn't elapsed yet -> not due.
void test_not_due_before_interval(void) {
    TEST_ASSERT_FALSE(wifi_down_blink_due(3999, 0, 4000, false));
    TEST_ASSERT_FALSE(wifi_down_blink_due(5000, 1500, 4000, false));  // elapsed 3500
}

// Disconnected and interval has elapsed (boundary: exactly at interval
// counts as due, not strictly greater).
void test_due_after_interval_elapsed(void) {
    TEST_ASSERT_TRUE(wifi_down_blink_due(4000, 0, 4000, false));
    TEST_ASSERT_TRUE(wifi_down_blink_due(5000, 0, 4000, false));
    TEST_ASSERT_TRUE(wifi_down_blink_due(6000, 1500, 4000, false));  // elapsed 4500
}

// millis() rollover (~49.7 days uptime): unsigned subtraction must still
// give the right answer when now_ms has wrapped past last_blink_ms.
void test_due_calc_survives_millis_rollover(void) {
    uint32_t last = 0xFFFFFFF0u;       // 16 ticks before rollover
    TEST_ASSERT_FALSE(wifi_down_blink_due(100u, last, 4000, false));  // elapsed 116
    TEST_ASSERT_TRUE(wifi_down_blink_due(4000u, last, 4000, false));  // elapsed 4016
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_not_due_when_connected);
    RUN_TEST(test_not_due_before_interval);
    RUN_TEST(test_due_after_interval_elapsed);
    RUN_TEST(test_due_calc_survives_millis_rollover);
    return UNITY_END();
}
