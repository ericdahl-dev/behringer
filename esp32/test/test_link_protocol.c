#include "unity.h"
#include "link_protocol.h"
#include <string.h>
#include <stdint.h>

// Controllable clock — strong def overrides the weak shim in link_protocol.c
static uint32_t s_test_now;
uint32_t link_proto_millis(void) { return s_test_now; }

void setUp(void)    { s_test_now = 0; link_proto_reset(); }
void tearDown(void) {}

static void make_alive_packet_full(uint8_t* buf, int* len,
                                    uint8_t nodeid[8], int64_t us_per_beat,
                                    int64_t beat_origin_micro, int64_t time_origin_us);

static void put_be64(uint8_t* buf, int* i, int64_t v) {
    for (int s = 56; s >= 0; s -= 8) buf[(*i)++] = (v >> s) & 0xff;
}

// Craft a minimal Alive packet with a Timeline TLV.
// microsPerBeat for 120 BPM = 60e6 / 120 = 500000 us
static void make_alive_packet(uint8_t* buf, int* len,
                               uint8_t nodeid[8], int64_t us_per_beat) {
    make_alive_packet_full(buf, len, nodeid, us_per_beat, 0, 0);
}

// Full control over all three tmln fields.
static void make_alive_packet_full(uint8_t* buf, int* len,
                                    uint8_t nodeid[8], int64_t us_per_beat,
                                    int64_t beat_origin_micro, int64_t time_origin_us) {
    memset(buf, 0, 512);
    int i = 0;

    // magic
    const uint8_t magic[8] = {'_','a','s','d','p','_','v', 1};
    memcpy(buf + i, magic, 8); i += 8;

    // msgType=Alive(1), ttl=3, groupId=0
    buf[i++] = 1;   // Alive
    buf[i++] = 3;   // ttl
    buf[i++] = 0; buf[i++] = 0;  // groupId

    // NodeId (8 bytes)
    memcpy(buf + i, nodeid, 8); i += 8;

    // TLV: Timeline key=0x746d6c6e, size=24
    buf[i++] = 0x74; buf[i++] = 0x6d; buf[i++] = 0x6c; buf[i++] = 0x6e; // key
    buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x18; // size=24

    put_be64(buf, &i, us_per_beat);
    put_be64(buf, &i, beat_origin_micro);
    put_be64(buf, &i, time_origin_us);

    *len = i;
}

// Append a mep4 TLV (6-byte value: uint32 BE ip + uint16 BE port) to a
// packet already built by make_alive_packet/make_alive_packet_full.
static void append_mep4(uint8_t* buf, int* len, uint32_t ip, uint16_t port) {
    int i = *len;
    buf[i++] = 0x6d; buf[i++] = 0x65; buf[i++] = 0x70; buf[i++] = 0x34; // key 'mep4'
    buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x06; // size=6
    buf[i++] = (ip >> 24) & 0xff; buf[i++] = (ip >> 16) & 0xff;
    buf[i++] = (ip >> 8)  & 0xff; buf[i++] = ip & 0xff;
    buf[i++] = (port >> 8) & 0xff; buf[i++] = port & 0xff;
    *len = i;
}

static uint8_t NODE_A[8] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
static uint8_t NODE_B[8] = {0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8};

void test_valid_packet_extracts_bpm(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);  // 120 BPM
    link_proto_parse(buf, len);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 120.0, (float)link_proto_bpm());
}

void test_wrong_magic_ignored(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    buf[0] = 'X';  // corrupt magic
    link_proto_parse(buf, len);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, (float)link_proto_bpm());
}

void test_short_packet_ignored(void) {
    uint8_t buf[10] = {'_','a','s','d','p','_','v',1,1,3};
    TEST_ASSERT_FALSE(link_proto_parse(buf, 10));
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());
}

void test_alive_adds_peer(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());
}

void test_two_peers_counted(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);
    make_alive_packet(buf, &len, NODE_B, 500000LL);
    link_proto_parse(buf, len);
    TEST_ASSERT_EQUAL_INT(2, link_proto_peers());
}

