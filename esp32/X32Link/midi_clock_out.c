// 24-PPQN MIDI clock OUT — a thin adapter over the shared clock_ticker engine.
// See midi_clock_out.h.
#include "midi_clock_out.h"

void midi_clock_out_reset(MidiClockOut* s) {
    clock_ticker_reset(s);
}

int midi_clock_out_ticks_due(MidiClockOut* s, double beats_now, int max_burst) {
    return clock_ticker_ticks_due(s, beats_now, MIDI_CLOCK_OUT_PPQN, max_burst);
}
