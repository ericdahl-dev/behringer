// Ableton Link measurement protocol — pure logic (LNK-018).
//
// Pinger-only client: ping a peer's `mep4` unicast endpoint (discovered by
// LNK-017's link_proto_peer_endpoint()), exchange a few ping/pong round
// trips, and derive this device's clock offset relative to the session's
// shared virtual clock (GHostTime). We never implement the PingResponder
// (reply) side — this firmware never broadcasts Alive/Response packets, so
// no peer will ever ping us back.
//
// Wire format (big-endian), distinct from link_protocol.c's discovery
// header:
//   magic       = "_link_v\x01"  (8 bytes)
//   messageType = uint8          kPing=1, kPong=2
//   ... TLV payload: key(4 BE) + size(4 BE) + value(size), same envelope
//       convention as link_protocol.c's discovery TLVs ...
//
// No WiFi/Arduino dependency here — testable on host. See
// link_measurement_io.cpp for the thin WiFiUDP unicast glue that drives
// this module on real hardware.
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cap of accumulated offset samples before we start evicting the oldest
// (ring buffer). Ticket calls 32 "enough for our purposes" — official Link
// SDK accumulates up to 100 but that's conservative, not a protocol
// minimum.
#define LINK_MEASUREMENT_MAX_SAMPLES 32

// Sample count at which an in-flight attempt is considered "good enough" to
// commit a GhostXForm from (io.cpp decides when to stop pinging once it
// reaches this many accumulated samples). Not specified numerically by the
// ticket ("see Acceptance Criteria for the exact target" — AC doesn't
// actually give one); picked as a judgment call: by round 4 a pinger with
// PrevGHostTime echoing has produced ~7-8 samples (2 per round after round
// 1), which is enough for a stable median while still converging in well
// under the 250ms failure window at LAN RTT speed.
#define LINK_MEASUREMENT_READY_SAMPLES 8

// Worst-case encoded length of a Ping message (magic + type + HostTime TLV
// + PrevGHostTime TLV).
#define LINK_MEASUREMENT_PING_MAX_LEN 41

typedef struct {
    int64_t intercept_us;  // signed microsecond offset, host -> ghost
    bool    valid;
} LinkGhostXForm;

// slope is hardcoded 1.0 in the real protocol too — Link does not model
// clock drift in this path.
int64_t link_ghost_xform_host_to_ghost(LinkGhostXForm xform, int64_t host_us);

/* ---------------------------------------------------------------------- */
/* Wire format: TLV build/parse for ping/pong messages                    */
/* ---------------------------------------------------------------------- */

// Build a Ping message (what we send) into buf.
//   Round 1:    HostTime{host_time_us} only           (has_prev_ghost=false)
//   Round N>1:  HostTime{host_time_us} + PrevGHostTime{prev_ghost_us}
// Returns the number of bytes written, or 0 if buf_cap is too small.
int link_measurement_build_ping(uint8_t* buf, int buf_cap,
                                 int64_t host_time_us,
                                 bool has_prev_ghost, int64_t prev_ghost_us);

typedef struct {
    bool    has_host_time;
    int64_t host_time_us;
    bool    has_prev_ghost_time;
    int64_t prev_ghost_time_us;
} LinkPingFields;

// Parse a Ping message body back out. Mainly exists for host-test
// round-tripping of link_measurement_build_ping() — production code (the
// pinger) never needs to parse its own pings.
bool link_measurement_parse_ping(const uint8_t* buf, int len, LinkPingFields* out);

typedef struct {
    bool    has_ghost_time;
    int64_t ghost_time_us;
    bool    has_host_time;       // our own echoed HostTime from the ping this pong replies to
    int64_t host_time_us;
    bool    has_prev_ghost_time; // our own echoed PrevGHostTime (round 2+)
    int64_t prev_ghost_time_us;
} LinkPongFields;

// Parse a Pong message (what we receive). TLV order is not guaranteed;
// looked up by key. SessionMembership ('sess') is present but ignored — we
// don't track session identity changes here. Returns false if the magic or
// message type don't match (not a Pong message at all); a structurally
// valid Pong with missing fields still returns true with the corresponding
// has_* flags left false.
bool link_measurement_parse_pong(const uint8_t* buf, int len, LinkPongFields* out);

/* ---------------------------------------------------------------------- */
/* Sample buffer / median / offset math                                   */
/* ---------------------------------------------------------------------- */

void link_measurement_samples_reset(void);
int  link_measurement_samples_count(void);

// Given one received pong and our local clock reading at the moment it
// arrived (h_recv), compute sample_a (always, if HostTime+GHostTime
// present) and sample_b (only if PrevGHostTime also present) per the
// closed-form offset formulas, and push whichever apply into the capped
// ring buffer (oldest evicted on overflow). Returns the number of samples
// pushed (0, 1, or 2).
int link_measurement_add_pong_samples(int64_t h_recv, const LinkPongFields* pong);

// Median of the currently accumulated samples. Returns false if the buffer
// is empty.
bool link_measurement_median(double* out_median);

/* ---------------------------------------------------------------------- */
/* Attempt lifecycle — state owned here, driven by named events from the  */
/* Arduino glue (link_measurement_io.cpp), which is the only piece that   */
/* knows about real sockets/timing.                                       */
/* ---------------------------------------------------------------------- */

// Resets all module state: sample buffer, committed GhostXForm, active flag.
void link_measurement_reset(void);

// Starts a fresh measurement attempt: clears the sample buffer, marks
// active=true. Does not touch any previously committed GhostXForm.
void link_measurement_attempt_begin(void);

// Ends the in-flight attempt.
//   success=true:  derive a GhostXForm from the current sample buffer's
//                  median and commit it (valid=true) — unless the buffer
//                  is empty, in which case this is a no-op on the xform.
//   success=false: leave the existing committed GhostXForm (if any)
//                  untouched rather than overwriting it with garbage, per
//                  the ticket's failure-handling rule.
// Either way, active becomes false.
void link_measurement_attempt_end(bool success);

LinkGhostXForm link_measurement_current_xform(void);
bool           link_measurement_active(void);

#ifdef __cplusplus
}
#endif