void test_same_peer_not_double_counted(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);
    link_proto_parse(buf, len);
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());
}

void test_byebye_removes_peer(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());

    // ByeBye packet
    uint8_t bye[20];
    memset(bye, 0, sizeof(bye));
    const uint8_t magic[8] = {'_','a','s','d','p','_','v', 1};
    memcpy(bye, magic, 8);
    bye[8] = 3;  // ByeBye
    memcpy(bye + 12, NODE_A, 8);
    link_proto_parse(bye, 20);
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());
}

void test_bpm_updates_to_latest(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);   // 120 BPM
    link_proto_parse(buf, len);
    make_alive_packet(buf, &len, NODE_A, 333333LL);   // ~180 BPM
    link_proto_parse(buf, len);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 180.0, (float)link_proto_bpm());
}

void test_peer_expires_after_ttl(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);              // added at t=0, TTL 15000
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());

    s_test_now = 10000;                       // before TTL
    link_proto_tick();
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());

    s_test_now = 15001;                       // past TTL
    link_proto_tick();
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());
}

void test_bpm_resets_when_last_peer_byebyes(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);  // 120 BPM
    link_proto_parse(buf, len);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 120.0, (float)link_proto_bpm());

    uint8_t bye[20] = {'_','a','s','d','p','_','v',1, 3};
    memcpy(bye + 12, NODE_A, 8);
    link_proto_parse(bye, 20);
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, (float)link_proto_bpm());
}

void test_bpm_resets_when_last_peer_expires(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);  // 120 BPM
    link_proto_parse(buf, len);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 120.0, (float)link_proto_bpm());

    s_test_now = 15001;
    link_proto_tick();
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, (float)link_proto_bpm());
}

void test_bpm_held_while_one_peer_remains(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);  // 120 BPM
    link_proto_parse(buf, len);
    make_alive_packet(buf, &len, NODE_B, 500000LL);
    link_proto_parse(buf, len);

    uint8_t bye[20] = {'_','a','s','d','p','_','v',1, 3};
    memcpy(bye + 12, NODE_A, 8);
    link_proto_parse(bye, 20);                        // one peer left
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 120.0, (float)link_proto_bpm());
}

void test_timeline_false_before_any_tmln_seen(void) {
    LinkTimeline tl;
    TEST_ASSERT_FALSE(link_proto_timeline(&tl));
}

void test_timeline_parses_full_24_byte_payload(void) {
    uint8_t buf[512]; int len;
    // beatOrigin fixed-point: 4.5 beats -> llround(4.5 * 1e6) = 4500000
    int64_t beat_origin_micro = 4500000LL;
    int64_t time_origin_us    = 123456789012LL;
    make_alive_packet_full(buf, &len, NODE_A, 500000LL, beat_origin_micro, time_origin_us);
    TEST_ASSERT_TRUE(link_proto_parse(buf, len));

    LinkTimeline tl;
    TEST_ASSERT_TRUE(link_proto_timeline(&tl));
    TEST_ASSERT_EQUAL_INT64(500000LL, tl.micros_per_beat);
    TEST_ASSERT_EQUAL_INT64(beat_origin_micro, tl.beat_origin_micro);
    TEST_ASSERT_EQUAL_INT64(time_origin_us, tl.time_origin_us);
}

void test_tick_expiry_drops_mep4_for_expired_peer(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    append_mep4(buf, &len, 0xC0A80105u, 20808);
    link_proto_parse(buf, len);                       // NODE_A at slot 0, has mep4

    s_test_now = 15001;                                // past TTL
    link_proto_tick();
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());

    // A new peer reusing the now-vacant slot 0 must not inherit A's mep4.
    make_alive_packet(buf, &len, NODE_B, 500000LL);    // no mep4
    link_proto_parse(buf, len);
    TEST_ASSERT_EQUAL_INT(1, link_proto_peers());

    uint32_t ip; uint16_t port;
    TEST_ASSERT_FALSE(link_proto_peer_endpoint(0, &ip, &port));
}

