#pragma once

// Link timing — internal, not user-configurable
#define LINK_DEFAULT_BPM     120.0f
#define LINK_BPM_THRESHOLD   0.5f
#define LINK_POLL_MS         50
#define LINK_SEND_INTERVAL_MS 500
#define LINK_REFRESH_BARS     1

// USB MIDI clock input — per-input knobs (used when input_source = MIDI)
#define MCK_BPM_THRESHOLD    1.5f     // MIDI jitter is higher than Link
#define MCK_POLL_MS          10
#define MCK_PPQN             24       // MIDI clock pulses per beat (protocol constant; drives the per-beat flag)
#define MCK_CLOCK_WINDOW     48       // pulses to average for BPM (2 beats) — smooths 1ms-poll timestamp jitter
#define MCK_CLOCK_TIMEOUT_MS 2000     // no pulse for this long → clock stopped

// First-boot defaults — overridden after first web config save
#define DEFAULT_WIFI_SSID    "X32-Emulator"
#define DEFAULT_WIFI_PASS    ""
#define DEFAULT_MIXER_IP     "192.168.4.1"
#define DEFAULT_MODEL        1    // MODEL_XR18
#define DEFAULT_FX_SLOT      1
#define DEFAULT_INPUT_SOURCE 0    // TEMPO_SRC_LINK
#define DEFAULT_FDR_ENABLE     0    // FaderDisp off on non-FDR firmware
#define DEFAULT_FDR_CHAN_COUNT 32
#define DEFAULT_QUANTUM_BEATS  4    // bar-quantized phase: beats per bar

// Board profile (LNK-025) — the Waveshare ESP32-S3-Touch-LCD-1.47 carries the
// 1.47" LCD, so its single board flag implies the on-device display UI: one
// -DBOARD_WAVESHARE_S3_TOUCH_LCD_147 yields the full LNK-014/015 touch build
// instead of two flags (BOARD_* + HAS_TOUCH_DISPLAY) hand-kept in sync. This
// MUST live in a shared header every relevant translation unit includes —
// deriving it inside X32Link.ino alone leaves touch_display.cpp (a separate TU)
// unaware, compiling its body out and undefined-ref'ing touch_display_begin().
#if defined(BOARD_WAVESHARE_S3_TOUCH_LCD_147) && !defined(HAS_TOUCH_DISPLAY)
#define HAS_TOUCH_DISPLAY
#endif
