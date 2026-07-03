#pragma once
// Thin Arduino/WiFiUDP glue for the Link measurement (ping/pong) client.
// All protocol logic (TLV build/parse, sample math, GhostXForm derivation)
// lives in the pure, host-tested link_measurement.{h,c} — this file is
// socket + timing glue only. Not host-testable (needs real WiFiUDP).

// Opens this module's own unicast UDP socket (separate from
// link_listener.cpp's multicast discovery socket / port 20808) and resets
// measurement state. Call once, after WiFi is up — mirrors
// link_listener_begin().
void link_measurement_io_begin();

// Service the measurement client: detects the 0->1 reference-peer trigger
// (or a replaced reference peer) via link_proto_peer_endpoint(), drives the
// in-flight ping/pong exchange, the 50ms watchdog retry, and the
// 5-consecutive-timeout (250ms) failure path. Call every loop tick,
// alongside link_listener_poll()/tick().
void link_measurement_io_poll();

// LNK-026 diagnostic (temporary): count of detected timeline epoch resets
// (transport re-origins) that invalidated the committed GhostXForm. Surfaced on
// /phasedbg so the fix can be verified even when WiFi drops during the re-origin.
#include <stdint.h>
uint32_t link_measurement_io_epoch_resets(void);
