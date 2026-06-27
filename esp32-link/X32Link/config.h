#pragma once

// Link timing — internal, not user-configurable
#define LINK_DEFAULT_BPM     120.0f
#define LINK_BPM_THRESHOLD   0.5f
#define LINK_POLL_MS         50
#define LINK_SEND_INTERVAL_MS 500
#define LINK_REFRESH_BARS     1

// First-boot defaults — overridden after first web config save
#define DEFAULT_WIFI_SSID    "X32-Emulator"
#define DEFAULT_WIFI_PASS    ""
#define DEFAULT_MIXER_IP     "192.168.4.1"
#define DEFAULT_MODEL        1    // MODEL_XR18
#define DEFAULT_FX_SLOT      1
