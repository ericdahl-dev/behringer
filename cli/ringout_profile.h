#ifndef RINGOUT_PROFILE_H
#define RINGOUT_PROFILE_H

/* Ring-out profile: the result of a proactive ring-out run — the static GEQ
 * notch set plus the measured gain-before-feedback margin and the target it was
 * made for. Saved by --save-profile (ring-out) and applied by --load-profile
 * (reactive) so live protection starts from the soundcheck result. Pure
 * serialize/parse, no I/O — host-testable. See docs/ringout-design.md. */

#define RINGOUT_PROFILE_BANDS 31

typedef struct {
    char  model[8];                          /* "XR18" / "X32" */
    int   bus;                               /* monitor bus the ring-out targeted */
    int   fx_slot;                           /* FX slot holding the TEQ (1-4) */
    float margin_db;                         /* gain-before-feedback margin achieved */
    char  created[24];                       /* ISO-8601 timestamp (no space), optional */
    float band_db[RINGOUT_PROFILE_BANDS];    /* per-GEQ-par notch depth (dB); 0 = flat */
} RingoutProfile;

/* Serialize to a human-readable text buffer. Returns bytes written (excluding
 * the NUL), or -1 if the buffer is too small. */
int ringout_profile_serialize(const RingoutProfile *p, char *buf, int buflen);

/* Parse text produced by ringout_profile_serialize into *p. Returns 0 on
 * success, -1 on malformed input. Bands not named in the text default to 0. */
int ringout_profile_parse(const char *text, RingoutProfile *p);

#endif /* RINGOUT_PROFILE_H */
