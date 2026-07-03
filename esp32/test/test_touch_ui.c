#include "unity.h"
#include "touch_ui.h"

void setUp(void)    {}
void tearDown(void) {}

// Task 1: ui_hit — first rect containing (x,y), else -1. TL inclusive, BR excl.
void test_ui_hit(void) {
    ui_rect_t r[] = {{0,0,10,10},{20,20,10,10}};
    TEST_ASSERT_EQUAL_INT(1,  ui_hit(r, 2, 25, 25));
    TEST_ASSERT_EQUAL_INT(-1, ui_hit(r, 1, 50, 50));
    TEST_ASSERT_EQUAL_INT(0,  ui_hit(r, 1, 0, 0));    // TL inclusive
    TEST_ASSERT_EQUAL_INT(-1, ui_hit(r, 1, 10, 10));  // BR exclusive
    // Overlap: first match wins.
    ui_rect_t o[] = {{0,0,30,30},{10,10,10,10}};
    TEST_ASSERT_EQUAL_INT(0, ui_hit(o, 2, 15, 15));
    // Empty list.
    TEST_ASSERT_EQUAL_INT(-1, ui_hit(r, 0, 5, 5));
}

// Task 2: ui_bpm_str — "%.1f", or "--.-" when bpm <= 0.
void test_ui_bpm_str(void) {
    char b[8];
    ui_bpm_str(b, sizeof b, 120.4f); TEST_ASSERT_EQUAL_STRING("120.4", b);
    ui_bpm_str(b, sizeof b, 0.0f);   TEST_ASSERT_EQUAL_STRING("--.-", b);
    ui_bpm_str(b, sizeof b, -5.0f);  TEST_ASSERT_EQUAL_STRING("--.-", b);
    ui_bpm_str(b, sizeof b, 60.0f);  TEST_ASSERT_EQUAL_STRING("60.0", b);
}

// Task 3: ui_phase_angle — maps 0..quantum to 0..360; wraps; 0 if quantum <= 0.
void test_ui_phase_angle(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   ui_phase_angle(0.0f, 4.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, ui_phase_angle(2.0f, 4.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   ui_phase_angle(4.0f, 4.0f));  // wrap
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   ui_phase_angle(1.0f, 0.0f));  // bad
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f,  ui_phase_angle(5.0f, 4.0f));  // wrap>1
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, ui_phase_angle(3.0f, 4.0f));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_hit);
    RUN_TEST(test_ui_bpm_str);
    RUN_TEST(test_ui_phase_angle);
    return UNITY_END();
}
