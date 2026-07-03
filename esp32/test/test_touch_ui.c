#include "unity.h"
#include "touch_ui.h"
#include <string.h>

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

// Task 4: ui_apply_settings_tap — mutate config per tapped field.
void test_ui_apply_settings_tap(void) {
    AppConfig c;
    // Model change keeps fx_slot when still in range.
    config_defaults(&c); c.model = MODEL_XR18; c.fx_slot = 4;
    ui_apply_settings_tap(&c, UI_F_MODEL_X32);
    TEST_ASSERT_EQUAL_INT(MODEL_X32, c.model);
    TEST_ASSERT_EQUAL_INT(4, c.fx_slot);
    // Model change clamps fx_slot to new max.
    config_defaults(&c); c.model = MODEL_X32; c.fx_slot = 7;
    ui_apply_settings_tap(&c, UI_F_MODEL_XR);
    TEST_ASSERT_EQUAL_INT(MODEL_XR18, c.model);
    TEST_ASSERT_EQUAL_INT(4, c.fx_slot);   // clamp
    // Source toggle.
    config_defaults(&c); c.input_source = 0;
    ui_apply_settings_tap(&c, UI_F_SRC_MIDI);
    TEST_ASSERT_EQUAL_INT(1, c.input_source);
    ui_apply_settings_tap(&c, UI_F_SRC_LINK);
    TEST_ASSERT_EQUAL_INT(0, c.input_source);
    // Quantum inc/dec clamped 1..16.
    config_defaults(&c); c.quantum_beats = 16;
    ui_apply_settings_tap(&c, UI_F_QUANTUM_INC);
    TEST_ASSERT_EQUAL_INT(16, c.quantum_beats);
    c.quantum_beats = 1;
    ui_apply_settings_tap(&c, UI_F_QUANTUM_DEC);
    TEST_ASSERT_EQUAL_INT(1, c.quantum_beats);
    c.quantum_beats = 4;
    ui_apply_settings_tap(&c, UI_F_QUANTUM_INC);
    TEST_ASSERT_EQUAL_INT(5, c.quantum_beats);
    ui_apply_settings_tap(&c, UI_F_QUANTUM_DEC);
    TEST_ASSERT_EQUAL_INT(4, c.quantum_beats);
    // Slot pick within range.
    config_defaults(&c); c.model = MODEL_X32;
    ui_apply_settings_tap(&c, UI_F_SLOT_1 + 5);
    TEST_ASSERT_EQUAL_INT(6, c.fx_slot);
    // Slot pick out of range for model is ignored.
    config_defaults(&c); c.model = MODEL_XR18; c.fx_slot = 2;
    ui_apply_settings_tap(&c, UI_F_SLOT_1 + 5);   // slot 6 > XR18 max 4
    TEST_ASSERT_EQUAL_INT(2, c.fx_slot);
}

// Task 5: ui_kbd_apply — append printable, '\b' backspace, ignore when full.
void test_ui_kbd_apply(void) {
    char b[4] = "";
    ui_kbd_apply(b, sizeof b, 'a');
    ui_kbd_apply(b, sizeof b, 'b');
    TEST_ASSERT_EQUAL_STRING("ab", b);
    ui_kbd_apply(b, sizeof b, '\b');
    TEST_ASSERT_EQUAL_STRING("a", b);
    char e[4] = "";
    ui_kbd_apply(e, sizeof e, '\b');       // backspace empty = no-op
    TEST_ASSERT_EQUAL_STRING("", e);
    char f[3] = "ab";
    ui_kbd_apply(f, sizeof f, 'c');        // full = ignored
    TEST_ASSERT_EQUAL_STRING("ab", f);
}

// Task 6: ui_ip_apply — only accept [0-9.] and '\b'.
void test_ui_ip_apply(void) {
    char ip[16] = "";
    ui_ip_apply(ip, sizeof ip, '1');
    ui_ip_apply(ip, sizeof ip, '9');
    ui_ip_apply(ip, sizeof ip, '2');
    ui_ip_apply(ip, sizeof ip, '.');
    TEST_ASSERT_EQUAL_STRING("192.", ip);
    ui_ip_apply(ip, sizeof ip, 'x');       // non-ip char ignored
    TEST_ASSERT_EQUAL_STRING("192.", ip);
    ui_ip_apply(ip, sizeof ip, '\b');      // backspace still works
    TEST_ASSERT_EQUAL_STRING("192", ip);
    // A valid mixer_ip built this way passes the existing validator.
    AppConfig c;
    config_defaults(&c);
    strcpy(c.mixer_ip, "192.168.0.10");
    TEST_ASSERT_TRUE(config_validate(&c));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ui_hit);
    RUN_TEST(test_ui_bpm_str);
    RUN_TEST(test_ui_phase_angle);
    RUN_TEST(test_ui_apply_settings_tap);
    RUN_TEST(test_ui_kbd_apply);
    RUN_TEST(test_ui_ip_apply);
    return UNITY_END();
}
