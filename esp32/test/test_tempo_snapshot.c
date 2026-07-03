#include "unity.h"
#include "tempo_snapshot.h"

void setUp(void) {}
void tearDown(void) {}

static void test_publish_read_roundtrip(void) {
    tempo_snapshot_publish(120.5f, 2.5f, true, 4);
    TempoSnapshot s;
    tempo_snapshot_read(&s);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_EQUAL_FLOAT(120.5f, s.bpm);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, s.phase);
    TEST_ASSERT_EQUAL_INT(4, s.quantum);
}

static void test_invalid_yields_sentinel_phase(void) {
    tempo_snapshot_publish(120.0f, 2.5f, false, 4);   // not valid, but phase given
    TempoSnapshot s;
    tempo_snapshot_read(&s);
    TEST_ASSERT_FALSE(s.valid);
    TEST_ASSERT_TRUE(s.phase < 0.0f);                 // forced to sentinel
}

// The contradiction the old torn-read path could emit: valid=true, phase=-1.
static void test_never_valid_with_negative_phase(void) {
    tempo_snapshot_publish(120.0f, -1.0f, true, 4);   // contradictory input
    TempoSnapshot s;
    tempo_snapshot_read(&s);
    TEST_ASSERT_FALSE(s.valid && s.phase < 0.0f);     // impossible by construction
    TEST_ASSERT_FALSE(s.valid);                        // coerced to not-valid
}

static void test_read_null_safe(void) {
    tempo_snapshot_read(NULL);                         // must not crash
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_publish_read_roundtrip);
    RUN_TEST(test_invalid_yields_sentinel_phase);
    RUN_TEST(test_never_valid_with_negative_phase);
    RUN_TEST(test_read_null_safe);
    return UNITY_END();
}
