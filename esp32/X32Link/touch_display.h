#ifndef TOUCH_DISPLAY_H
#define TOUCH_DISPLAY_H

// On-device 1.47" LCD + touch for the Waveshare ESP32-S3-Touch-LCD-1.47.
// Raw LovyanGFX (no LVGL) driving the JD9853 panel (ST7789 command set), plus
// the AXS5106L capacitive touch controller (pure parse in axs5106l.c). Gated by
// HAS_TOUCH_DISPLAY so screenless boards are unaffected. LNK-014.

// Panel + touch + LVGL-free splash init. Call once in setup(), early (shows the
// splash while WiFi connects).
void touch_display_begin(void);

// Poll touch, echo raw coordinates to Serial + on-screen. Call from loop().
void touch_display_tick(void);

#endif // TOUCH_DISPLAY_H
