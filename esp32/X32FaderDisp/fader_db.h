#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define FADER_DB_MINUS_INF (-90.0f)   /* the "-oo" floor */

/* X32 fader law: OSC float f (0..1) → dB. Returns FADER_DB_MINUS_INF at f<=0.
 * (Exact piecewise from docs/xr18-xair-osc-cheatsheet.md.) */
float fader_to_db(float f);

/* Format a fader float as a scribble-strip dB string ("+10.0", "0.0", "-6.0",
 * "-oo"), ≤7 chars. Writes into out[n]. */
void fader_db_str(float f, char *out, int n);

#ifdef __cplusplus
}
#endif
