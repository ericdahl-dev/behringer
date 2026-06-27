#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Where the BPM comes from. Stored in NVS as input_source.
typedef enum { TEMPO_SRC_LINK = 0, TEMPO_SRC_MIDI = 1 } TempoSourceKind;

// Select the active input. Call once at boot before pre_net/begin.
void     tempo_source_select(int kind);

void     tempo_source_pre_net(void);   // pre-WiFi init (USB MIDI enumerates here)
void     tempo_source_begin(void);     // post-WiFi init (Link joins multicast here)
void     tempo_source_poll(void);      // service the input every bpm_task tick
float    tempo_source_bpm(void);       // current BPM; 0.0 = no live signal
bool     tempo_source_active(void);    // peers>0 / pulses within timeout
bool     tempo_source_beat(void);      // true once per beat, self-clears (LED)

float    tempo_source_threshold(void); // per-input change threshold
uint32_t tempo_source_poll_ms(void);   // per-input poll interval

#ifdef __cplusplus
}
#endif
