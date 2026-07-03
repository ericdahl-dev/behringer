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

// --- midi_phase_calc / midi_phase_valid (LNK-019) ----------------------
// 24 PPQN: quantum=4 -> 96 pulses/bar. Pulse-count-modulo-bar is exact
// (no clock-sync estimation involved at all), independent of BPM.

void test_phase_calc_zero_pulses(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, midi_phase_calc(0u, 4));
}

void test_phase_calc_within_first_bar(void) {
    // 30 pulses into a 96-pulse bar -> 30/24 = 1.25 beats
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, midi_phase_calc(30u, 4));
}

void test_phase_calc_wraps_past_bar(void) {
    // 100 pulses, 96 pulses/bar -> 4 pulses into the next bar -> 4/24
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f / 24.0f, midi_phase_calc(100u, 4));
}

void test_phase_calc_exact_bar_boundary(void) {
    // exactly 2 bars (192 pulses) -> phase 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, midi_phase_calc(192u, 4));
}

void test_phase_calc_quantum_1(void) {
    // quantum=1 -> 24 pulses/bar; 12 pulses -> 0.5 beats
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, midi_phase_calc(12u, 1));
}

void test_phase_calc_invalid_quantum_returns_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, midi_phase_calc(50u, 0));
}

void test_phase_valid_false_before_first_bar(void) {
    // quantum=4 -> needs 96 pulses; 95 isn't enough yet
    TEST_ASSERT_FALSE(midi_phase_valid(95u, 4));
}

void test_phase_valid_true_at_exactly_one_bar(void) {
    TEST_ASSERT_TRUE(midi_phase_valid(96u, 4));
}

void test_phase_valid_true_past_one_bar(void) {
    TEST_ASSERT_TRUE(midi_phase_valid(200u, 4));
}

void test_phase_valid_false_zero_pulses(void) {
    TEST_ASSERT_FALSE(midi_phase_valid(0u, 4));
}

void test_phase_valid_false_invalid_quantum(void) {
    TEST_ASSERT_FALSE(midi_phase_valid(1000u, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_120_bpm);
    RUN_TEST(test_60_bpm);
    RUN_TEST(test_180_bpm);
    RUN_TEST(test_zero_span_returns_zero);
    RUN_TEST(test_phase_calc_zero_pulses);
    RUN_TEST(test_phase_calc_within_first_bar);
    RUN_TEST(test_phase_calc_wraps_past_bar);
    RUN_TEST(test_phase_calc_exact_bar_boundary);
    RUN_TEST(test_phase_calc_quantum_1);
    RUN_TEST(test_phase_calc_invalid_quantum_returns_zero);
    RUN_TEST(test_phase_valid_false_before_first_bar);
    RUN_TEST(test_phase_valid_true_at_exactly_one_bar);
    RUN_TEST(test_phase_valid_true_past_one_bar);
    RUN_TEST(test_phase_valid_false_zero_pulses);
    RUN_TEST(test_phase_valid_false_invalid_quantum);
    return UNITY_END();
}
