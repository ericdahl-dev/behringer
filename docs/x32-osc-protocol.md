# X32 / M32 OSC Protocol — source & firmware node map

Index and provenance for the OSC reference material the ESP32 firmwares build
against. **The upstream documents are linked, not copied** — see License below.

## The canonical document

**"Unofficial X32/M32 OSC Protocol"** by **Patrick-Gilles Maillot** — the
reverse-engineered reference for the entire X32/M32 OSC node tree (updated
**2021-06-29**).

- Author page: <https://sites.google.com/site/patrickmaillot/x32>
- Companion site: <https://x32ram.com/>
- C source for all the tools it documents: <https://github.com/pmaillot/X32-Behringer>
  (vendored here read-only as the `src/` submodule).

**License / redistribution.** Maillot distributes his utilities as *freeware for
non-commercial use* ("commercial users should contact me"). The protocol PDF is
his work. We therefore **link** to it and keep only our own derived, attributed
extracts in this repo:

- [`xr18-xair-osc-cheatsheet.md`](xr18-xair-osc-cheatsheet.md) — XAir/XR18 control nodes + value encoding
- [`xr18-meters-osc.md`](xr18-meters-osc.md) — meter/RTA subscription (the ToastSaver input side)
- [`xr18-geq-osc.md`](xr18-geq-osc.md) — GEQ nodes

## X32 vs XAir — what changes

| Aspect          | X32 / M32                       | XR18 / XAir                          |
|-----------------|---------------------------------|--------------------------------------|
| OSC UDP port    | **10023**                       | **10024**                            |
| Input channels  | `/ch/01..32`                    | `/ch/01..16`                         |
| Buses           | `/bus/01..16`                   | `/bus/1..6`                          |
| Control surface | motor faders + LCD scribbles    | **none** (rackmount, app-controlled) |
| Change push     | `/xremote` keepalive            | `/xremote` keepalive (same)          |

Both share the same sub-node grammar (`/…/mix/fader`, `/…/mix/on`,
`/…/config/name`, `/headamp/NN/gain`, `/fx/N/par/MM`, …) and the same float/dB
value encoding. A firmware written against the XAir cheat-sheet ports to X32 by
switching the port and widening the channel ranges (`config_model_port()` in
`esp32/X32Link/app_config.c` already encodes the 10023/10024 split).

## Firmware → node map

Which nodes each ESP32 firmware reads/writes. Tasks live in `esp32/tasks/`.

| Firmware (tasks)        | Reads (subscribe via `/xremote`)      | Writes                                   | Model     |
|-------------------------|---------------------------------------|------------------------------------------|-----------|
| **X32FaderDisp** (`FDR-`) | `/ch/NN/mix/fader ,f`                | `/ch/NN/config/name ,s` (dB string)      | X32 only  |
| **X32SafeMutes** (`MUT-`) | `/ch/NN/mix/on ,i` (+ bus/dca)       | `/ch/NN/mix/on ,i` (restore locked)      | X32 + XR18|
| **MidiOscIttt** (`ITT-`)  | any node (rule triggers) + USB-MIDI  | any node / USB-MIDI (rule actions)       | X32 + XR18|
| X32Link / MidiClock     | — (send only)                         | `/fx/N/par/01 ,f` (delay time)           | X32 + XR18|
| ToastSaver (`TSV-`)     | `/meters/N` (RTA blob)                | `/fx/N/par/MM ,f` (TEQ notch)            | XR18      |

All three new firmwares require the shared **`osc_in`** receive/subscribe path
(task `LNK-013`) — X32Link today only *sends*. The on-device **X32 emulator**
(`esp32/X32_emulator/`) implements `/xremote` server-side and is the integration
target for testing them without hardware.
