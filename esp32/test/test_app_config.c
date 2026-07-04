#include "unity.h"
#include "app_config.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

void test_defaults_model_is_xr18(void) {
    AppConfig cfg;
    config_defaults(&cfg);
    TEST_ASSERT_EQUAL_INT(MODEL_XR18, cfg.model);
}

void test_defaults_fx_slot_is_1(void) {
    AppConfig cfg;
    config_defaults(&cfg);
    TEST_ASSERT_EQUAL_INT(1, cfg.fx_slot);
}

void test_defaults_ip_set(void) {
    AppConfig cfg;
    config_defaults(&cfg);
    TEST_ASSERT_TRUE(cfg.mixer_ip[0] != '\0');
}

void test_xr18_port(void) {
    TEST_ASSERT_EQUAL_INT(10024, config_model_port(MODEL_XR18));
}

void test_x32_port(void) {
    TEST_ASSERT_EQUAL_INT(10023, config_model_port(MODEL_X32));
}

void test_xr18_slot_max(void) {
    TEST_ASSERT_EQUAL_INT(4, config_model_slot_max(MODEL_XR18));
}

void test_x32_slot_max(void) {
    TEST_ASSERT_EQUAL_INT(8, config_model_slot_max(MODEL_X32));
}

void test_validate_rejects_slot_zero(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.fx_slot = 0;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_rejects_slot_above_max_xr18(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model   = MODEL_XR18;
    cfg.fx_slot = 5;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_rejects_slot_above_max_x32(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model   = MODEL_X32;
    cfg.fx_slot = 9;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_rejects_empty_ip(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.mixer_ip[0] = '\0';
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_accepts_valid_xr18(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model   = MODEL_XR18;
    cfg.fx_slot = 4;
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_validate_accepts_valid_x32(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model   = MODEL_X32;
    cfg.fx_slot = 8;
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_defaults_input_source_is_link(void) {
    AppConfig cfg; config_defaults(&cfg);
    TEST_ASSERT_EQUAL_INT(0, cfg.input_source);  // 0 = Ableton Link
}

void test_validate_accepts_midi_source(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.input_source = 1;  // USB MIDI
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_validate_rejects_source_above_range(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.input_source = 2;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_rejects_negative_source(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.input_source = -1;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_defaults_fdr_chan_is_32(void) {
    AppConfig cfg; config_defaults(&cfg);
    TEST_ASSERT_EQUAL_INT(32, cfg.fdr_chan_count);
}

void test_validate_rejects_bad_chan_count(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.fdr_chan_count = 17;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_accepts_chan_16(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.fdr_chan_count = 16;
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_defaults_quantum_beats_is_4(void) {
    AppConfig cfg; config_defaults(&cfg);
    TEST_ASSERT_EQUAL_INT(4, cfg.quantum_beats);
}

void test_validate_rejects_quantum_beats_zero(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.quantum_beats = 0;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_rejects_quantum_beats_above_16(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.quantum_beats = 17;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

void test_validate_accepts_quantum_beats_1(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.quantum_beats = 1;
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_validate_accepts_quantum_beats_16(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.quantum_beats = 16;
    TEST_ASSERT_TRUE(config_validate(&cfg));
}

void test_validate_rejects_negative_quantum_beats(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.quantum_beats = -1;
    TEST_ASSERT_FALSE(config_validate(&cfg));
}

// LNK-032: config_set_model owns the model→slot dependency for both editors.
void test_set_model_x32_keeps_valid_high_slot(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model = MODEL_X32; cfg.fx_slot = 8;
    config_set_model(&cfg, MODEL_X32);
    TEST_ASSERT_EQUAL_INT(MODEL_X32, cfg.model);
    TEST_ASSERT_EQUAL_INT(8, cfg.fx_slot);          // 8 is valid on X32
}

void test_set_model_xr18_clamps_slot_to_max(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model = MODEL_X32; cfg.fx_slot = 8;
    config_set_model(&cfg, MODEL_XR18);
    TEST_ASSERT_EQUAL_INT(MODEL_XR18, cfg.model);
    TEST_ASSERT_EQUAL_INT(4, cfg.fx_slot);          // clamped 8 -> 4
}

void test_set_model_ignores_invalid_model(void) {
    AppConfig cfg; config_defaults(&cfg);
    cfg.model = MODEL_X32; cfg.fx_slot = 7;
    config_set_model(&cfg, 99);
    TEST_ASSERT_EQUAL_INT(MODEL_X32, cfg.model);     // unchanged
    TEST_ASSERT_EQUAL_INT(7, cfg.fx_slot);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_set_model_x32_keeps_valid_high_slot);
    RUN_TEST(test_set_model_xr18_clamps_slot_to_max);
    RUN_TEST(test_set_model_ignores_invalid_model);
    RUN_TEST(test_defaults_fdr_chan_is_32);
    RUN_TEST(test_validate_rejects_bad_chan_count);
    RUN_TEST(test_validate_accepts_chan_16);
    RUN_TEST(test_defaults_quantum_beats_is_4);
    RUN_TEST(test_validate_rejects_quantum_beats_zero);
    RUN_TEST(test_validate_rejects_quantum_beats_above_16);
    RUN_TEST(test_validate_accepts_quantum_beats_1);
    RUN_TEST(test_validate_accepts_quantum_beats_16);
    RUN_TEST(test_validate_rejects_negative_quantum_beats);
    RUN_TEST(test_defaults_model_is_xr18);
    RUN_TEST(test_defaults_fx_slot_is_1);
    RUN_TEST(test_defaults_ip_set);
    RUN_TEST(test_xr18_port);
    RUN_TEST(test_x32_port);
    RUN_TEST(test_xr18_slot_max);
    RUN_TEST(test_x32_slot_max);
    RUN_TEST(test_validate_rejects_slot_zero);
    RUN_TEST(test_validate_rejects_slot_above_max_xr18);
    RUN_TEST(test_validate_rejects_slot_above_max_x32);
    RUN_TEST(test_validate_rejects_empty_ip);
    RUN_TEST(test_validate_accepts_valid_xr18);
    RUN_TEST(test_validate_accepts_valid_x32);
    RUN_TEST(test_defaults_input_source_is_link);
    RUN_TEST(test_validate_accepts_midi_source);
    RUN_TEST(test_validate_rejects_source_above_range);
    RUN_TEST(test_validate_rejects_negative_source);
    return UNITY_END();
}
