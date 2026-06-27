#include "midi_bpm.h"
#include "midi_clock.h"
#include "midi_bpm_calc.h"
#include "config.h"
#include <Arduino.h>

void midi_bpm_init() {
    // nothing to initialise — state lives in midi_clock module
}

float midi_bpm_update() {
    uint32_t count = midi_clock_pulse_count();
    if (count < (uint32_t)MCK_CLOCK_WINDOW) return 0.0f;

    // Timeout: no pulse for MCK_CLOCK_TIMEOUT_MS → clock stopped
    uint32_t last_us = midi_clock_last_pulse_us();
    if ((uint32_t)(micros() - last_us) > (uint32_t)(MCK_CLOCK_TIMEOUT_MS * 1000UL))
        return 0.0f;

    // MCK_CLOCK_WINDOW timestamps span (MCK_CLOCK_WINDOW-1) intervals.
    // Scale up to a full beat (MCK_CLOCK_WINDOW intervals = 24 PPQN).
    uint32_t newest = midi_clock_get_timestamp(MCK_CLOCK_WINDOW - 1);
    uint32_t oldest = midi_clock_get_timestamp(0);
    uint32_t span_us = newest - oldest;  // uint32_t wraps correctly

    return midi_bpm_calc(span_us, MCK_CLOCK_WINDOW);  // pure, host-tested
}
