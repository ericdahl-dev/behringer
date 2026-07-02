// Pure trigger-decision for the WiFi-down status LED — see wifi_down_blink.h.
#include "wifi_down_blink.h"

bool wifi_down_blink_due(uint32_t now_ms, uint32_t last_blink_ms,
                          uint32_t interval_ms, bool wifi_connected) {
    if (wifi_connected) return false;
    // Unsigned subtraction wraps correctly across millis() rollover (~49.7
    // days), same trick every millis()-timed caller in this codebase relies
    // on (see wifi_try_connect()'s `millis() - start` above it in
    // X32Link.ino).
    return (uint32_t)(now_ms - last_blink_ms) >= interval_ms;
}
