#pragma once

// WiFi — 2.4 GHz only (ESP32-S3 does not support 5 GHz)
#define WIFI_SSID            "X32-Emulator"
#define WIFI_PASS            ""

// X32 emulator (XIAO ESP32-S3 SoftAP at 192.168.4.1)
#define XR18_IP              "192.168.4.1"
#define XR18_PORT            10023

// Ableton Link → FX slot
#define LINK_FX_SLOT         1           // FX slot 1–4; must be a delay type
#define LINK_DEFAULT_BPM     120.0f      // initial tempo offered to Link session
#define LINK_BPM_THRESHOLD   0.5f        // min BPM change that triggers an OSC write
#define LINK_POLL_MS         50          // FreeRTOS task poll interval (ms)
