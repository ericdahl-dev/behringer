// Pure send-policy for the tempo→OSC bridge: given the latest BPM reading,
// decide whether to emit OSC this tick. Extracted from bpm_task so the timing
// rules (change threshold, per-bar refresh, min send interval, bpm=0 gate) are
// host-testable. No Arduino deps — the caller passes the clock in.

#include "bpm_publisher.h"
#include <math.h>

static float    s_tracked;
static float    s_threshold;
static uint32_t s_send_interval;
static int      s_refresh_bars;
static uint32_t s_last_send;
static uint32_t s_last_bar;

void bpm_publisher_init(float threshold, uint32_t send_interval_ms, int refresh_bars) {
    s_tracked       = 0.0f;
    s_threshold     = threshold;
    s_send_interval = send_interval_ms;
    s_refresh_bars  = refresh_bars;
    s_last_send     = 0;
    s_last_bar      = 0;
}

PublishDecision bpm_publisher_step(float bpm, bool active, uint32_t now_ms) {
    PublishDecision d = { false, bpm, false };

    bool changed = bpm > 0.0f && fabsf(bpm - s_tracked) >= s_threshold;
    if (changed) s_tracked = bpm;

    bool refresh = bpm > 0.0f && active &&
                   (uint32_t)(now_ms - s_last_bar) >=
                       (uint32_t)(4 * s_refresh_bars * 60000.0f / bpm);

    if ((changed || refresh) && (uint32_t)(now_ms - s_last_send) >= s_send_interval) {
        d.send      = true;
        d.refresh   = refresh && !changed;
        s_last_send = now_ms;
        if (refresh) s_last_bar = now_ms;
    }
    if (changed) s_last_bar = now_ms;
    return d;
}
