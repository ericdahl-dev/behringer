#include "unity.h"
#include "bar.h"

void setUp(void) {}
void tearDown(void) {}

static void test_bar_beats(void) {
    TEST_ASSERT_EQUAL_INT(4, bar_beats(4, 1));   // 4/4, one bar
    TEST_ASSERT_EQUAL_INT(6, bar_beats(3, 2));   // 3/4, two bars
    TEST_ASSERT_EQUAL_INT(7, bar_beats(7, 1));   // odd meter
    TEST_ASSERT_EQUAL_INT(0, bar_beats(0, 1));   // invalid quantum
    TEST_ASSERT_EQUAL_INT(0, bar_beats(4, 0));   // invalid bars
}

static void test_bar_ms(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, (float)bar_ms(120.0f, 4, 1));  // 4 beats @120 = 4*500ms
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1500.0f, (float)bar_ms(120.0f, 3, 1));  // 3-beat bar
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4000.0f, (float)bar_ms(120.0f, 4, 2));  // two bars
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,    (float)bar_ms(0.0f, 4, 1));    // bpm <= 0
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bar_beats);
    RUN_TEST(test_bar_ms);
    return UNITY_END();
}
