# behringer — X32 / XR18 control suite

Tools for automating and syncing Behringer **X32 / XR18** digital mixers over
OSC. The through-line is an automatic **feedback destroyer** ("ToastSaver" —
watch the RTA, notch the ringing band) built for several platforms, plus a
tempo-sync side firmware and an emulator to test against.

## Components

| Dir | What it is | Stack | Tasks | Guide |
|---|---|---|---|---|
| [`cli/`](cli/) | Command-line tools: **XAir_ToastSaver** (feedback destroyer — `/meters/4` RTA → TEQ notch), plus X32/XAir CLIs forked from upstream. Pure logic in `toast_logic`/`x32tap_logic`, host-tested. | C | root `T-` | — |
| [`esp32-link/`](esp32-link/) | On-board **tempo → FX-delay** bridge firmware: reads Ableton Link **or** USB-MIDI clock (runtime-selectable in a web UI) and writes delay time over OSC. | Arduino/ESP32-S3 | `LNK-` / `MCK-` | [AGENTS.md](esp32-link/AGENTS.md) |
| [`esp32/`](esp32/) | **X32 emulator** firmware — emulates a mixer so the other tools can be tested without hardware. | Arduino/ESP32-S3 | `ESP-` | — |
| [`ios/`](ios/) | **ToastSaver for iOS** — ToastSaverCore (Linux-testable detection logic) + FFT (Accelerate) + audio capture + XR18 OSC + SwiftUI. | Swift | `IOS-` | — |
| [`src/`](src/) | Upstream [pmaillot/X32-Behringer](https://github.com/pmaillot/X32-Behringer) as a **read-only** submodule. `cli/` was forked from here; don't edit. | C (submodule) | — | — |
| [`docs/`](docs/) | OSC protocol references: [GEQ](docs/xr18-geq-osc.md), [meters/RTA](docs/xr18-meters-osc.md). | — | — | — |

The feedback destroyer is the recurring product across `cli/` (shipping),
`ios/` (in progress), and a planned ESP32 build (`TSV-` tasks in
`esp32-link/tasks/`). `esp32-link`'s tempo bridge and the `esp32` emulator are
the supporting firmware.

## Working in here

Each component is self-contained — build/test it from its own directory:

- **cli:** `cd cli && make` → binaries in `cli/build/`; host tests
  `make test_toast_logic test_x32tap`.
- **esp32-link:** see [esp32-link/AGENTS.md](esp32-link/AGENTS.md) for the
  `arduino-cli` build/flash lines; host logic tests via `cd esp32-link/test && make`.
- **esp32 / ios:** see their `tasks/` for current scope.

Read a component's `AGENTS.md` before editing it, and don't cross-edit
(working in `esp32-link/` ≠ touching `cli/` or `esp32/`).

## Tasks (Ordna)

Work is tracked with [Ordna](AGENTS.md) — one tracker per component, markdown
files under each `tasks/` dir:

| Tracker | Prefix | Scope |
|---|---|---|
| `./tasks/` | `T-` | CLI tools / ToastSaver / cross-cutting |
| `esp32-link/tasks/` | `LNK-` `MCK-` `TSV-` | Link bridge · MIDI clock · planned ESP32 ToastSaver |
| `esp32/tasks/` | `ESP-` | X32 emulator |
| `ios/tasks/` | `IOS-` | iOS ToastSaver |

`ordna ls` (run inside a component) lists its issues. The root
[`AGENTS.md`](AGENTS.md) documents the Ordna format and CLI.
