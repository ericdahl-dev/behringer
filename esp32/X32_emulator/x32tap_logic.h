#ifndef X32TAP_LOGIC_H
#define X32TAP_LOGIC_H

float ms_to_normalized(int ms);
int   normalized_to_ms(float f);

/* Returns the gate meter level [0.0,1.0] from a /meters/6 OSC response,
 * or -1.0 if the packet is not a meter response. */
float parse_gate_meter(const char *buf, int len);

/* Fills buf (must be 40 bytes) with the OSC /meters subscribe message
 * for the given channel (1-32). */
void build_meters_subscribe(char *buf, int channel);

/* Converts a tap interval in milliseconds to BPM. Returns 0 for ms <= 0. */
int bpm_from_ms(int ms);

/* Mixer profiles */
typedef struct {
	const char *name;
	int         port;
	int         fx_slots;
	int         channels;
} MixerProfile;

extern const MixerProfile mixer_xair;
extern const MixerProfile mixer_x32;

const MixerProfile *mixer_by_name(const char *name);

#endif
