// Host tests for the pure Link measurement-session orchestrator (LNK-031).
// These encode the LNK-026 regressions as unit tests: they used to be catchable
// only by flashing hardware.
#include "unity.h"
#include "link_measurement_session.h"
#include "link_measurement.h"   // LINK_MEASUREMENT_READY_SAMPLES

static LinkSession s;
static LinkSessionAct acts[8];
void setUp(void)    { link_session_reset(&s); }
void tearDown(void) {}

// Convenience: start an attempt against a peer and return the action count.
static int start(uint32_t ip, uint16_t port, int64_t now) {
    return link_session_on_trigger(&s, true, ip, port, now, /*active=*/false, acts, 8);
}

/* ---- trigger ----------------------------------------------------------- */

void test_first_peer_starts_attempt_flush_first(void) {
    int n = start(0xC0A80105, 20808, 1000);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_INT(LS_FLUSH_RX,     acts[0].type);   // LNK-026 bug 2: flush before anything
    TEST_ASSERT_EQUAL_INT(LS_START_ATTEMPT, acts[1].type);
    TEST_ASSERT_EQUAL_INT(LS_SEND_PING,    acts[2].type);
    TEST_ASSERT_FALSE(acts[2].has_prev_ghost);             // round 1: no PrevGHostTime
    TEST_ASSERT_EQUAL_UINT32(0xC0A80105, acts[2].ip);
    TEST_ASSERT_EQUAL_INT(1000, (int)acts[2].send_time_us);
    TEST_ASSERT_TRUE(s.have_ref);
    TEST_ASSERT_EQUAL_INT64(1000 + LINK_SESSION_REMEASURE_US, s.next_measure_us);
    TEST_ASSERT_EQUAL_INT64(1000, s.last_send_us);
}

void test_no_peer_forgets_ref(void) {
    start(0xC0A80105, 20808, 1000);
    int n = link_session_on_trigger(&s, false, 0, 0, 2000, false, acts, 8);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_FALSE(s.have_ref);
}

void test_same_peer_not_due_no_retrigger(void) {
    start(0xC0A80105, 20808, 1000);              // next_measure = 1000 + 2e6
    int n = link_session_on_trigger(&s, true, 0xC0A80105, 20808, 5000, false, acts, 8);
    TEST_ASSERT_EQUAL_INT(0, n);                 // same peer, not due
}

void test_due_timer_retriggers(void) {
    start(0xC0A80105, 20808, 1000);
    int64_t due = 1000 + LINK_SESSION_REMEASURE_US;
    int n = link_session_on_trigger(&s, true, 0xC0A80105, 20808, due, false, acts, 8);
    TEST_ASSERT_EQUAL_INT(3, n);                 // periodic re-measure
    TEST_ASSERT_EQUAL_INT(LS_FLUSH_RX, acts[0].type);
}

void test_different_peer_retriggers(void) {
    start(0xC0A80105, 20808, 1000);
    int n = link_session_on_trigger(&s, true, 0xC0A80106, 20808, 1500, false, acts, 8);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_UINT32(0xC0A80106, acts[2].ip);
}

void test_trigger_gated_by_active(void) {
    start(0xC0A80105, 20808, 1000);
    int64_t due = 1000 + LINK_SESSION_REMEASURE_US;
    int n = link_session_on_trigger(&s, true, 0xC0A80105, 20808, due, /*active=*/true, acts, 8);
    TEST_ASSERT_EQUAL_INT(0, n);                 // due but an attempt is already running
}

/* ---- epoch reset (LNK-026 bug 1) --------------------------------------- */

void test_epoch_reset_on_backward_jump(void) {
    // prime with a timeline, then a big backward time_origin jump = re-origin.
    link_session_on_timeline(&s, true, 510000000, acts, 8);     // prime
    int n = link_session_on_timeline(&s, true, 200000, acts, 8); // jumps back ~510s
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(LS_RESET_XFORM, acts[0].type);
    TEST_ASSERT_FALSE(s.have_ref);                              // forces re-target
    TEST_ASSERT_EQUAL_INT64(0, s.next_measure_us);             // re-measure now
    // ...and the next trigger immediately starts a fresh attempt (due, now >= 0)
    int m = link_session_on_trigger(&s, true, 0xC0A80105, 20808, 100, false, acts, 8);
    TEST_ASSERT_EQUAL_INT(3, m);
}

void test_no_reset_on_small_or_forward_step(void) {
    link_session_on_timeline(&s, true, 1000000, acts, 8);          // prime
    TEST_ASSERT_EQUAL_INT(0, link_session_on_timeline(&s, true, 1500000, acts, 8)); // forward
    TEST_ASSERT_EQUAL_INT(0, link_session_on_timeline(&s, true, 1400000, acts, 8)); // small back (<1s)
}

