#pragma once

// X32FaderDisp — write live fader dB onto channel scribble strips (X32/M32).
#define FDR_NAME_REFRESH_MS   100   // min ms between scribble writes per channel
#define FDR_CHAN_COUNT        32    // X32 channels (16 for XR18)
#define FDR_RESTORE_ON_EXIT   1     // save/restore original names on disable/stop
#define FDR_XREMOTE_INTERVAL  9000  // ms between /xremote keepalives

// First-boot defaults — consumed by the shared app_config.c
#define DEFAULT_WIFI_SSID    "X32-Emulator"
#define DEFAULT_WIFI_PASS    ""
#define DEFAULT_MIXER_IP     "192.168.4.1"
#define DEFAULT_MODEL        2    // MODEL_X32 (FaderDisp is X32-only; port 10023)
#define DEFAULT_FX_SLOT      1
#define DEFAULT_INPUT_SOURCE 0
#define DEFAULT_FDR_ENABLE     1
#define DEFAULT_FDR_CHAN_COUNT 32   // X32; set 16 for XR18
#define DEFAULT_QUANTUM_BEATS  4    // bar-quantized phase: beats per bar (unused by this firmware)
