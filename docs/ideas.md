# Ideas backlog — creative things to build on the ESP32 platform

A brainstorm of features/products beyond the straight ports of the upstream
X32 toolset. These exploit what a cheap, always-on, **sensored** box can do that
a PC program cannot — and lean into an **Ableton Live** workflow (Live's native
sync is Ableton Link, which we already run on-device).

> Status: **ideas, not committed work.** None of these have Ordna tasks yet.
> When one is chosen, scope it the way the `FDR-`/`MUT-`/`ITT-` firmwares were
> (design note + agent-ready `todo` tasks under `esp32/tasks/`).

## Building blocks we already have

The reason most of these are "80% there":

- **On-device Ableton Link** (`esp32/X32Link/link_protocol.c`) — a ~100-line
  gossip-timeline parser. We currently read **tempo** only; Link also carries
  **beat phase + bar position (quantum)** and start/stop, which we don't use yet.
- **OSC out** to the mixer (`osc_out.*` / `osc_sender.*`) and **OSC in /
  subscribe** (`osc_in` / `osc_listener`, task `LNK-013`).
- **Mic + FFT** (INMP441 I2S, ToastSaver `TSV-` tasks) and **live RTA / meters**
  (50 ms / 20 Hz, see [`xr18-meters-osc.md`](xr18-meters-osc.md)).
