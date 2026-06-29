#ifndef RTA_BINS_H
#define RTA_BINS_H

#define RTA_BIN_COUNT 100

/* Center frequency (Hz) for each /meters/4 RTA bin.
 *
 * Derived by log-frequency interpolation of the 31 ISO 1/3-octave GEQ-band
 * anchors: TOAST_GEQ_BIN[i] (cli/toast_logic.c) ↔ the ISO center frequencies,
 * with 1 kHz pinned to bin 56 (pistonphone-verified). Verify the off-anchor
 * bins on hardware with a console sine sweep via `XAir_ToastSaver --rta-probe`.
 */
extern const float RTA_BIN_FREQ[RTA_BIN_COUNT];

/* Nearest RTA bin index (0..99) for a frequency, inverse of RTA_BIN_FREQ. */
int rta_bin_for_freq(float hz);

#endif /* RTA_BINS_H */
