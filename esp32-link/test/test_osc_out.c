#include "unity.h"
#include "osc_out.h"
#include <string.h>
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

void test_bpm_120_normalized(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1667f, bpm_to_normalized(120.0f));
}

void test_bpm_60_normalized(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3333f, bpm_to_normalized(60.0f));
}

void test_bpm_180_normalized(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1111f, bpm_to_normalized(180.0f));
}

void test_slow_bpm_clamped_to_one(void) {
    // 15 BPM → 4000 ms → clamped to 3000 ms → 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, bpm_to_normalized(15.0f));
}

void test_zero_bpm_returns_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bpm_to_normalized(0.0f));
}

void test_packet_length_is_24(void) {
    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(24, osc_build_fx_delay(buf, 1, 0.1667f));
}

void test_packet_path_prefix(void) {
    uint8_t buf[32] = {0};
    osc_build_fx_delay(buf, 1, 0.0f);
    TEST_ASSERT_EQUAL_UINT8('/', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('f', buf[1]);
    TEST_ASSERT_EQUAL_UINT8('x', buf[2]);
    TEST_ASSERT_EQUAL_UINT8('/', buf[3]);
}

void test_packet_slot_in_path(void) {
    uint8_t buf[32] = {0};
    osc_build_fx_delay(buf, 3, 0.0f);
    TEST_ASSERT_EQUAL_UINT8('3', buf[4]);
}

void test_packet_type_tag(void) {
    uint8_t buf[32] = {0};
    osc_build_fx_delay(buf, 1, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(',', buf[16]);
    TEST_ASSERT_EQUAL_UINT8('f', buf[17]);
    TEST_ASSERT_EQUAL_UINT8(0,   buf[18]);
    TEST_ASSERT_EQUAL_UINT8(0,   buf[19]);
}

void test_packet_float_roundtrips(void) {
    uint8_t buf[32] = {0};
    osc_build_fx_delay(buf, 1, 0.1667f);
    uint32_t be = ((uint32_t)buf[20]<<24)|((uint32_t)buf[21]<<16)
                 |((uint32_t)buf[22]<<8)|(uint32_t)buf[23];
    float decoded;
    memcpy(&decoded, &be, 4);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1667f, decoded);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bpm_120_normalized);
    RUN_TEST(test_bpm_60_normalized);
    RUN_TEST(test_bpm_180_normalized);
    RUN_TEST(test_slow_bpm_clamped_to_one);
    RUN_TEST(test_zero_bpm_returns_zero);
    RUN_TEST(test_packet_length_is_24);
    RUN_TEST(test_packet_path_prefix);
    RUN_TEST(test_packet_slot_in_path);
    RUN_TEST(test_packet_type_tag);
    RUN_TEST(test_packet_float_roundtrips);
    return UNITY_END();
}
