# Context — Behringer mixer tooling

Glossary of domain language. Definitions only — no implementation detail.

## ToastSaver
A feedback-destroyer for X32/XR18 mixers: it watches the mixer's RTA spectrum
and tames acoustic feedback by cutting EQ bands. Exists as a CLI/TUI, an iOS
app, and (planned) standalone ESP32 firmware.

## Notch
A single EQ band cut placed to suppress feedback at one frequency. May be
**reactive** (placed live when feedback is detected) or **pinned** — a notch
loaded from a saved profile that is never auto-released.

## Reactive mode
ToastSaver listens to the RTA and places/releases notches live as feedback
comes and goes. A notch is placed only after a narrow peak is **confirmed**
(persists several frames); it is **released** (ramped back to flat) once its
band falls quiet for a release window.

## Ring-out mode
A proactive pre-show pass: ToastSaver slowly raises a monitor bus's gain and
notches each ringing frequency as it appears, establishing headroom before the
band hits a hard **ceiling**. Stops at a target gain **margin**, the ceiling,
or a max notch count. The bus fader is always **restored** to its start on
completion or abort.

## Doctor / pre-flight
A readiness check that the mixer can actually be notched: each bus/LR output is
in GEQ or TEQ EQ mode, the reactive FX slot holds a GEQ/TEQ, and the RTA is
streaming. **Doctor** is the explicit `--doctor` check (with `--fix`);
**pre-flight** is the same check run automatically before reactive/ring-out,
aborting if the mixer is not ready.

## Ceiling / margin / release window
- **Ceiling** — the hard upper gain bound ring-out never crosses.
- **Margin** — gain headroom achieved above the start point before feedback.
- **Release window** — the time a notched band must stay quiet before its
  notch ramps out.
