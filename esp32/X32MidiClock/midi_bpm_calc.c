#include "midi_bpm_calc.h"

float midi_bpm_calc(uint32_t span_us, int window) {
    if (span_us == 0 || window < 2) return 0.0f;
    // span covers (window-1) intervals; a beat is `window` intervals.
    return 60000000.0f * (float)(window - 1) / ((float)span_us * (float)window);
}

float midi_phase_calc(uint32_t pulse_count, int quantum) {
    if (quantum < 1) return 0.0f;
    uint32_t pulses_per_bar = 24u * (uint32_t)quantum;
    return (float)(pulse_count % pulses_per_bar) / 24.0f;
}

bool midi_phase_valid(uint32_t pulse_count, int quantum) {
    if (quantum < 1) return false;
    return pulse_count >= 24u * (uint32_t)quantum;
}
