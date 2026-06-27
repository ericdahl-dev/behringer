#pragma once

#define MCK_DEFAULT_BPM       120.0f
#define MCK_BPM_THRESHOLD     1.5f     // ignore jitter smaller than this
#define MCK_SEND_INTERVAL_MS  500      // minimum ms between OSC sends
#define MCK_REFRESH_BARS      1        // resend every N bars (4 beats)
#define MCK_CLOCK_WINDOW      24       // pulses to average (1 beat = 24 PPQN)
#define MCK_CLOCK_TIMEOUT_MS  2000     // stop sending OSC if no pulse for this long
#define MCK_POLL_MS           10       // BPM task poll interval

// First-boot defaults — overridden after first web config save
#define DEFAULT_WIFI_SSID    "X32-Emulator"
#define DEFAULT_WIFI_PASS    ""
#define DEFAULT_MIXER_IP     "192.168.4.1"
#define DEFAULT_MODEL        1         // MODEL_XR18
#define DEFAULT_FX_SLOT      1
#define DEFAULT_INPUT_SOURCE 1         // TEMPO_SRC_MIDI (this firmware is MIDI-only)
