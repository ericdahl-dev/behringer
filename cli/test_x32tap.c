#include <stdio.h>
#include <math.h>
#include <string.h>
#include "x32tap_logic.h"

static int passed = 0, failed = 0;

static void ok(const char *name) {
	passed++;
	printf("PASS  %s\n", name);
}

static void fail(const char *name, const char *reason) {
	failed++;
	printf("FAIL  %s: %s\n", name, reason);
}

#define ASSERT_FLOAT_EQ(name, got, want) do { \
	float _g = (got), _w = (want); \
	if (fabsf(_g - _w) < 0.0001f) ok(name); \
	else { char _buf[64]; snprintf(_buf, sizeof(_buf), "got %.4f want %.4f", _g, _w); fail(name, _buf); } \
} while (0)

#define ASSERT_INT_EQ(name, got, want) do { \
	int _g = (got), _w = (want); \
	if (_g == _w) ok(name); \
	else { char _buf[64]; snprintf(_buf, sizeof(_buf), "got %d want %d", _g, _w); fail(name, _buf); } \
} while (0)

int main(void) {

	/* ms_to_normalized: converts ms interval to [0.0, 1.0] (3000ms = 1.0) */
	ASSERT_FLOAT_EQ("ms_to_normalized: 0ms = 0.0",    ms_to_normalized(0),    0.0f);
	ASSERT_FLOAT_EQ("ms_to_normalized: 1500ms = 0.5", ms_to_normalized(1500), 0.5f);
	ASSERT_FLOAT_EQ("ms_to_normalized: 3000ms = 1.0", ms_to_normalized(3000), 1.0f);
	ASSERT_FLOAT_EQ("ms_to_normalized: 6000ms clamps to 1.0", ms_to_normalized(6000), 1.0f);

	/* normalized_to_ms: inverse of ms_to_normalized */
	ASSERT_INT_EQ("normalized_to_ms: 0.0 = 0",   normalized_to_ms(0.0f), 0);
	ASSERT_INT_EQ("normalized_to_ms: 0.5 = 1500", normalized_to_ms(0.5f), 1500);
	ASSERT_INT_EQ("normalized_to_ms: 1.0 = 3000", normalized_to_ms(1.0f), 3000);

	/* parse_gate_meter: extracts the gate meter float from a /meters/6 response.
	 * Returns -1.0 if the packet doesn't look like a meter response. */
	{
		/* build a fake /meters/6 response packet */
		char pkt[64] = {0};
		float val = 0.75f;
		unsigned char *b = (unsigned char *)&val;

		/* address: "/meters/6" null-padded to 12 bytes */
		memcpy(pkt, "/meters/6", 9);
		/* OSC type tag at offset 12: ",b" null-padded */
		pkt[12] = ','; pkt[13] = 'b';
		/* blob size at offset 16: big-endian int (we put 40 bytes) */
		pkt[16] = 0; pkt[17] = 0; pkt[18] = 0; pkt[19] = 40;
		/* gate meter float at offset 28, little-endian (X32 meter format) */
		pkt[28] = b[0]; pkt[29] = b[1]; pkt[30] = b[2]; pkt[31] = b[3];

		ASSERT_FLOAT_EQ("parse_gate_meter: returns level from valid packet",
			parse_gate_meter(pkt, 64), 0.75f);

		/* packet that isn't a meter response */
		char bad[16] = "/info";
		ASSERT_FLOAT_EQ("parse_gate_meter: returns -1 for non-meter packet",
			parse_gate_meter(bad, 16), -1.0f);
	}

	/* build_meters_subscribe: fills buf with the 40-byte /meters OSC message.
	 * Channel 1 maps to index 0, channel 32 maps to index 31. */
	{
		char buf[40];
		build_meters_subscribe(buf, 1);
		ASSERT_INT_EQ("build_meters_subscribe: address is /meters",
			strncmp(buf, "/meters", 7), 0);
		ASSERT_INT_EQ("build_meters_subscribe: /meters/6 at offset 16",
			strncmp(buf + 16, "/meters/6", 9), 0);
		ASSERT_INT_EQ("build_meters_subscribe: channel 1 -> index 0 at byte 31",
			(unsigned char)buf[31], 0);

		build_meters_subscribe(buf, 5);
		ASSERT_INT_EQ("build_meters_subscribe: channel 5 -> index 4 at byte 31",
			(unsigned char)buf[31], 4);
	}

	/* bpm_from_ms: converts tap interval to BPM */
	ASSERT_INT_EQ("bpm_from_ms: 500ms = 120 bpm", bpm_from_ms(500), 120);
	ASSERT_INT_EQ("bpm_from_ms: 1000ms = 60 bpm", bpm_from_ms(1000), 60);
	ASSERT_INT_EQ("bpm_from_ms: 0ms clamps (no div by zero)", bpm_from_ms(0), 0);

	printf("\n%d passed, %d failed\n", passed, failed);
	return failed ? 1 : 0;
}
