#include "unity.h"
#include "fader_db.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* Boundary cases from the X32 fader law (FDR-002 acceptance). */
void test_top_is_plus_10(void)   { TEST_ASSERT_FLOAT_WITHIN(0.01f,  10.0f, fader_to_db(1.00f)); }
void test_three_quarter_is_0(void){ TEST_ASSERT_FLOAT_WITHIN(0.01f,  0.0f, fader_to_db(0.75f)); }
void test_half_is_minus_10(void) { TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, fader_to_db(0.50f)); }
void test_quarter_is_minus_30(void){ TEST_ASSERT_FLOAT_WITHIN(0.01f,-30.0f, fader_to_db(0.25f)); }
void test_zero_is_minus_inf(void){ TEST_ASSERT_FLOAT_WITHIN(0.01f, FADER_DB_MINUS_INF, fader_to_db(0.0f)); }

void test_segment_boundary_minus_60(void) {  /* 0.0625 joins the bottom segments */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -60.0f, fader_to_db(0.0625f));
}

void test_str_formats(void) {
    char b[8];
    fader_db_str(1.00f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("+10.0", b);
    fader_db_str(0.75f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("0.0",   b);
    fader_db_str(0.50f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("-10.0", b);
    fader_db_str(0.00f, b, sizeof b); TEST_ASSERT_EQUAL_STRING("-oo",   b);
}

void test_str_fits_scribble_width(void) {
    char b[8];
    for (float f = 0.0f; f <= 1.0f; f += 0.013f) {
        fader_db_str(f, b, sizeof b);
        TEST_ASSERT_TRUE(strlen(b) <= 7);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_top_is_plus_10);
    RUN_TEST(test_three_quarter_is_0);
    RUN_TEST(test_half_is_minus_10);
    RUN_TEST(test_quarter_is_minus_30);
    RUN_TEST(test_zero_is_minus_inf);
    RUN_TEST(test_segment_boundary_minus_60);
    RUN_TEST(test_str_formats);
    RUN_TEST(test_str_fits_scribble_width);
    return UNITY_END();
}
