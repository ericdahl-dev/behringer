// Host tests for the pure 24-PPQN MIDI-clock-out scheduler (LNK-027).
#include "unity.h"
#include "midi_clock_out.h"

static MidiClockOut s;
void setUp(void)    { midi_clock_out_reset(&s); }
void tearDown(void) {}

#define SLOT (1.0 / 24.0)  // one 1/24-beat grid step

void test_first_call_primes_and_emits_nothing(void) {
    // First observation after a reset just aligns the grid — no backlog dump.
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, 10.0, 4));
}

void test_one_tick_per_slot_crossing(void) {
    midi_clock_out_ticks_due(&s, 10.0, 4);                 // prime
    TEST_ASSERT_EQUAL_INT(1, midi_clock_out_ticks_due(&s, 10.0 + SLOT, 4));
    TEST_ASSERT_EQUAL_INT(1, midi_clock_out_ticks_due(&s, 10.0 + 2*SLOT, 4));
}

void test_twentyfour_ticks_per_beat(void) {
    double b = 0.0;
    midi_clock_out_ticks_due(&s, b, 4);                    // prime at beat 0
    int total = 0;
    for (int i = 1; i <= 24; i++)                          // step to exactly beat 1
        total += midi_clock_out_ticks_due(&s, i * SLOT, 4);
    TEST_ASSERT_EQUAL_INT(24, total);                      // 24 PPQN
}

void test_same_slot_emits_nothing(void) {
    midi_clock_out_ticks_due(&s, 10.0, 4);
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, 10.0 + 0.4*SLOT, 4));  // still slot 240
}

void test_small_multi_slot_gap_within_burst(void) {
    midi_clock_out_ticks_due(&s, 0.0, 4);
    TEST_ASSERT_EQUAL_INT(3, midi_clock_out_ticks_due(&s, 3*SLOT, 4));  // caught up 3 ticks
}

void test_backward_move_emits_nothing(void) {
    midi_clock_out_ticks_due(&s, 5.0, 4);
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, 4.0, 4));
}

void test_large_jump_reprimes_no_flood(void) {
    midi_clock_out_ticks_due(&s, 0.0, 4);                  // prime
    // A tempo re-origin jumps the beat position by way more than max_burst:
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, 100.0, 4));  // realign, no burst
    // ...and it keeps ticking cleanly from the new position:
    TEST_ASSERT_EQUAL_INT(1, midi_clock_out_ticks_due(&s, 100.0 + SLOT, 4));
}

void test_negative_beats_treated_as_zero(void) {
    // glue passes <0 while phase isn't valid; must not emit or crash.
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, -1.0, 4));   // primes at 0
    TEST_ASSERT_EQUAL_INT(0, midi_clock_out_ticks_due(&s, -5.0, 4));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_first_call_primes_and_emits_nothing);
    RUN_TEST(test_one_tick_per_slot_crossing);
    RUN_TEST(test_twentyfour_ticks_per_beat);
    RUN_TEST(test_same_slot_emits_nothing);
    RUN_TEST(test_small_multi_slot_gap_within_burst);
    RUN_TEST(test_backward_move_emits_nothing);
    RUN_TEST(test_large_jump_reprimes_no_flood);
    RUN_TEST(test_negative_beats_treated_as_zero);
    return UNITY_END();
}
