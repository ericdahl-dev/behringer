#include "unity.h"
#include "link_measurement.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    { link_measurement_reset(); }
void tearDown(void) {}

/* ---------------------------------------------------------------------- */
/* GhostXForm                                                              */
/* ---------------------------------------------------------------------- */

void test_host_to_ghost_applies_intercept(void) {
    LinkGhostXForm xf = { .intercept_us = 12345, .valid = true };
    TEST_ASSERT_EQUAL_INT64(112345, link_ghost_xform_host_to_ghost(xf, 100000));
}

void test_host_to_ghost_handles_negative_intercept(void) {
    LinkGhostXForm xf = { .intercept_us = -500, .valid = true };
    TEST_ASSERT_EQUAL_INT64(9500, link_ghost_xform_host_to_ghost(xf, 10000));
}

/* ---------------------------------------------------------------------- */
/* Ping build/parse round trip                                             */
/* ---------------------------------------------------------------------- */

void test_build_ping_round1_has_no_prev_ghost(void) {
    uint8_t buf[64];
    int n = link_measurement_build_ping(buf, sizeof(buf), 1000, false, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    // magic
    const uint8_t magic[8] = {'_','l','i','n','k','_','v', 1};
    TEST_ASSERT_EQUAL_MEMORY(magic, buf, 8);
    TEST_ASSERT_EQUAL_UINT8(1, buf[8]);  // kPing
}

void test_build_ping_round1_parses_back(void) {
    uint8_t buf[64];
    int n = link_measurement_build_ping(buf, sizeof(buf), 424242, false, 0);

    LinkPingFields f;
    TEST_ASSERT_TRUE(link_measurement_parse_ping(buf, n, &f));
    TEST_ASSERT_TRUE(f.has_host_time);
    TEST_ASSERT_EQUAL_INT64(424242, f.host_time_us);
    TEST_ASSERT_FALSE(f.has_prev_ghost_time);
}

void test_build_ping_round2_parses_back_with_prev_ghost(void) {
    uint8_t buf[64];
    int n = link_measurement_build_ping(buf, sizeof(buf), 99, true, -7777);

    LinkPingFields f;
    TEST_ASSERT_TRUE(link_measurement_parse_ping(buf, n, &f));
    TEST_ASSERT_TRUE(f.has_host_time);
    TEST_ASSERT_EQUAL_INT64(99, f.host_time_us);
    TEST_ASSERT_TRUE(f.has_prev_ghost_time);
    TEST_ASSERT_EQUAL_INT64(-7777, f.prev_ghost_time_us);
}

void test_build_ping_fails_when_buf_too_small(void) {
    uint8_t buf[5];
    TEST_ASSERT_EQUAL_INT(0, link_measurement_build_ping(buf, sizeof(buf), 1, false, 0));
}

void test_parse_ping_rejects_wrong_magic(void) {
    uint8_t buf[64];
    int n = link_measurement_build_ping(buf, sizeof(buf), 1, false, 0);
    buf[0] = 'X';
    LinkPingFields f;
    TEST_ASSERT_FALSE(link_measurement_parse_ping(buf, n, &f));
}

void test_parse_ping_rejects_wrong_type(void) {
    uint8_t buf[64];
    int n = link_measurement_build_ping(buf, sizeof(buf), 1, false, 0);
    buf[8] = 2;  // kPong, not kPing
    LinkPingFields f;
    TEST_ASSERT_FALSE(link_measurement_parse_ping(buf, n, &f));
}

/* ---------------------------------------------------------------------- */
/* Pong parse — including "build a pong wrapping it" round trip            */
/* ---------------------------------------------------------------------- */

static void put_be32_helper(uint8_t* buf, int* i, uint32_t v) {
    buf[(*i)++] = (uint8_t)(v >> 24);
    buf[(*i)++] = (uint8_t)(v >> 16);
    buf[(*i)++] = (uint8_t)(v >> 8);
    buf[(*i)++] = (uint8_t)v;
}

static void put_be64_helper(uint8_t* buf, int* i, int64_t v) {
    for (int s = 56; s >= 0; s -= 8) buf[(*i)++] = (uint8_t)(v >> s);
}

// Test-only helper: a real Link peer's PingResponder builds the pong, which
// we never implement in production (pinger-only firmware). Mirrors the
// make_alive_packet() style helper in test_link_protocol.c.
//
// Wraps: SessionMembership (8 raw bytes, ignorable) + GHostTime{ghost_us},
// followed by the *raw bytes* of our own echoed ping payload (already
// built via link_measurement_build_ping) verbatim, exactly as the spec
// describes.
static int build_pong_wrapping_ping(uint8_t* out, int out_cap,
                                     int64_t ghost_us,
                                     const uint8_t* ping_buf, int ping_len) {
    const uint8_t magic[8] = {'_','l','i','n','k','_','v', 1};
    int i = 0;
    memcpy(out + i, magic, 8); i += 8;
    out[i++] = 2;  // kPong

    // SessionMembership TLV ('sess'), 8 raw bytes — ignored by our parser.
    put_be32_helper(out, &i, 0x73657373u);
    put_be32_helper(out, &i, 8);
    memset(out + i, 0xAB, 8); i += 8;

    // GHostTime TLV
    put_be32_helper(out, &i, 0x5f5f6774u);
    put_be32_helper(out, &i, 8);
    put_be64_helper(out, &i, ghost_us);

    // Echo the ping's own TLVs verbatim (skip its 9-byte magic+type header).
    TEST_ASSERT_TRUE_MESSAGE(i + (ping_len - 9) <= out_cap, "pong buffer too small");
    memcpy(out + i, ping_buf + 9, (size_t)(ping_len - 9));
    i += (ping_len - 9);

    return i;
}

void test_pong_round_trip_round1(void) {
    uint8_t ping[64];
    int ping_len = link_measurement_build_ping(ping, sizeof(ping), 5000, false, 0);

    uint8_t pong[128];
    int pong_len = build_pong_wrapping_ping(pong, sizeof(pong), 99999, ping, ping_len);

    LinkPongFields f;
    TEST_ASSERT_TRUE(link_measurement_parse_pong(pong, pong_len, &f));
    TEST_ASSERT_TRUE(f.has_ghost_time);
    TEST_ASSERT_EQUAL_INT64(99999, f.ghost_time_us);
    TEST_ASSERT_TRUE(f.has_host_time);
    TEST_ASSERT_EQUAL_INT64(5000, f.host_time_us);
    TEST_ASSERT_FALSE(f.has_prev_ghost_time);
}

void test_pong_round_trip_round2_with_prev_ghost(void) {
    uint8_t ping[64];
    int ping_len = link_measurement_build_ping(ping, sizeof(ping), 6000, true, 4242);

    uint8_t pong[128];
    int pong_len = build_pong_wrapping_ping(pong, sizeof(pong), 88888, ping, ping_len);

    LinkPongFields f;
    TEST_ASSERT_TRUE(link_measurement_parse_pong(pong, pong_len, &f));
    TEST_ASSERT_TRUE(f.has_ghost_time);
    TEST_ASSERT_EQUAL_INT64(88888, f.ghost_time_us);
    TEST_ASSERT_TRUE(f.has_host_time);
    TEST_ASSERT_EQUAL_INT64(6000, f.host_time_us);
    TEST_ASSERT_TRUE(f.has_prev_ghost_time);
    TEST_ASSERT_EQUAL_INT64(4242, f.prev_ghost_time_us);
}

void test_pong_rejects_wrong_magic(void) {
    uint8_t pong[20] = {'_','l','i','n','k','_','v', 1, 2};
    pong[0] = 'Z';
    LinkPongFields f;
    TEST_ASSERT_FALSE(link_measurement_parse_pong(pong, sizeof(pong), &f));
}

void test_pong_rejects_wrong_type(void) {
    uint8_t pong[20] = {'_','l','i','n','k','_','v', 1, 1 /* kPing, not kPong */};
    LinkPongFields f;
    TEST_ASSERT_FALSE(link_measurement_parse_pong(pong, sizeof(pong), &f));
}

void test_pong_short_packet_ignored(void) {
    uint8_t pong[5] = {'_','l','i','n','k'};
    LinkPongFields f;
    TEST_ASSERT_FALSE(link_measurement_parse_pong(pong, sizeof(pong), &f));
}

void test_pong_with_unknown_tlv_still_parses_known_fields(void) {
    uint8_t pong[64];
    int i = 0;
    const uint8_t magic[8] = {'_','l','i','n','k','_','v', 1};
    memcpy(pong + i, magic, 8); i += 8;
    pong[i++] = 2;  // kPong

    // Unknown TLV the parser must skip gracefully.
    put_be32_helper(pong, &i, 0xDEADBEEFu);
    put_be32_helper(pong, &i, 4);
    memset(pong + i, 0, 4); i += 4;

    // GHostTime
    put_be32_helper(pong, &i, 0x5f5f6774u);
    put_be32_helper(pong, &i, 8);
    put_be64_helper(pong, &i, 321);

    LinkPongFields f;
    TEST_ASSERT_TRUE(link_measurement_parse_pong(pong, i, &f));
    TEST_ASSERT_TRUE(f.has_ghost_time);
    TEST_ASSERT_EQUAL_INT64(321, f.ghost_time_us);
    TEST_ASSERT_FALSE(f.has_host_time);
}

/* ---------------------------------------------------------------------- */
/* Sample math                                                             */
/* ---------------------------------------------------------------------- */

void test_add_pong_samples_round1_pushes_sample_a_only(void) {
    // h_recv=1000, h_sent=200, g=500 -> sample_a = 500 - (1000+200)/2 = 500-600 = -100
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.ghost_time_us = 500;
    f.has_host_time  = true; f.host_time_us  = 200;

    int added = link_measurement_add_pong_samples(1000, &f);
    TEST_ASSERT_EQUAL_INT(1, added);
    TEST_ASSERT_EQUAL_INT(1, link_measurement_samples_count());

    double m;
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -100.0f, (float)m);
}

