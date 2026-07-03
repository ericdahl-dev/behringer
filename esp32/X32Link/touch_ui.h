#ifndef TOUCH_UI_H
#define TOUCH_UI_H

#include <stdint.h>
#include <stddef.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Axis-aligned rectangle for hit-testing. Top-left inclusive, bottom-right
// exclusive (see ui_hit).
typedef struct { int16_t x, y, w, h; } ui_rect_t;

// Settings-screen field ids. Slots are contiguous so UI_F_SLOT_1 + n addresses
// slot n+1.
typedef enum {
    UI_F_SRC_LINK = 0,
    UI_F_SRC_MIDI,
    UI_F_MODEL_XR,
    UI_F_MODEL_X32,
    UI_F_SLOT_1,
    UI_F_SLOT_2,
    UI_F_SLOT_3,
    UI_F_SLOT_4,
    UI_F_SLOT_5,
    UI_F_SLOT_6,
    UI_F_SLOT_7,
    UI_F_SLOT_8,
    UI_F_QUANTUM_INC,
    UI_F_QUANTUM_DEC
} ui_field_t;

// Return index of first rect in r[0..n) containing (x,y), else -1.
int ui_hit(const ui_rect_t *r, int n, int x, int y);

// Format bpm as "%.1f" into out, or "--.-" when bpm <= 0.
void ui_bpm_str(char *out, size_t n, float bpm);

// Map phase 0..quantum to 0..360 degrees (wrapping); 0 if quantum <= 0.
float ui_phase_angle(float phase, float quantum);

// Mutate cfg per a tapped settings field.
void ui_apply_settings_tap(AppConfig *cfg, int field_id);

// Append a printable key to buf (NUL-terminated), '\b' backspaces. No-op when
// full or backspacing an empty buffer.
void ui_kbd_apply(char *buf, size_t cap, char key);

// Like ui_kbd_apply but only accepts [0-9.] and '\b'.
void ui_ip_apply(char *buf, size_t cap, char key);

#ifdef __cplusplus
}
#endif

#endif // TOUCH_UI_H
