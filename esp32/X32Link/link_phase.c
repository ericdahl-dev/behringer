// Pure bar-quantized phase math for the Link adapter — see link_phase.h.
#include "link_phase.h"
#include <math.h>

double link_phase_beats_now(LinkTimeline timeline, int64_t ghost_now_us) {
    return ((double)timeline.beat_origin_micro / 1e6)
         + (double)(ghost_now_us - timeline.time_origin_us)
           / (double)timeline.micros_per_beat;
}

double link_phase_from_beats(double beats_now, double quantum) {
    double phase_now = fmod(beats_now, quantum);
    if (phase_now < 0) phase_now += quantum;
    return phase_now;
}
