#include "bpm_tracker.h"
#include <math.h>

static float s_bpm;
static float s_threshold;

void bpm_tracker_init(float initial_bpm, float threshold) {
    s_bpm       = initial_bpm;
    s_threshold = threshold;
}

bool bpm_tracker_update(float new_bpm) {
    if (fabsf(new_bpm - s_bpm) >= s_threshold) {
        s_bpm = new_bpm;
        return true;
    }
    return false;
}

float bpm_tracker_get(void) {
    return s_bpm;
}
