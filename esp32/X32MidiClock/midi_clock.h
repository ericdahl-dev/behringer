#pragma once
#include <stdint.h>

void     midi_clock_init(void);
uint32_t midi_clock_pulse_count(void);
uint32_t midi_clock_last_pulse_us(void);
uint32_t midi_clock_get_timestamp(uint32_t index);  // index 0 = oldest in window
bool     midi_clock_beat_flag(void);                // returns true once per beat, then clears

// LNK-027 MIDI clock OUT: bring up the shared USBMIDI endpoint (idempotent —
// safe alongside midi_clock_init()) and send one 0xF8 real-time clock tick.
// The IN path (midi_clock_init/poll) and the OUT path are mutually exclusive by
// input_source, but both share this one USBMIDI device.
void     midi_clock_usb_begin(void);
void     midi_clock_send_f8(void);
