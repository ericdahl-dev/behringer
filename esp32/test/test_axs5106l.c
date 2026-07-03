#include "unity.h"
#include "axs5106l.h"

void setUp(void)    {}
void tearDown(void) {}

// Single finger: 12-bit X/Y each split [status|hi_nibble][lo_byte]; the status
// high nibble must be masked off. X=0x123, Y=0x045, with 0xF0 status noise.
void test_single_touch_decode_masks_status_nibble(void) {
    uint8_t r[AXS5106L_REPORT_LEN] = {0};
    r[1] = 1;       // finger count
    r[2] = 0xF1;    // x_hi: status noise 0xF0 | coord nibble 0x1
    r[3] = 0x23;    // x_lo
    r[4] = 0xF0;    // y_hi: status noise | coord nibble 0x0
    r[5] = 0x45;    // y_lo
    r[6] = 0x77;    // pressure (ignored)

    axs_touch_t t;
    int rc = axs5106l_parse(r, sizeof(r), &t);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT8(1, t.count);
    TEST_ASSERT_EQUAL_UINT8(1, t.points_len);
    TEST_ASSERT_EQUAL_UINT16(0x123, t.points[0].x);
    TEST_ASSERT_EQUAL_UINT16(0x045, t.points[0].y);
}

// No touch: count 0 -> no points decoded, success.
void test_no_touch(void) {
    uint8_t r[AXS5106L_REPORT_LEN] = {0};
    axs_touch_t t;
    TEST_ASSERT_EQUAL_INT(0, axs5106l_parse(r, sizeof(r), &t));
    TEST_ASSERT_EQUAL_UINT8(0, t.count);
    TEST_ASSERT_EQUAL_UINT8(0, t.points_len);
}

// Two fingers decode independently; point 1 lives at offset 8.
void test_two_touch_decode(void) {
    uint8_t r[AXS5106L_REPORT_LEN] = {0};
    r[1] = 2;
    r[2] = 0x01; r[3] = 0x23;   // p0 x=0x123
    r[4] = 0x00; r[5] = 0x45;   // p0 y=0x045
    r[8] = 0x0A; r[9] = 0xBC;   // p1 x=0xABC
    r[10] = 0x00; r[11] = 0x14; // p1 y=0x014
    axs_touch_t t;
    TEST_ASSERT_EQUAL_INT(0, axs5106l_parse(r, sizeof(r), &t));
    TEST_ASSERT_EQUAL_UINT8(2, t.points_len);
    TEST_ASSERT_EQUAL_UINT16(0x123, t.points[0].x);
    TEST_ASSERT_EQUAL_UINT16(0xABC, t.points[1].x);
    TEST_ASSERT_EQUAL_UINT16(0x014, t.points[1].y);
}

// Controller reports more fingers than we buffer -> points_len clamps to MAX,
// count still reflects the raw report (no OOB write past points[MAX]).
void test_count_over_max_clamps(void) {
    uint8_t r[AXS5106L_REPORT_LEN] = {0};
    r[1] = 5;   // more than AXS5106L_MAX_POINTS
    axs_touch_t t;
    TEST_ASSERT_EQUAL_INT(0, axs5106l_parse(r, sizeof(r), &t));
    TEST_ASSERT_EQUAL_UINT8(5, t.count);
    TEST_ASSERT_EQUAL_UINT8(AXS5106L_MAX_POINTS, t.points_len);
}

// Guards: short buffer and NULLs return -1.
void test_guards(void) {
    uint8_t r[AXS5106L_REPORT_LEN] = {0};
    axs_touch_t t;
    TEST_ASSERT_EQUAL_INT(-1, axs5106l_parse(r, AXS5106L_REPORT_LEN - 1, &t));
    TEST_ASSERT_EQUAL_INT(-1, axs5106l_parse(NULL, sizeof(r), &t));
    TEST_ASSERT_EQUAL_INT(-1, axs5106l_parse(r, sizeof(r), NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_touch_decode_masks_status_nibble);
    RUN_TEST(test_no_touch);
    RUN_TEST(test_two_touch_decode);
    RUN_TEST(test_count_over_max_clamps);
    RUN_TEST(test_guards);
    return UNITY_END();
}
