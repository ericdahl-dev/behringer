#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The send-policy decision for one tempo reading. */
typedef struct {
    bool  send;     /* emit OSC this tick? */
    float bpm;      /* value to emit (valid when send) */
    bool  refresh;  /* true if this is a periodic refresh, not a change */
} PublishDecision;

/* Configure the policy. threshold = min BPM change to trigger a send;
 * send_interval_ms = floor between sends; refresh_bars = resend every N bars. */
void bpm_publisher_init(float threshold, uint32_t send_interval_ms, int refresh_bars);

/* Feed the latest reading each tick. bpm<=0 means "no signal" (never sends);
 * active gates the periodic refresh (peers present / pulses flowing). */
PublishDecision bpm_publisher_step(float bpm, bool active, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
