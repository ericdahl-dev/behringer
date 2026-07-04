# 4. Touch screen carries real-time controls; static config lives in the web UI

Date: 2026-07-04
Status: accepted

## Context

X32Link has two config surfaces: the **web config UI** (rack-panel page served
over WiFi) and the on-device **1.47" touch LCD settings screen** (LNK-014/015).
During the LNK-032 config-edit review a gap surfaced: `midi_clock_out_enable`
(LNK-027) can be set from the web UI but has no control on the touch settings
screen — and that screen is already full (source / model / FX slot / quantum /
mixer IP / Write & Reboot).

The question: make room on the touch screen for every config field so the two
editors stay at parity, or accept that the touch screen intentionally shows a
subset?

## Decision

The **web UI is the complete config surface**. The **touch screen carries only
controls a user would plausibly change in real time, at the rack** — plus live
status (the phase wheel, BPM) — not static setup that requires a reboot to take
effect.

- Reboot-gated setup — WiFi credentials, mixer IP/model, FX slot, and
  `midi_clock_out_enable` — is a "configure once, Write & Reboot" concern and
  belongs on the web page.
- The touch screen is for on-the-fly changes (e.g. bar quantum) and live readout.

So `midi_clock_out_enable` having no touch control is **intentional, not a bug**:
it's a reboot-gated config flag, not a real-time control.

## Consequences

- The touch settings screen stays uncluttered and focused; not every web field
  needs a touch twin — the two editors are deliberately **not** at full parity.
- The LNK-032 shared config-edit helpers (`config_set_model`, and future
  `config_set_*`) still let both editors share clamp/dependency rules for the
  fields they *do* both expose.
- Future architecture reviews should **not** re-flag "the touch UI can't set
  field X" as a defect for reboot-gated config fields — that is this decision,
  not drift. (This closes the LNK-032 "touch can't set midi_clock_out" item as
  won't-do rather than deferred.)
- If a field becomes real-time-relevant later, it can earn a touch control then.
