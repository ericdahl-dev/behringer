#include "unity.h"
#include "bpm_publisher.h"

void setUp(void)    { bpm_publisher_init(0.5f, 500, 1); }
void tearDown(void) {}

void test_change_past_threshold_sends(void) {
    PublishDecision d = bpm_publisher_step(120.0f, true, 1000);
    TEST_ASSERT_TRUE(d.send);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, d.bpm);
}

void test_jitter_below_threshold_does_not_send(void) {
    bpm_publisher_step(120.0f, true, 1000);               // establishes 120
    PublishDecision d = bpm_publisher_step(120.3f, true, 2000);  // +0.3 < 0.5
    TEST_ASSERT_FALSE(d.send);
}

void test_send_interval_throttles(void) {
    bpm_publisher_step(120.0f, true, 1000);               // sends
    PublishDecision d = bpm_publisher_step(130.0f, true, 1200);  // change, but 200ms < 500
    TEST_ASSERT_FALSE(d.send);
}

void test_zero_bpm_never_sends(void) {
    bpm_publisher_step(120.0f, true, 1000);
    PublishDecision d = bpm_publisher_step(0.0f, true, 5000);    // no signal
    TEST_ASSERT_FALSE(d.send);
}

void test_periodic_refresh_after_a_bar(void) {
    bpm_publisher_step(120.0f, true, 1000);               // send; bar = 4*60000/120 = 2000ms
    PublishDecision d = bpm_publisher_step(120.0f, true, 3001);  // no change, bar elapsed
    TEST_ASSERT_TRUE(d.send);
    TEST_ASSERT_TRUE(d.refresh);
}

void test_inactive_suppresses_refresh(void) {
    bpm_publisher_step(120.0f, true, 1000);
    PublishDecision d = bpm_publisher_step(120.0f, false, 3001); // bar elapsed but inactive
    TEST_ASSERT_FALSE(d.send);
}

void test_change_sends_even_when_inactive(void) {
    PublishDecision d = bpm_publisher_step(120.0f, false, 1000); // a real change always emits
    TEST_ASSERT_TRUE(d.send);
    TEST_ASSERT_FALSE(d.refresh);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_change_past_threshold_sends);
    RUN_TEST(test_jitter_below_threshold_does_not_send);
    RUN_TEST(test_send_interval_throttles);
    RUN_TEST(test_zero_bpm_never_sends);
    RUN_TEST(test_periodic_refresh_after_a_bar);
    RUN_TEST(test_inactive_suppresses_refresh);
    RUN_TEST(test_change_sends_even_when_inactive);
    return UNITY_END();
}
