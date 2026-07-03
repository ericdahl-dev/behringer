#include "unity.h"
#include "link_phase.h"

void setUp(void)    {}
void tearDown(void) {}

// timeline: 1 beat = 500000us (120 BPM), origin at beat 0, ghost time 0.
// At ghost_now_us = 500000 (one beat elapsed), beats_now should be 1.0.
void test_beats_now_one_beat_elapsed(void) {
    LinkTimeline tl = { .micros_per_beat = 500000, .beat_origin_micro = 0,
                         .time_origin_us = 0 };
    double beats = link_phase_beats_now(tl, 500000);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, (float)beats);
}

// Non-zero beat origin: timeline says beat 2.0 at ghost time origin.
void test_beats_now_with_beat_origin(void) {
    LinkTimeline tl = { .micros_per_beat = 500000,
                         .beat_origin_micro = 2000000,  // 2.0 beats
                         .time_origin_us = 1000000 };
    // 1.5 beats (750000us) after the time origin -> 2.0 + 1.5 = 3.5
    double beats = link_phase_beats_now(tl, 1000000 + 750000);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.5f, (float)beats);
}

// ghost_now_us before the timeline's time_origin_us -> negative beats_now.
void test_beats_now_can_be_negative(void) {
    LinkTimeline tl = { .micros_per_beat = 500000, .beat_origin_micro = 0,
                         .time_origin_us = 1000000 };
    double beats = link_phase_beats_now(tl, 500000);  // 0.5 beats early
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, (float)beats);
}

void test_phase_from_beats_within_first_bar(void) {
    double phase = link_phase_from_beats(1.5, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, (float)phase);
}

void test_phase_from_beats_wraps_past_quantum(void) {
    // 5.25 beats, quantum 4 -> phase 1.25
    double phase = link_phase_from_beats(5.25, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, (float)phase);
}

// Negative beats_now must be shifted into [0, quantum) per the ticket's
// explicit fmod()-can-return-negative note.
void test_phase_from_beats_negative_beats_now(void) {
    // -1.0 beats, quantum 4 -> fmod(-1,4) == -1 -> shifted to 3.0
    double phase = link_phase_from_beats(-1.0, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.0f, (float)phase);
}

void test_phase_from_beats_negative_beats_now_multi_bar(void) {
    // -9.5 beats, quantum 4 -> fmod(-9.5,4) == -1.5 -> shifted to 2.5
    double phase = link_phase_from_beats(-9.5, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.5f, (float)phase);
}

void test_phase_from_beats_at_exact_quantum_boundary(void) {
    // exactly on a bar boundary -> phase 0
    double phase = link_phase_from_beats(8.0, 4.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, (float)phase);
}

// LNK-026: a real re-origin (captured on hardware) jumps time_origin backward by
// ~510 seconds — the session's ghost-time epoch reset when transport restarted.
void test_epoch_reset_detects_backward_jump(void) {
    TEST_ASSERT_TRUE(link_phase_timeline_epoch_reset(510898878, 416793));
}

// Normal play: time_origin advances forward every gossip -> never a reset.
void test_epoch_reset_ignores_forward_advance(void) {
    TEST_ASSERT_FALSE(link_phase_timeline_epoch_reset(498285685, 498941640));
}

// A small backward step (UDP reordering / gossip jitter delivering a slightly
// older timeline) must NOT be treated as a re-origin — else the wheel flickers
// to "syncing" on every out-of-order packet.
void test_epoch_reset_ignores_small_backward_jitter(void) {
    TEST_ASSERT_FALSE(link_phase_timeline_epoch_reset(500000000, 499500000));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_beats_now_one_beat_elapsed);
    RUN_TEST(test_beats_now_with_beat_origin);
    RUN_TEST(test_beats_now_can_be_negative);
    RUN_TEST(test_phase_from_beats_within_first_bar);
    RUN_TEST(test_phase_from_beats_wraps_past_quantum);
    RUN_TEST(test_phase_from_beats_negative_beats_now);
    RUN_TEST(test_phase_from_beats_negative_beats_now_multi_bar);
    RUN_TEST(test_phase_from_beats_at_exact_quantum_boundary);
    RUN_TEST(test_epoch_reset_detects_backward_jump);
    RUN_TEST(test_epoch_reset_ignores_forward_advance);
    RUN_TEST(test_epoch_reset_ignores_small_backward_jitter);
    return UNITY_END();
}
