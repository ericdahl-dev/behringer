#include "unity.h"
#include "led_phase.h"

void setUp(void)    {}
void tearDown(void) {}

// Not valid -> never flash, regardless of prev/phase values.
void test_invalid_never_flashes(void) {
    TEST_ASSERT_FALSE(led_phase_should_flash(0.9f, 0.1f, false));
    TEST_ASSERT_FALSE(led_phase_should_flash(-1.0f, 0.0f, false));
}

// First reading since (re)sync (prev_phase < 0) -> seed only, no flash,
// even if phase is small (would look like a wrap if compared naively).
void test_first_reading_since_sync_does_not_flash(void) {
    TEST_ASSERT_FALSE(led_phase_should_flash(-1.0f, 0.02f, true));
    TEST_ASSERT_FALSE(led_phase_should_flash(-1.0f, 0.99f, true));
}

// Normal within-beat progression (phase increasing) -> no flash.
void test_phase_increasing_does_not_flash(void) {
    TEST_ASSERT_FALSE(led_phase_should_flash(0.10f, 0.15f, true));
    TEST_ASSERT_FALSE(led_phase_should_flash(0.0f, 0.01f, true));
}

// Phase dropped from near-1 back to near-0 -> wrap detected -> flash.
void test_phase_wrap_flashes(void) {
    TEST_ASSERT_TRUE(led_phase_should_flash(0.97f, 0.02f, true));
    TEST_ASSERT_TRUE(led_phase_should_flash(0.999f, 0.0f, true));
}

// Exactly equal phase (poll landed on the same reading twice) -> not a
// wrap (phase < prev_phase is strict).
void test_equal_phase_does_not_flash(void) {
    TEST_ASSERT_FALSE(led_phase_should_flash(0.5f, 0.5f, true));
}

// prev_phase exactly 0.0 is a valid prior reading (not the -1.0f sentinel).
void test_prev_phase_zero_is_a_real_reading(void) {
    TEST_ASSERT_TRUE(led_phase_should_flash(0.0f, -0.0001f, true));  // jitter dip below 0 reads as a wrap
    TEST_ASSERT_FALSE(led_phase_should_flash(0.0f, 0.0f, true));
    TEST_ASSERT_FALSE(led_phase_should_flash(0.0f, 0.5f, true));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_invalid_never_flashes);
    RUN_TEST(test_first_reading_since_sync_does_not_flash);
    RUN_TEST(test_phase_increasing_does_not_flash);
    RUN_TEST(test_phase_wrap_flashes);
    RUN_TEST(test_equal_phase_does_not_flash);
    RUN_TEST(test_prev_phase_zero_is_a_real_reading);
    return UNITY_END();
}
