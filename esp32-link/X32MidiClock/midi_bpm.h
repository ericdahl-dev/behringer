#pragma once

void  midi_bpm_init(void);
float midi_bpm_update(void);  // returns 0.0 if no clock or timed out
