// Pure trigger-decision for the WiFi-down status LED (LNK-023).
//
// led_task() in X32Link.ino polls this every 5ms alongside the existing
// phase-wrap beat flash (led_phase.c) — when WiFi isn't connected (initial
// connect attempt or parked in AP fallback), the LED should fire an
// occasional triple-blink distinct from the per-beat flash. This is just
// the "is it time yet" decision, pulled out so it's host-testable without
// FreeRTOS/Arduino/WiFi.h. Mirrors led_phase.c's pattern: pure fn, actual
// GPIO sequencing stays in led_task().
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Should the triple-blink fire on this poll? `now_ms`/`last_blink_ms` are
// both millis()-style timestamps (wraparound-safe via unsigned subtraction
// — same trick millis() callers always rely on). `wifi_connected` mirrors
// `WiFi.status() == WL_CONNECTED`; when true, always false regardless of
// timing (the gate is here too so the decision is fully described by its
// inputs, same rationale as led_phase_should_flash()'s `valid` param).
bool wifi_down_blink_due(uint32_t now_ms, uint32_t last_blink_ms,
                          uint32_t interval_ms, bool wifi_connected);

#ifdef __cplusplus
}
#endif
