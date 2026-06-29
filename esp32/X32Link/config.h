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
#define MCK_CLOCK_WINDOW     24       // pulses to average (1 beat = 24 PPQN)
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
