#include <string.h>
#include "x32tap_logic.h"

#define TAP_MAX_MS 3000

float ms_to_normalized(int ms) {
	float f = (float)ms / TAP_MAX_MS;
	if (f < 0.f) f = 0.f;
	if (f > 1.f) f = 1.f;
	return f;
}

int normalized_to_ms(float f) {
	return (int)(f * TAP_MAX_MS);
}

/* X32 meter responses start with "/meters/6" and carry gate meter as a
 * little-endian float at byte offset 28 within the blob data. */
float parse_gate_meter(const char *buf, int len) {
	float f;
	if (len < 32 || strncmp(buf, "/meters/6", 9) != 0)
		return -1.0f;
	memcpy(&f, buf + 28, sizeof(float));
	return f;
}

/* 40-byte OSC /meters message matching X32TapW format:
 *   [0-7]   /meters\0         address
 *   [8-15]  ,siii\0\0\0       type tag
 *   [16-27] /meters/6\0\0\0   string arg
 *   [28-31] 0x00000000        int: channel index (byte 31 = channel-1)
 *   [32-39] 0x00000000 x2     int args (unused)
 */
int bpm_from_ms(int ms) {
	if (ms <= 0) return 0;
	return 60000 / ms;
}

const MixerProfile mixer_xair = {"xair", 10024, 4, 16};
const MixerProfile mixer_x32  = {"x32",  10023, 8, 32};

const MixerProfile *mixer_by_name(const char *name) {
	if (name && (strcmp(name, "xair") == 0 || strcmp(name, "xr18") == 0))
		return &mixer_xair;
	return &mixer_x32;
}

void build_meters_subscribe(char *buf, int channel) {
	memset(buf, 0, 40);
	memcpy(buf,      "/meters",   7);
	memcpy(buf + 8,  ",siii",     5);
	memcpy(buf + 16, "/meters/6", 9);
	buf[31] = (char)(channel - 1);
}
