#include "midi_bpm_calc.h"

float midi_bpm_calc(uint32_t span_us, int window) {
    if (span_us == 0 || window < 2) return 0.0f;
    // span covers (window-1) intervals; a beat is `window` intervals.
    return 60000000.0f * (float)(window - 1) / ((float)span_us * (float)window);
}
