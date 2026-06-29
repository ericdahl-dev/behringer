#pragma once
#include "osc_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*osc_on_message_t)(const osc_msg_t *msg);

/* Open a UDP socket to the mixer, send /xinfo + /xremote, and dispatch every
 * parsed incoming OSC message to `cb`. Shared by the client control firmwares
 * (FaderDisp / SafeMutes / MidiOscIttt). */
void osc_listener_begin(const char *mixer_ip, int port, osc_on_message_t cb);

/* Service the socket (call each loop tick): receives + dispatches, and re-sends
 * /xremote inside the mixer's subscription window. */
void osc_listener_poll(void);

/* Send a raw OSC packet to the mixer from the listener's socket, so any reply
 * comes back to this same socket (e.g. /ch/NN/config/name queries). */
void osc_listener_send(const uint8_t *buf, int len);

void osc_listener_end(void);

#ifdef __cplusplus
}
#endif
