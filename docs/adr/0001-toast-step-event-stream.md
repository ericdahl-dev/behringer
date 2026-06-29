# 1. toast_step returns an event stream, not a single action

Date: 2026-06-29
Status: accepted

## Context

The reactive detection loop (`run_detection()`) is inline in
`XAir_ToastSaver.c` and untested. T-029 extracts it as a pure `toast_step()`
into `toast_logic.c`. The natural model to copy is `ringout_step()`, which
returns ONE action per frame — because ring-out genuinely does one thing at a
time (raise, back off, or place one notch).

Reactive detection is not symmetric. Each frame does a **release pass** over
all active notches (every releasing band emits its own ramp toward flat) plus a
**detection pass** that may place one new notch. So a single band notching while
several others ramp out is normal — a single-action return cannot represent it
without either losing events or pushing the multi-band ramp state back onto the
caller.

## Decision

`toast_step()` fills a caller-provided array of `ToastAction` events and returns
the count:

```c
int toast_step(ToastState *st, const float *bins, const float *baseline,
               const ToastConfig *cfg, ToastAction *out, int max);
```

`ToastAction = { ToastOp op; int par; float val; int bin; float level; }`,
`op ∈ {TOAST_NOTCH, TOAST_RELEASE_RAMP, TOAST_RELEASE_DONE}`. The function is
time-free (frame counters only, no `now_sec()`), does no OSC sends, and owns all
per-band machine state in `ToastState`. The caller sends `val` to the mixer for
every event and logs only `NOTCH`/`RELEASE_DONE`. Wall-clock timestamps live in
the caller. The baseline is rolled by the caller (`update_baseline`) and passed
in `const`.

## Consequences

- The same event stream feeds OSC sends **and** the T-031 TUI log panel — one
  seam, two consumers.
- Passes the deletion test: removing `toast_step` would re-concentrate all
  confirm/narrowness/release/ramp complexity, with nothing left in the caller.
- Asymmetric with `ringout_step` — a reader must learn two shapes. Accepted
  because the two loops are genuinely different problems.
- The single-band acceptance tests still hold: their frames yield a count of 1.
