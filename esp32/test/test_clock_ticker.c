// Host tests for the shared pure clock-tick scheduler (LNK-027/LNK-028).
#include "unity.h"
#include "clock_ticker.h"

static ClockTicker ct;
static BarReset    br;
void setUp(void)    { clock_ticker_reset(&ct); bar_reset_reset(&br); }
void tearDown(void) {}

/* ---- ClockTicker ------------------------------------------------------- */

void test_first_call_primes_and_emits_nothing(void) {
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 10.0, 4, 4));
}

void test_ppqn4_four_ticks_per_beat(void) {
    double slot = 1.0 / 4.0;
    clock_ticker_ticks_due(&ct, 0.0, 4, 4);            // prime at beat 0
    int total = 0;
    for (int i = 1; i <= 4; i++)
        total += clock_ticker_ticks_due(&ct, i * slot, 4, 4);
    TEST_ASSERT_EQUAL_INT(4, total);                   // 4 PPQN over one beat
}

void test_ppqn1_one_tick_per_beat(void) {
    clock_ticker_ticks_due(&ct, 0.0, 1, 4);            // prime
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 0.5, 1, 4));  // same beat-slot
    TEST_ASSERT_EQUAL_INT(1, clock_ticker_ticks_due(&ct, 1.0, 1, 4));  // next beat
}

void test_ppqn24_matches_midi_cadence(void) {
    double slot = 1.0 / 24.0;
    clock_ticker_ticks_due(&ct, 0.0, 24, 4);           // prime
    int total = 0;
    for (int i = 1; i <= 24; i++)
        total += clock_ticker_ticks_due(&ct, i * slot, 24, 4);
    TEST_ASSERT_EQUAL_INT(24, total);
}

void test_large_jump_reprimes_no_flood(void) {
    clock_ticker_ticks_due(&ct, 0.0, 4, 4);            // prime
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 100.0, 4, 4));       // realign
    TEST_ASSERT_EQUAL_INT(1, clock_ticker_ticks_due(&ct, 100.0 + 0.25, 4, 4)); // resumes
}

void test_backward_move_emits_nothing(void) {
    clock_ticker_ticks_due(&ct, 5.0, 4, 4);
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 4.0, 4, 4));
}

void test_negative_beats_and_bad_ppqn(void) {
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, -1.0, 4, 4));  // primes at 0
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 1.0, 0, 4));   // ppqn<=0 no-op
    TEST_ASSERT_EQUAL_INT(0, clock_ticker_ticks_due(&ct, 1.0, -4, 4));
}

/* ---- BarReset ---------------------------------------------------------- */

void test_bar_reset_primes_then_fires_on_crossing(void) {
    TEST_ASSERT_FALSE(bar_reset_due(&br, 0.0, 4.0));   // prime at bar 0
    TEST_ASSERT_FALSE(bar_reset_due(&br, 3.9, 4.0));   // still bar 0
    TEST_ASSERT_TRUE (bar_reset_due(&br, 4.1, 4.0));   // crossed into bar 1
    TEST_ASSERT_FALSE(bar_reset_due(&br, 5.0, 4.0));   // still bar 1
    TEST_ASSERT_TRUE (bar_reset_due(&br, 8.0, 4.0));   // bar 2
}

void test_bar_reset_backward_reprimes_no_fire(void) {
    bar_reset_due(&br, 8.0, 4.0);                      // prime at bar 2
    TEST_ASSERT_FALSE(bar_reset_due(&br, 1.0, 4.0));   // jumped back -> reprime, no false downbeat
    TEST_ASSERT_FALSE(bar_reset_due(&br, 1.5, 4.0));   // same bar 0
    TEST_ASSERT_TRUE (bar_reset_due(&br, 4.0, 4.0));   // clean crossing again
}

void test_bar_reset_multibar_jump_reprimes_no_fire(void) {
    bar_reset_due(&br, 0.0, 4.0);                      // prime bar 0
    TEST_ASSERT_FALSE(bar_reset_due(&br, 40.0, 4.0));  // +10 bars (re-origin) -> reprime, no fire
    TEST_ASSERT_TRUE (bar_reset_due(&br, 44.0, 4.0));  // next clean crossing fires
}

void test_bar_reset_bad_quantum(void) {
    TEST_ASSERT_FALSE(bar_reset_due(&br, 4.0, 0.0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_first_call_primes_and_emits_nothing);
    RUN_TEST(test_ppqn4_four_ticks_per_beat);
    RUN_TEST(test_ppqn1_one_tick_per_beat);
    RUN_TEST(test_ppqn24_matches_midi_cadence);
    RUN_TEST(test_large_jump_reprimes_no_flood);
    RUN_TEST(test_backward_move_emits_nothing);
    RUN_TEST(test_negative_beats_and_bad_ppqn);
    RUN_TEST(test_bar_reset_primes_then_fires_on_crossing);
    RUN_TEST(test_bar_reset_backward_reprimes_no_fire);
    RUN_TEST(test_bar_reset_multibar_jump_reprimes_no_fire);
    RUN_TEST(test_bar_reset_bad_quantum);
    return UNITY_END();
}
