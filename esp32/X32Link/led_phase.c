// Pure phase-wrap-detection decision for the beat LED — see led_phase.h.
#include "led_phase.h"

bool led_phase_should_flash(float prev_phase, float phase, bool valid) {
    if (!valid) return false;
    if (prev_phase < 0.0f) return false;  // first reading since (re)sync: seed only
    return phase < prev_phase;            // wrapped: near-1 -> near-0
}
