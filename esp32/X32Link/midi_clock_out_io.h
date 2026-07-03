#pragma once
// LNK-027 glue for Link → USB-MIDI clock OUT. Owns the high-priority timer task
// that reads the monotonic beat position from the tempo_source seam, runs the
// pure midi_clock_out.c scheduler, and writes 0xF8 ticks over USB-MIDI. Not
// host-testable (needs FreeRTOS + TinyUSB) — all timing math lives in the pure
// module. Call once, after USB + the phase pipeline are up, only when
// input_source == Link and midi_clock_out_enable.

void midi_clock_out_io_begin(void);
