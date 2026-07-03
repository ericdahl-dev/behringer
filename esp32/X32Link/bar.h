#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A bar is `quantum` beats — ARC-003. One place owns that assumption so non-4/4
// quanta (g_config.quantum_beats is 1..16) get the right refresh/resend cadence,
// instead of the hardcoded `4 *` scattered across bpm_publisher.c and
// X32MidiClock.ino. (MIDI is 24 PPQN, so a bar is 24*quantum pulses — but the
// two call sites here work in beats and milliseconds, not pulses.)

// Total beats in `bars` bars. Returns 0 for quantum < 1 or bars < 1.
int bar_beats(int quantum, int bars);

// Milliseconds for `bars` bars at `bpm`. Returns 0.0 for bpm <= 0, quantum < 1,
// or bars < 1.
double bar_ms(float bpm, int quantum, int bars);

#ifdef __cplusplus
}
#endif
