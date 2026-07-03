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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_hit);
    return UNITY_END();
}
