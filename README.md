# behringer — X32 / XR18 control suite

Tools for automating and syncing Behringer **X32 / XR18** digital mixers over
OSC. The through-line is an automatic **feedback destroyer** ("ToastSaver" —
watch the RTA, notch the ringing band) built for several platforms, plus a
tempo-sync side firmware and an emulator to test against.

## Components

| Dir | What it is | Stack | Tasks | Guide |
|---|---|---|---|---|
| [`cli/`](cli/) | Command-line tools: **XAir_ToastSaver** (feedback destroyer — `/meters/4` RTA → TEQ notch), plus X32/XAir CLIs forked from upstream. Pure logic in `toast_logic`/`x32tap_logic`, host-tested. | C | root `T-` | — |
| [`esp32/`](esp32/) | On-board **tempo → FX-delay** bridge firmware (X32Link, X32MidiClock) + **X32 emulator** for testing without hardware. | Arduino/ESP32-S3 | `LNK-` / `MCK-` / `ESP-` | [AGENTS.md](esp32/AGENTS.md) |
| [`ios/`](ios/) | **ToastSaver for iOS** — ToastSaverCore (Linux-testable detection logic) + FFT (Accelerate) + audio capture + XR18 OSC + SwiftUI. | Swift | `IOS-` | — |
| [`src/`](src/) | Upstream [pmaillot/X32-Behringer](https://github.com/pmaillot/X32-Behringer) as a **read-only** submodule. `cli/` was forked from here; don't edit. | C (submodule) | — | — |
| [`docs/`](docs/) | OSC protocol references: [protocol index/provenance](docs/x32-osc-protocol.md), [XAir cheat-sheet](docs/xr18-xair-osc-cheatsheet.md), [GEQ](docs/xr18-geq-osc.md), [meters/RTA](docs/xr18-meters-osc.md). Plus the [ideas backlog](docs/ideas.md) — creative features beyond the upstream ports — and the [ring-out assistant design](docs/ringout-design.md). | — | — | — |

The feedback destroyer is the recurring product across `cli/` (shipping),
`ios/` (in progress), and a planned ESP32 build (`TSV-` tasks in
`esp32/tasks/`). The tempo bridge and emulator are the supporting firmware in
`esp32/`.

## Working in here

Each component is self-contained — build/test it from its own directory:

- **cli:** `cd cli && make` → binaries in `cli/build/`; host tests
  `make test_toast_logic test_x32tap`.
- **esp32:** see [esp32/AGENTS.md](esp32/AGENTS.md) for the `arduino-cli`
  build/flash lines; host logic tests via `cd esp32/test && make`.
- **ios:** see `ios/tasks/` for current scope.

Read a component's `AGENTS.md` before editing it, and don't cross-edit
(working in `esp32/` ≠ touching `cli/` or `ios/`).

## Tasks (Ordna)

Work is tracked with [Ordna](AGENTS.md) — one tracker per component, markdown
files under each `tasks/` dir:

| Tracker | Prefix | Scope |
|---|---|---|
| `./tasks/` | `T-` | CLI tools / ToastSaver / cross-cutting |
| `esp32/tasks/` | `LNK-` `MCK-` `TSV-` `ESP-` | Link bridge · MIDI clock · ESP32 ToastSaver · emulator |
| `ios/tasks/` | `IOS-` | iOS ToastSaver |

`ordna ls` (run inside a component) lists its issues. The root
[`AGENTS.md`](AGENTS.md) documents the Ordna format and CLI.
