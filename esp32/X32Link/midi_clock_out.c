// Pure 24-PPQN MIDI-clock-out scheduler — see midi_clock_out.h.
#include "midi_clock_out.h"
#include <math.h>

void midi_clock_out_reset(MidiClockOut* s) {
    s->last_tick = 0;
    s->primed    = false;
}

int midi_clock_out_ticks_due(MidiClockOut* s, double beats_now, int max_burst) {
    if (beats_now < 0.0) beats_now = 0.0;
    int64_t tick = (int64_t)floor(beats_now * (double)MIDI_CLOCK_OUT_PPQN);

    if (!s->primed) {           // first observation: align the grid, no backlog
        s->primed    = true;
        s->last_tick = tick;
        return 0;
    }

    int64_t due = tick - s->last_tick;
    if (due <= 0) return 0;     // backward or same 1/24-beat slot

    if (max_burst > 0 && due > (int64_t)max_burst) {
        // Big forward jump (tempo re-origin / long stall): realign to the new
        // position instead of flooding the bus with a catch-up burst.
        s->last_tick = tick;
        return 0;
    }

    s->last_tick = tick;
    return (int)due;
}
