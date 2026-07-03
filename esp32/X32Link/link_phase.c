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

bool link_phase_timeline_epoch_reset(int64_t prev_time_origin_us, int64_t new_time_origin_us) {
    // A genuine re-origin resets the ghost clock, so time_origin jumps backward by
    // far more than gossip jitter / UDP reordering (sub-second). Threshold at 1s:
    // real re-origins jump by many seconds (~510s observed on hardware), while an
    // out-of-order gossip is never close.
    const int64_t RESET_THRESHOLD_US = 1000000;  // 1 second
    return (prev_time_origin_us - new_time_origin_us) > RESET_THRESHOLD_US;
}
