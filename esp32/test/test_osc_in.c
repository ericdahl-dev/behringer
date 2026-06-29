#include "unity.h"
#include "osc_in.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

/* tiny OSC encoder (mirrors osc_out.c) for building test packets */
static int put_str(uint8_t *b, int o, const char *s) {
    int n = (int)strlen(s) + 1;
    memcpy(b + o, s, n); o += n;
    while (o % 4) b[o++] = 0;
    return o;
}
static int put_be32(uint8_t *b, int o, uint32_t v) {
    b[o++] = (uint8_t)(v >> 24); b[o++] = (uint8_t)(v >> 16);
    b[o++] = (uint8_t)(v >> 8);  b[o++] = (uint8_t)v;
    return o;
}

void test_parses_float_fader_message(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/ch/01/mix/fader");
    o = put_str(b, o, ",f");
    float f = 0.75f; uint32_t bits; memcpy(&bits, &f, 4);
    o = put_be32(b, o, bits);

    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_STRING("/ch/01/mix/fader", m.address);
    TEST_ASSERT_EQUAL_STRING("f", m.tags);
    TEST_ASSERT_EQUAL_INT(1, m.n_args);
    TEST_ASSERT_EQUAL_INT(OSC_ARG_FLOAT, m.args[0].type);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.75f, m.args[0].val.f);
}

void test_parses_int_mute_message(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/ch/03/mix/on");
    o = put_str(b, o, ",i");
    o = put_be32(b, o, 0);
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_STRING("/ch/03/mix/on", m.address);
    TEST_ASSERT_EQUAL_INT(1, m.n_args);
    TEST_ASSERT_EQUAL_INT(OSC_ARG_INT, m.args[0].type);
    TEST_ASSERT_EQUAL_INT(0, m.args[0].val.i);
}

void test_parses_multiple_args(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/foo");
    o = put_str(b, o, ",if");
    o = put_be32(b, o, 5);
    float f = 0.5f; uint32_t bits; memcpy(&bits, &f, 4); o = put_be32(b, o, bits);
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_INT(2, m.n_args);
    TEST_ASSERT_EQUAL_INT(5, m.args[0].val.i);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, m.args[1].val.f);
}

void test_parses_string_arg(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/info");
    o = put_str(b, o, ",s");
    o = put_str(b, o, "XR18");
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_INT(OSC_ARG_STRING, m.args[0].type);
    TEST_ASSERT_EQUAL_STRING("XR18", m.args[0].val.s);
}

void test_empty_tags_no_args(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/xremote");
    o = put_str(b, o, ",");
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_STRING("/xremote", m.address);
    TEST_ASSERT_EQUAL_INT(0, m.n_args);
}

void test_unknown_tag_stops_parsing(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/foo");
    o = put_str(b, o, ",fb");                 // float then a blob we don't decode
    float f = 1.0f; uint32_t bits; memcpy(&bits, &f, 4); o = put_be32(b, o, bits);
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(0, osc_in_parse(b, o, &m));
    TEST_ASSERT_EQUAL_INT(1, m.n_args);       // kept the float, stopped at 'b'
}

void test_rejects_non_address(void) {
    uint8_t b[8] = {'x','y','z',0,0,0,0,0};
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(-1, osc_in_parse(b, 8, &m));
}

void test_rejects_truncated_arg(void) {
    uint8_t b[64]; int o = 0;
    o = put_str(b, o, "/foo");
    o = put_str(b, o, ",f");
    b[o++] = 0x3f; b[o++] = 0x80;              // only 2 of 4 float bytes
    osc_msg_t m;
    TEST_ASSERT_EQUAL_INT(-1, osc_in_parse(b, o, &m));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_float_fader_message);
    RUN_TEST(test_parses_int_mute_message);
    RUN_TEST(test_parses_multiple_args);
    RUN_TEST(test_parses_string_arg);
    RUN_TEST(test_empty_tags_no_args);
    RUN_TEST(test_unknown_tag_stops_parsing);
    RUN_TEST(test_rejects_non_address);
    RUN_TEST(test_rejects_truncated_arg);
    return UNITY_END();
}
