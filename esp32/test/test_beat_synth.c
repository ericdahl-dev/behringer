// Host tests for the pure free-running beat generator (LNK-033).
#include "unity.h"
#include "beat_synth.h"

static BeatSynth s;
void setUp(void)    { beat_synth_reset(&s); }
void tearDown(void) {}

void test_fresh_state_fires_on_first_step_past_interval(void) {
    // Matches the old inline behaviour: static last=0, a large millis() reading
    // is >= one interval so the first observed beat fires.
    TEST_ASSERT_TRUE(beat_synth_step(&s, 1000, 120.0f));  // interval 500, 1000-0 >= 500
}

void test_no_fire_within_interval(void) {
    beat_synth_step(&s, 1000, 120.0f);                    // latch at 1000
    TEST_ASSERT_FALSE(beat_synth_step(&s, 1200, 120.0f)); // +200 < 500
    TEST_ASSERT_FALSE(beat_synth_step(&s, 1499, 120.0f)); // +499 < 500
}

void test_fires_at_interval_boundary(void) {
    beat_synth_step(&s, 1000, 120.0f);
    TEST_ASSERT_TRUE(beat_synth_step(&s, 1500, 120.0f));  // exactly 500
    TEST_ASSERT_FALSE(beat_synth_step(&s, 1700, 120.0f)); // relatched at 1500, +200 < 500
    TEST_ASSERT_TRUE(beat_synth_step(&s, 2000, 120.0f));  // +500
}

void test_bpm_zero_or_negative_never_fires_and_keeps_state(void) {
    beat_synth_step(&s, 1000, 120.0f);                    // latch 1000
    TEST_ASSERT_FALSE(beat_synth_step(&s, 5000, 0.0f));   // bpm 0 -> false
    TEST_ASSERT_FALSE(beat_synth_step(&s, 5000, -60.0f)); // bpm <0 -> false
    // state untouched: a resumed BPM fires promptly (5000 - 1000 >> interval)
    TEST_ASSERT_TRUE(beat_synth_step(&s, 5000, 120.0f));
}

void test_bpm_change_recomputes_interval(void) {
    beat_synth_step(&s, 1000, 60.0f);                     // interval 1000, fires, latch 1000
    TEST_ASSERT_FALSE(beat_synth_step(&s, 1600, 60.0f));  // +600 < 1000
    // now 240 bpm -> interval 250; 1600-1000=600 >= 250 -> fires
    TEST_ASSERT_TRUE(beat_synth_step(&s, 1600, 240.0f));
}

void test_reset_clears_latch(void) {
    beat_synth_step(&s, 10000, 120.0f);
    beat_synth_reset(&s);
    TEST_ASSERT_TRUE(beat_synth_step(&s, 600, 120.0f));   // last_ms back to 0
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_state_fires_on_first_step_past_interval);
    RUN_TEST(test_no_fire_within_interval);
    RUN_TEST(test_fires_at_interval_boundary);
    RUN_TEST(test_bpm_zero_or_negative_never_fires_and_keeps_state);
    RUN_TEST(test_bpm_change_recomputes_interval);
    RUN_TEST(test_reset_clears_latch);
    return UNITY_END();
}
