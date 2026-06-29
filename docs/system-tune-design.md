# Automated system tune — design / vision

The arc for ToastSaver: from a **feedback destroyer** into an **automated system
tune** tool. This note captures the full vision so the in-flight ring-out work
fits a bigger picture.

> **Status:** vision / not-yet-scoped. The **ring-out** half (monitors) is being
> built now — `docs/ringout-design.md`, tasks `T-021..T-025`. The **pink-noise
> response-EQ** half (mains) described here is scoped *after* `T-025`. No tune
> code exists yet.

## Two complementary procedures, one engine

A full tune is **two different procedures** — not one. They share our entire core
(RTA parse, GEQ map, OSC, the pure-`*_step()` controller pattern) but solve
different problems:

| | **Ring-out** (building now) | **Pink-noise response EQ** (new) |
|---|---|---|
| Target | monitor bus (wedge / IEM) | mains LR (and optionally monitors, pre-ring-out) |
| Problem | feedback-limited **gain** | tonal / room **response** |
| Stimulus | none (passive) + gain drive | **pink noise** from the console generator |
| Method | drive gain up, notch rings | measure RTA vs a target curve, EQ the GEQ flat |
| Loop | closed-loop with gain | open-loop with a known stimulus |
| Output | notches + gain-before-feedback margin | a flattened GEQ curve |

You **don't ring out the mains** — mains rarely feed back. Mains tuning is a
frequency-response problem, which is why it needs a *stimulus* (pink noise),
not a gain ramp.

## Pink-noise response EQ — algorithm

Pink noise has equal energy per octave, so on a 1/3-octave RTA a flat system
reads flat. Deviations from flat *are* the system+room response; EQ the inverse.

1. **Route** the console's internal generator (pink) to the output under test
   (LR, or a monitor bus). *(OSC node for the generator: TBD — confirm from the
   protocol doc; it was not in the slice of the cheat-sheet pulled so far.)*
2. **Measure**: average the `/meters/4` RTA over N seconds → a stable 31-band
   (or 100-bin → 31-band) magnitude response.
3. **Target**: a chosen curve — flat, or a gentle **house curve** (HF tilt),
   not dead-flat.
4. **Correct**: per GEQ band, `correction_dB = target_dB − measured_dB`,
   **smoothed** (1/3-oct), **limited** (cut-preferred; never fill a null;
   max boost/cut clamp), applied to the LR/bus GEQ.
5. **Iterate**: re-measure → re-correct until within tolerance or a max-iteration
   cap. Optional: **spatial averaging** over a few mic positions.

Same shape as ring-out: a pure `tune_step(state, rta_avg) → geq_correction[]`,
host-testable with synthetic pink-noise frames; platform code drives the
generator and writes the GEQ.

## The full pipeline ("as automated as possible")

1. **Mains response EQ** — pink noise → measure at a mic → flatten LR GEQ to the
   target curve.
2. **Per-monitor** — optionally pink-flatten each wedge, **then ring it out**
   (the current work) to maximise gain-before-feedback.
3. **Report** — final EQ curves + per-monitor headroom margins (and reuse the
   ring-out profile format from `T-024`).

## Honest limits (so the tune is musical, not destructive)

- **Magnitude only.** RTA gives level per band, not phase — so this **cannot**
  do time-alignment, crossover, or polarity (those need a dual-channel FFT
  transfer function, à la Smaart). "As much as possible" with the XR18 RTA =
  response EQ + ring-out, **not** full system alignment. Be upfront about this.
- **Don't blindly invert the RTA.** Naive auto-EQ chases room modes and comb
  filtering. Guardrails: heavy smoothing, cut-preferred / limited boost, a target
  house curve, optional spatial averaging.
- **Mic matters.** A measurement mic (or at least a known one) makes the curve
  meaningful; a random mic biases it. Same calibration caveat as the SPL ideas
  in `ideas.md`.

## Architecture fit

A **third mode** alongside reactive + ring-out — `--tune` — same pattern as the
rest of ToastSaver:

- **Pure**: `tune_logic.{c,h}` with `tune_step()` (host-tested) — and the same
  core ports to `ToastSaverCore` / ESP32 later.
- **Platform**: drive the internal generator, write the LR/bus GEQ, average the
  RTA. Reuses `osc_*`, `fader_*`, `db_to_geq_float`, `bin_to_geq_par`.

## Open questions before scoping `T-` tasks
- The internal **generator's OSC node** (pink-noise enable + routing) — confirm
  from the X32/XAir protocol doc.
- Default **target curve** (flat vs house tilt) and boost/cut limits.
- Whether to do **spatial averaging** in v1 or single-position.
- Measurement-mic guidance / calibration.
