// Pure free-running beat generator — see beat_synth.h.
#include "beat_synth.h"

void beat_synth_reset(BeatSynth* s) {
    s->last_ms = 0;
}

bool beat_synth_step(BeatSynth* s, uint32_t now_ms, float bpm) {
    if (bpm <= 0.0f) return false;
    uint32_t interval = (uint32_t)(60000.0f / bpm);
    if ((uint32_t)(now_ms - s->last_ms) >= interval) {
        s->last_ms = now_ms;
        return true;
    }
    return false;
}
