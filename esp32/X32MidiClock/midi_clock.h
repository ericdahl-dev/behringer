#pragma once
#include <stdint.h>

void     midi_clock_init(void);
uint32_t midi_clock_pulse_count(void);
uint32_t midi_clock_last_pulse_us(void);
uint32_t midi_clock_get_timestamp(uint32_t index);  // index 0 = oldest in window
bool     midi_clock_beat_flag(void);                // returns true once per beat, then clears