void test_add_pong_samples_round2_pushes_sample_a_and_b(void) {
    // h_recv=1000, h_sent=200, g=500, prev_g=300
    // sample_a = 500 - (1000+200)/2 = -100
    // sample_b = (500+300)/2 - 200 = 400 - 200 = 200
    LinkPongFields f = {0};
    f.has_ghost_time = true;      f.ghost_time_us      = 500;
    f.has_host_time  = true;      f.host_time_us       = 200;
    f.has_prev_ghost_time = true; f.prev_ghost_time_us = 300;

    int added = link_measurement_add_pong_samples(1000, &f);
    TEST_ASSERT_EQUAL_INT(2, added);
    TEST_ASSERT_EQUAL_INT(2, link_measurement_samples_count());

    double m;
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    // median of {-100, 200} = 50
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, (float)m);
}

void test_add_pong_samples_rejects_stale_rtt(void) {
    // LNK-026 bug 2: a pong left in the RX buffer across the ~2s idle gap
    // between attempts echoes a host_time ~2s old. rtt = h_recv - host_time
    // then blows past any LAN bound; committing it underestimates the
    // GhostXForm by ~rtt/2 (~1s ~= 2 beats). Reject such samples outright.
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.ghost_time_us = 500;
    f.has_host_time  = true; f.host_time_us  = 200;
    // h_recv 2_000_200 -> rtt = 2_000_000 us (2 s), way over the bound.
    TEST_ASSERT_EQUAL_INT(0, link_measurement_add_pong_samples(2000200, &f));
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

void test_add_pong_samples_rejects_negative_rtt(void) {
    // h_recv earlier than the echoed host_time is impossible on one clock;
    // treat it as a stale/mismatched pong and drop it.
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.ghost_time_us = 500;
    f.has_host_time  = true; f.host_time_us  = 1000;
    TEST_ASSERT_EQUAL_INT(0, link_measurement_add_pong_samples(200, &f));
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

void test_add_pong_samples_skips_when_ghost_time_missing(void) {
    LinkPongFields f = {0};
    f.has_host_time = true; f.host_time_us = 200;
    TEST_ASSERT_EQUAL_INT(0, link_measurement_add_pong_samples(1000, &f));
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

void test_add_pong_samples_skips_when_host_time_missing(void) {
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.ghost_time_us = 500;
    TEST_ASSERT_EQUAL_INT(0, link_measurement_add_pong_samples(1000, &f));
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

void test_median_false_when_no_samples(void) {
    double m;
    TEST_ASSERT_FALSE(link_measurement_median(&m));
}

void test_median_odd_count(void) {
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true;

    f.ghost_time_us = 10; f.host_time_us = 0;
    link_measurement_add_pong_samples(0, &f);  // sample_a = 10 - 0 = 10
    f.ghost_time_us = 30; f.host_time_us = 0;
    link_measurement_add_pong_samples(0, &f);  // sample_a = 30
    f.ghost_time_us = 20; f.host_time_us = 0;
    link_measurement_add_pong_samples(0, &f);  // sample_a = 20

    double m;
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, (float)m);  // sorted {10,20,30} -> middle = 20
}

void test_median_even_count_averages_middle_two(void) {
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true;

    f.ghost_time_us = 10; link_measurement_add_pong_samples(0, &f);
    f.ghost_time_us = 30; link_measurement_add_pong_samples(0, &f);

    double m;
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, (float)m);  // (10+30)/2
}

void test_samples_buffer_caps_at_32_and_evicts_oldest(void) {
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true; f.host_time_us = 0;

    // Fill the ring exactly full with 0s.
    for (int i = 0; i < LINK_MEASUREMENT_MAX_SAMPLES; i++) {
        f.ghost_time_us = 0;
        link_measurement_add_pong_samples(0, &f);
    }
    TEST_ASSERT_EQUAL_INT(LINK_MEASUREMENT_MAX_SAMPLES, link_measurement_samples_count());

    double m;
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, (float)m);

    // Push exactly one full ring's worth more of a different value — this
    // must fully evict the original 0s without growing past the cap.
    for (int i = 0; i < LINK_MEASUREMENT_MAX_SAMPLES; i++) {
        f.ghost_time_us = 1000;
        link_measurement_add_pong_samples(0, &f);
    }
    TEST_ASSERT_EQUAL_INT(LINK_MEASUREMENT_MAX_SAMPLES, link_measurement_samples_count());
    TEST_ASSERT_TRUE(link_measurement_median(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f, (float)m);  // all 32 slots now hold 1000
}

/* ---------------------------------------------------------------------- */
/* Attempt lifecycle                                                       */
/* ---------------------------------------------------------------------- */

void test_initial_state_inactive_and_invalid(void) {
    TEST_ASSERT_FALSE(link_measurement_active());
    LinkGhostXForm xf = link_measurement_current_xform();
    TEST_ASSERT_FALSE(xf.valid);
}

void test_attempt_begin_marks_active_and_clears_samples(void) {
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true; f.ghost_time_us = 5;
    link_measurement_add_pong_samples(0, &f);
    TEST_ASSERT_EQUAL_INT(1, link_measurement_samples_count());

    link_measurement_attempt_begin();
    TEST_ASSERT_TRUE(link_measurement_active());
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

void test_attempt_end_success_commits_median_as_xform(void) {
    link_measurement_attempt_begin();

    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true; f.host_time_us = 0;
    f.ghost_time_us = 100; link_measurement_add_pong_samples(0, &f);
    f.ghost_time_us = 200; link_measurement_add_pong_samples(0, &f);

    link_measurement_attempt_end(true);

    TEST_ASSERT_FALSE(link_measurement_active());
    LinkGhostXForm xf = link_measurement_current_xform();
    TEST_ASSERT_TRUE(xf.valid);
    TEST_ASSERT_EQUAL_INT64(150, xf.intercept_us);  // median(100,200)=150
}

void test_attempt_end_failure_leaves_existing_xform_untouched(void) {
    // First, a successful attempt commits a known-good xform.
    link_measurement_attempt_begin();
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true;
    f.ghost_time_us = 777;
    link_measurement_add_pong_samples(0, &f);
    link_measurement_attempt_end(true);
    TEST_ASSERT_EQUAL_INT64(777, link_measurement_current_xform().intercept_us);

    // Now a failed attempt (e.g. peer vanished mid-measurement) must not
    // overwrite the previously committed xform.
    link_measurement_attempt_begin();
    f.ghost_time_us = 999999;  // would be garbage if committed
    link_measurement_add_pong_samples(0, &f);
    link_measurement_attempt_end(false);

    TEST_ASSERT_FALSE(link_measurement_active());
    LinkGhostXForm xf = link_measurement_current_xform();
    TEST_ASSERT_TRUE(xf.valid);
    TEST_ASSERT_EQUAL_INT64(777, xf.intercept_us);
}

void test_attempt_end_success_with_no_samples_leaves_xform_invalid(void) {
    link_measurement_attempt_begin();
    link_measurement_attempt_end(true);  // no samples ever pushed

    TEST_ASSERT_FALSE(link_measurement_active());
    TEST_ASSERT_FALSE(link_measurement_current_xform().valid);
}

void test_reset_clears_everything(void) {
    link_measurement_attempt_begin();
    LinkPongFields f = {0};
    f.has_ghost_time = true; f.has_host_time = true; f.ghost_time_us = 42;
    link_measurement_add_pong_samples(0, &f);
    link_measurement_attempt_end(true);
    TEST_ASSERT_TRUE(link_measurement_current_xform().valid);

    link_measurement_reset();

    TEST_ASSERT_FALSE(link_measurement_active());
    TEST_ASSERT_FALSE(link_measurement_current_xform().valid);
    TEST_ASSERT_EQUAL_INT(0, link_measurement_samples_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_host_to_ghost_applies_intercept);
    RUN_TEST(test_host_to_ghost_handles_negative_intercept);

    RUN_TEST(test_build_ping_round1_has_no_prev_ghost);
    RUN_TEST(test_build_ping_round1_parses_back);
    RUN_TEST(test_build_ping_round2_parses_back_with_prev_ghost);
    RUN_TEST(test_build_ping_fails_when_buf_too_small);
    RUN_TEST(test_parse_ping_rejects_wrong_magic);
    RUN_TEST(test_parse_ping_rejects_wrong_type);

    RUN_TEST(test_pong_round_trip_round1);
    RUN_TEST(test_pong_round_trip_round2_with_prev_ghost);
    RUN_TEST(test_pong_rejects_wrong_magic);
    RUN_TEST(test_pong_rejects_wrong_type);
    RUN_TEST(test_pong_short_packet_ignored);
    RUN_TEST(test_pong_with_unknown_tlv_still_parses_known_fields);

    RUN_TEST(test_add_pong_samples_round1_pushes_sample_a_only);
    RUN_TEST(test_add_pong_samples_round2_pushes_sample_a_and_b);
    RUN_TEST(test_add_pong_samples_rejects_stale_rtt);
    RUN_TEST(test_add_pong_samples_rejects_negative_rtt);
    RUN_TEST(test_add_pong_samples_skips_when_ghost_time_missing);
    RUN_TEST(test_add_pong_samples_skips_when_host_time_missing);
    RUN_TEST(test_median_false_when_no_samples);
    RUN_TEST(test_median_odd_count);
    RUN_TEST(test_median_even_count_averages_middle_two);
    RUN_TEST(test_samples_buffer_caps_at_32_and_evicts_oldest);

    RUN_TEST(test_initial_state_inactive_and_invalid);
    RUN_TEST(test_attempt_begin_marks_active_and_clears_samples);
    RUN_TEST(test_attempt_end_success_commits_median_as_xform);
    RUN_TEST(test_attempt_end_failure_leaves_existing_xform_untouched);
    RUN_TEST(test_attempt_end_success_with_no_samples_leaves_xform_invalid);
    RUN_TEST(test_reset_clears_everything);
    return UNITY_END();
}
