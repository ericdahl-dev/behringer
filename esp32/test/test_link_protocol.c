#include "unity.h"
#include "link_protocol.h"
#include <string.h>
#include <stdint.h>

// Controllable clock — strong def overrides the weak shim in link_protocol.c
static uint32_t s_test_now;
uint32_t link_proto_millis(void) { return s_test_now; }

void setUp(void)    { s_test_now = 0; link_proto_reset(); }
void tearDown(void) {}

// Craft a minimal Alive packet with a Timeline TLV.
// microsPerBeat for 120 BPM = 60e6 / 120 = 500000 us
static void make_alive_packet(uint8_t* buf, int* len,
                               uint8_t nodeid[8], int64_t us_per_beat) {
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

    // microsPerBeat (int64 BE)
    for (int s = 56; s >= 0; s -= 8) buf[i++] = (us_per_beat >> s) & 0xff;
    // beatOrigin (int64, zeros)
    for (int s = 0; s < 8; s++) buf[i++] = 0;
    // timeOrigin (int64, zeros)
    for (int s = 0; s < 8; s++) buf[i++] = 0;

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

int main(void) {
    UNITY_BEGIN();
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