void test_first_timeline_primes_no_reset(void) {
    TEST_ASSERT_EQUAL_INT(0, link_session_on_timeline(&s, true, 5, acts, 8)); // no last_origin yet
    TEST_ASSERT_TRUE(s.have_last_origin);
}

void test_invalid_timeline_is_noop(void) {
    link_session_on_timeline(&s, true, 5000000, acts, 8);   // prime (5 s)
    TEST_ASSERT_EQUAL_INT(0, link_session_on_timeline(&s, false, 0, acts, 8));
    // a real >1s backward jump after the no-op still fires (last_origin untouched)
    TEST_ASSERT_EQUAL_INT(1, link_session_on_timeline(&s, true, 5, acts, 8));
}

/* ---- pong -------------------------------------------------------------- */

void test_pong_not_ready_repings_with_prev_ghost(void) {
    start(0xC0A80105, 20808, 1000);
    s.consecutive_timeouts = 3;                              // pretend some silence
    int n = link_session_on_pong(&s, 1200, /*has_ghost=*/true, 999, /*samples=*/1, acts, 8);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(LS_SEND_PING, acts[0].type);
    TEST_ASSERT_TRUE(acts[0].has_prev_ghost);               // round 2+ carries PrevGHostTime
    TEST_ASSERT_EQUAL_INT64(999, acts[0].prev_ghost_us);
    TEST_ASSERT_EQUAL_INT64(1200, acts[0].send_time_us);
    TEST_ASSERT_EQUAL_INT(0, s.consecutive_timeouts);       // pong resets the watchdog count
}

void test_pong_ready_commits(void) {
    start(0xC0A80105, 20808, 1000);
    int n = link_session_on_pong(&s, 1200, true, 999, LINK_MEASUREMENT_READY_SAMPLES, acts, 8);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(LS_END_OK, acts[0].type);         // no re-ping once committed
}

/* ---- watchdog ---------------------------------------------------------- */

void test_watchdog_silent_below_threshold(void) {
    start(0xC0A80105, 20808, 1000);                          // last_send = 1000
    TEST_ASSERT_EQUAL_INT(0, link_session_on_watchdog(&s, 1000 + 40000, acts, 8)); // 40ms < 50
}

void test_watchdog_reping_on_silence(void) {
    start(0xC0A80105, 20808, 1000);
    int n = link_session_on_watchdog(&s, 1000 + LINK_SESSION_WATCHDOG_US, acts, 8);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(LS_SEND_PING, acts[0].type);
    TEST_ASSERT_EQUAL_INT(1, s.consecutive_timeouts);
    TEST_ASSERT_EQUAL_INT64(1000 + LINK_SESSION_WATCHDOG_US, s.last_send_us); // relatched
}

void test_watchdog_abandons_after_max_timeouts(void) {
    start(0xC0A80105, 20808, 1000);
    int64_t t = 1000;
    for (int i = 1; i < LINK_SESSION_MAX_TIMEOUTS; i++) {    // fire 4 re-pings
        t += LINK_SESSION_WATCHDOG_US;
        int n = link_session_on_watchdog(&s, t, acts, 8);
        TEST_ASSERT_EQUAL_INT(1, n);
        TEST_ASSERT_EQUAL_INT(LS_SEND_PING, acts[0].type);
    }
    t += LINK_SESSION_WATCHDOG_US;                           // 5th timeout
    int n = link_session_on_watchdog(&s, t, acts, 8);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(LS_END_FAIL, acts[0].type);        // abandon; glue leaves xform untouched
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_first_peer_starts_attempt_flush_first);
    RUN_TEST(test_no_peer_forgets_ref);
    RUN_TEST(test_same_peer_not_due_no_retrigger);
    RUN_TEST(test_due_timer_retriggers);
    RUN_TEST(test_different_peer_retriggers);
    RUN_TEST(test_trigger_gated_by_active);
    RUN_TEST(test_epoch_reset_on_backward_jump);
    RUN_TEST(test_no_reset_on_small_or_forward_step);
    RUN_TEST(test_first_timeline_primes_no_reset);
    RUN_TEST(test_invalid_timeline_is_noop);
    RUN_TEST(test_pong_not_ready_repings_with_prev_ghost);
    RUN_TEST(test_pong_ready_commits);
    RUN_TEST(test_watchdog_silent_below_threshold);
    RUN_TEST(test_watchdog_reping_on_silence);
    RUN_TEST(test_watchdog_abandons_after_max_timeouts);
    return UNITY_END();
}
