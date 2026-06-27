#include "unity.h"
#include "midi_bpm_calc.h"

void setUp(void)    {}
void tearDown(void) {}

// MIDI clock = 24 PPQN. A WINDOW of 24 timestamps spans (WINDOW-1) intervals,
// scaled up to a full beat. At 120 BPM a beat = 500000 us, so one 24-PPQN
// interval = 500000/24 us and 23 intervals span = 60e6*23/(120*24) ≈ 479167 us.
void test_120_bpm(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.0f, midi_bpm_calc(479167u, 24));
}

void test_60_bpm(void) {
    // beat = 1000000 us → 23 intervals span = 60e6*23/(60*24) ≈ 958333 us
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, midi_bpm_calc(958333u, 24));
}

void test_180_bpm(void) {
    // 23 intervals span = 60e6*23/(180*24) ≈ 319444 us
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, midi_bpm_calc(319444u, 24));
}

void test_zero_span_returns_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, midi_bpm_calc(0u, 24));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_120_bpm);
    RUN_TEST(test_60_bpm);
    RUN_TEST(test_180_bpm);
    RUN_TEST(test_zero_span_returns_zero);
    return UNITY_END();
}