- **Touchscreen** (Waveshare ESP32-S3 4.3" LVGL), **WiFi + web config + NVS**,
  **USB-MIDI** device support, and the **on-device X32 emulator** for testing.

## Key insight: use Link **phase**, not just BPM

The upstream PC tools have no musical clock. We do. Anything that should land
**on the downbeat** — a scene change, an FX throw, a light cue — can be
quantized to the next bar using Link phase. This is the single biggest
unexploited capability and underpins several ideas below.

A note on Ableton integration scope:

- **Tempo / phase / transport** → pure Ableton Link, native to Live, **free for
  us** (we already parse it).
- **Track / clip / scene / arm state** → Live does **not** speak OSC natively;
  needs a small Live-side helper (**Max for Live** device or **AbletonOSC**
  remote script) talking UDP to the ESP32. Plan for this half when an idea needs
  to know "which clip launched."

---

## A. What on-device Link already unlocks (no Live-side code)

### A1. Bar-quantized scene / snapshot recall  ⭐ top pick
Fire `/-snap/load` exactly on the **downbeat** using Link phase, so a scene
change lands perfectly on bar 1 of the next section. Impossible for the PC tools
(no phase clock). Small, unique, demos beautifully. Extends `osc_in` + Link phase.

### A2. Reverse Link — the band leads Ableton  ⭐ top pick
Today we read BPM *from* Link. Flip it: derive tempo from a tap footswitch or
**kick-onset detection** (mic + RTA) and **broadcast it as Link session tempo**,
so Live's backing tracks time-stretch to follow the live drummer instead of the
band slaving to a click. We have the parser; the work is Link's tempo-*proposal*
side.

### A3. Phase-locked stage display
Big visual count-in / downbeat flasher / "4 bars to drop" section countdown for
the band, driven by Link bar position on the touchscreen.

### A4. Beat-synced FX aligned to the bar
Drive every time-based FX parameter (gated reverb, auto-pan, tremolo, LFO mod)
off the Link timeline and **phase**, not just delay-time-from-BPM. "Everything
with a time knob follows the actual downbeat."

## B. The big play — the "X32Ableton" bridge

### B1. Two-way Live ↔ mixer automation
The missing counterpart to `X32Reaper` (no Ableton equivalent exists). Launch a
clip/scene in Live's Session view → ESP32 recalls the matching mixer scene + FX,
**quantized to the next bar via Link**. Setlist automation where Session view
drives the whole rig. Needs the Live-side helper (Max for Live / AbletonOSC) for
clip/scene state; ESP32 owns the OSC-to-mixer half + the bar timing.

### B2. Max for Live companion device
Ship a small M4L device that pairs with the ESP32 over UDP, surfacing mixer
control inside Live's UI and letting Live automation lanes drive mixer params.
M4L is the Live-side face; ESP32 does the OSC.

## C. Bridge the mixer to the rest of the room

### C1. Home Assistant / MQTT bridge
Expose mixer state as smart-home entities: "mute all" from a wall switch, scene
recall on a schedule, occupancy-triggered presets. Generalizes the MidiOscIttt
rules engine to MQTT.

### C2. DMX / Art-Net lighting sync
ESP32 emits DMX/Art-Net synced to **Link tempo+phase or live audio levels** —
the mixer/drummer drives the stage lights. Bridges the audio and lighting worlds
without a dedicated console.

### C3. Mixer health watchdog → phone push
Pings the mixer; on dropout, repeated clipping, or unexpected phantom/routing
change, fires a push / Slack / MQTT notification. For unattended installs
(houses of worship, AV, rental).

## D. Stage hardware the XR18 lacks (it has no control surface)

### D1. Personal IEM monitor box  ⭐ top pick
Each musician gets a knob + screen box controlling **their own aux/bus send** —
the #1 in-ear-monitoring request. Gives the surfaceless XR18 a per-player tactile
surface. (Encoder + OLED + `osc_in`/`osc_out`.)

### D2. Mic-live tally lights
Drive an LED/strip from `/ch/NN/mix/on` mute state — "this mic is live" tallies
for theater / broadcast / podcast. Trivial with `osc_in`.

### D3. RTA spectrum LED installation
Turn the 40-band RTA into an addressable-LED spectrum display or ambient room
lighting that breathes with the mix. Install / art piece.

## E. Sensor-driven audio (mic + meters)

### E1. SPL auto-leveling / ambient compensation  ⭐ product-grade
Mic senses room loudness; nudge a music bus/master to hold a target over crowd
noise. Restaurants, bars, gyms, retail. Real product category.

### E2. SPL compliance logger
Timestamped SPL logging to SD/cloud for venue noise-ordinance limits — a
dedicated, tamper-evident noise box. Venues pay for this.

### E3. Proactive ring-out assistant  ⏳ in progress
ToastSaver is reactive; add a **soundcheck mode** that sweeps monitor gain, finds
feedback frequencies *before* the show, and pre-notches them. Same FFT/notch DSP,
used preventively. **Being built now** — design in
[`ringout-design.md`](ringout-design.md), CLI tasks `T-021..T-025`.

### E3b. Automated system tune (pink noise)  🔭 vision
The bigger arc E3 leads to: play **pink noise** from the console's internal
generator, measure the RTA, and **EQ the mains (and monitors) flat to a target
curve** — then ring out the monitors. A true automated PA tune from one
measurement mic. Magnitude-only (no phase/time-alignment). Full design in
[`system-tune-design.md`](system-tune-design.md); scoped after `T-025`.

### E4. Smarter auto-mixer
A gain-sharing automixer for panels/conferences/worship using the mixer's own
meters — the upstream `X32Automix` idea, done with live metering and a better
algorithm.

---

## Top picks (best effort-to-payoff for an Ableton workflow)

| Idea | Why | Effort |
|------|-----|--------|
| **A1 — Bar-quantized scene recall** | Uniquely ours (needs Link phase), tiny, great demo | Low |
| **A2 — Reverse Link (band leads Live)** | Genuinely novel; parser already exists | Medium |
| **D1 — Personal IEM monitor box** | Biggest real-world demand; fits the surfaceless XR18 | Medium |
| **E1/E2 — SPL leveling / logging** | Clearest *products*; a venue would buy them | Medium |
| **B1 — X32Ableton bridge** | Fills a real ecosystem gap (no Ableton equivalent) | High (needs M4L/AbletonOSC half) |

### Suggested prefixes when these become tasks
`SNP-` (snapshot/scene + Link phase), `RLK-` (reverse-Link tempo source),
`IEM-` (personal monitor box), `SPL-` (SPL leveling/logging/compliance),
`ABL-` (Ableton/M4L bridge), `LGT-` (DMX/Art-Net), `HAS-` (Home Assistant/MQTT).
