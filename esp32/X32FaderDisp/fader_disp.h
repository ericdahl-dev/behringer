#pragma once
#include "osc_in.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start: cache original channel names (queries /ch/NN/config/name) so they can
 * be restored later. chan_count is 16 (XR18) or 32 (X32). */
void fader_disp_begin(int chan_count);

/* osc_listener callback: on /ch/NN/mix/fader, write the dB string to that
 * channel's scribble name (throttled); on /ch/NN/config/name, cache originals. */
void fader_disp_handle(const osc_msg_t *msg);

/* Write the cached original names back (graceful stop / disable). */
void fader_disp_restore(void);

#ifdef __cplusplus
}
#endif