void test_byebye_drops_mep4_for_removed_peer(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    append_mep4(buf, &len, 0xC0A80105u, 20808);
    link_proto_parse(buf, len);

    uint8_t bye[20] = {'_','a','s','d','p','_','v',1, 3};
    memcpy(bye + 12, NODE_A, 8);
    link_proto_parse(bye, 20);
    TEST_ASSERT_EQUAL_INT(0, link_proto_peers());

    make_alive_packet(buf, &len, NODE_B, 500000LL);    // reuses slot 0, no mep4
    link_proto_parse(buf, len);

    uint32_t ip; uint16_t port;
    TEST_ASSERT_FALSE(link_proto_peer_endpoint(0, &ip, &port));
}

void test_mep4_survives_subsequent_packet_without_mep4(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    append_mep4(buf, &len, 0xC0A80105u, 20808);
    link_proto_parse(buf, len);

    // Refresh from the same peer, this time with no mep4 TLV.
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    link_proto_parse(buf, len);

    uint32_t ip; uint16_t port;
    TEST_ASSERT_TRUE(link_proto_peer_endpoint(0, &ip, &port));
    TEST_ASSERT_EQUAL_UINT32(0xC0A80105u, ip);
    TEST_ASSERT_EQUAL_UINT16(20808, port);
}

void test_peer_endpoint_false_when_no_mep4(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);   // no mep4 appended
    link_proto_parse(buf, len);

    uint32_t ip; uint16_t port;
    TEST_ASSERT_FALSE(link_proto_peer_endpoint(0, &ip, &port));
}

void test_peer_endpoint_false_for_out_of_range_index(void) {
    uint32_t ip; uint16_t port;
    TEST_ASSERT_FALSE(link_proto_peer_endpoint(0, &ip, &port));
    TEST_ASSERT_FALSE(link_proto_peer_endpoint(-1, &ip, &port));
}

void test_mep4_attached_to_matching_peer(void) {
    uint8_t buf[512]; int len;
    make_alive_packet(buf, &len, NODE_A, 500000LL);
    append_mep4(buf, &len, 0xC0A80105u /*192.168.1.5*/, 20808);
    TEST_ASSERT_TRUE(link_proto_parse(buf, len));

    uint32_t ip; uint16_t port;
    TEST_ASSERT_TRUE(link_proto_peer_endpoint(0, &ip, &port));
    TEST_ASSERT_EQUAL_UINT32(0xC0A80105u, ip);
    TEST_ASSERT_EQUAL_UINT16(20808, port);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mep4_attached_to_matching_peer);
    RUN_TEST(test_tick_expiry_drops_mep4_for_expired_peer);
    RUN_TEST(test_byebye_drops_mep4_for_removed_peer);
    RUN_TEST(test_mep4_survives_subsequent_packet_without_mep4);
    RUN_TEST(test_peer_endpoint_false_when_no_mep4);
    RUN_TEST(test_peer_endpoint_false_for_out_of_range_index);
    RUN_TEST(test_timeline_false_before_any_tmln_seen);
    RUN_TEST(test_timeline_parses_full_24_byte_payload);
    RUN_TEST(test_peer_expires_after_ttl);
    RUN_TEST(test_bpm_resets_when_last_peer_byebyes);
    RUN_TEST(test_bpm_resets_when_last_peer_expires);
    RUN_TEST(test_bpm_held_while_one_peer_remains);
    RUN_TEST(test_valid_packet_extracts_bpm);
    RUN_TEST(test_wrong_magic_ignored);
    RUN_TEST(test_short_packet_ignored);
    RUN_TEST(test_alive_adds_peer);
    RUN_TEST(test_two_peers_counted);
    RUN_TEST(test_same_peer_not_double_counted);
    RUN_TEST(test_byebye_removes_peer);
    RUN_TEST(test_bpm_updates_to_latest);
    return UNITY_END();
}
