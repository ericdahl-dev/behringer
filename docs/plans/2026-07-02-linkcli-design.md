# linkcli — Headless Ableton Link Peer CLI — Design

**Date:** 2026-07-02
**Status:** Design (validated in brainstorming; not yet planned into tasks)
**Target:** Host dev machine (macOS/Linux), not ESP32. Supports `esp32/X32Link` development.

## Problem

`X32Link`'s Link discovery (`link_protocol.c`) and clock-measurement (`link_measurement.c`
+ `link_measurement_io.cpp`) code can currently only be exercised end-to-end by running
real Ableton Live on the same LAN. That makes iteration slow and blocks an agent from
verifying Link-related changes to the firmware without a human at a DAW. We want a small
standalone CLI that acts as a real Link peer — broadcast a tempo, flip play/stop, change
BPM live — so a device (or an agent driving one over Bash) can be checked against it
directly.

## Decisions (from brainstorming)

1. **Wrap the vendored SDK, don't reimplement the wire protocol.** `esp32/lib/link` is
   a git submodule pinned to `github.com/Ableton/link` — the real, spec-correct
   implementation, including the `PingResponder` side that `X32Link` depends on (the
   firmware is pinger-only and never replies to pings itself, per
   `link_measurement.h:6-8`). Reimplementing `_asdp_v`/`_link_v` framing by hand would
   mean owning protocol-correctness ourselves for no benefit — the real SDK is already
   in the tree.
2. **Headless — no audio backend.** The SDK ships `AudioPlatform_Dummy.hpp`
   (`esp32/lib/link/examples/linkaudio/`), a pure timer/session-state engine with the
   same interface `LinkHut` uses, but no PortAudio/CoreAudio/ASIO dependency. `linkcli`
   uses this directly instead of `AudioPlatform.hpp`'s platform-specific backends — no
   system audio libraries, no `-framework AudioUnit`, nothing to install.
3. **Both flags and live stdin control.** Common cases (fixed BPM, run N seconds) are
   one Bash call with flags; anything needing mid-run changes (retune, start/stop) keeps
   the process alive and reads line-oriented commands from stdin. Status is periodic
   JSON lines on stdout either way, so an agent can pipe/parse without scraping a TTY
   table like `LinkHut`'s.
4. **New sibling directory `tools/linkcli/`, own `CMakeLists.txt`.** Not inside
   `esp32/lib/link/examples/` — that's vendored third-party code (a git submodule); a
   new example there would be a local modification to someone else's repo, awkward to
   carry across submodule updates. `esp32/AGENTS.md` also explicitly scopes itself away
   from top-level tooling ("Not the CLI tools... don't edit `cli/` when working here"),
   confirming a sibling `tools/` dir is the right seam, not `esp32/`.
5. **v1 ships just the binary.** No automated Python test harness in this pass — that's
   natural follow-up work once the binary is proven, analogous to `cli/test_ringout_emu.py`
   (spawn binary, drive it over a socket loop, PASS/FAIL tally) but for HTTP polling of
   `X32Link`'s `/status` instead of OSC.

## Why this satisfies X32Link's protocol expectations

Confirmed by reading `link_protocol.c`, `link_measurement.c`/`.h`, and
`link_measurement_io.cpp` directly:

- Discovery group/port (`link_listener.cpp:5-6`): standard Link multicast
  `224.76.78.75:20808`. `link_protocol.c` parses the standard Timeline TLV (`tmln`,
  key `0x746d6c6e`) and measurement-endpoint TLV (`mep4`, key `0x6d657034`) — both are
  real Link wire keys, so the SDK emits them with zero adaptation. `tempo_source_bpm()`
  reads straight off the parsed Timeline, so a peer's broadcast tempo shows up on the
  device immediately (gossip-driven, not measurement-gated).
- Measurement trigger (`link_measurement_io.cpp:84-106`): fires when a peer's `mep4`
  endpoint is first seen (not periodic). The device re-pings every 50ms
  (`WATCHDOG_US`), gives up after 5 consecutive timeouts (250ms total silence), and
  needs ≥8 accumulated pong samples (`LINK_MEASUREMENT_READY_SAMPLES`) before it
  commits a `GhostXForm` and `tempo_source_phase_valid()` goes true. A real SDK peer on
  the same LAN answers in well under 1ms, so this converges trivially — the only thing
  `linkcli` must not skip is publishing `mep4` (automatic; the SDK enables measurement
  endpoints by default, just don't strip that).
- Verification surface: the device serves `GET /status` →
  `{"bpm":F,"phase":F,"valid":bool,"quantum":N}` (`web_status_json.c`, 1Hz-polled by
  the web UI). Polling this while `linkcli` runs is the end-to-end check: `bpm` should
  track what we broadcast, `valid` should flip true once measurement completes.

No protocol deviations found on X32Link's side worth designing around — it's a
faithful (if partial) Link implementation.

## Interface

Binary: `tools/linkcli/build/linkcli`.

**Flags** (hand-parsed `argv`, no third-party arg-parsing dependency — consistent with
`cli/`'s stdlib-only convention):

| Flag | Default | Meaning |
|---|---|---|
| `--bpm F` | 120 | Initial tempo |
| `--quantum F` | 4 | Initial quantum (bar length in beats) |
| `--start` | off | Begin in the playing state immediately |
| `--duration N` | 0 (run until stdin EOF/`quit`/SIGINT) | Exit automatically after N seconds |
| `--json` | off | Emit periodic JSON status lines to stdout (else a human-readable one-line table, non-interactive since output may be piped) |
| `--status-interval-ms N` | 200 | Cadence of status output |

**Stdin commands** (one per line, read continuously unless stdin is closed):

```
tempo <bpm>       set tempo
quantum <n>       set quantum
start / stop      toggle transport (isPlaying)
enable / disable  toggle the Link session itself (peer join/leave — exercises the
                  device's 15s peer-TTL expiry, PEER_TTL_MS in link_protocol.c)
status            force an immediate status line
quit              clean shutdown
```

**JSON status schema** (mirrors the device's `/status` field names where they overlap,
so the two can be eyeballed side by side):

```json
{"tempo":128.0,"quantum":4.0,"numPeers":1,"isPlaying":true,"beat":12.34,"enabled":true}
```

## Build

`tools/linkcli/CMakeLists.txt` includes `esp32/lib/link/AbletonLinkConfig.cmake` and
links `Ableton::Link`; `main.cpp` includes `Link.hpp` directly plus
`AudioPlatform_Dummy.hpp` from the SDK's `linkaudio/` example dir for the session-state
engine (`startPlaying`/`stopPlaying`/`setTempo`/`setQuantum`, same shape `LinkHut`
uses). No CoreAudio/PortAudio/ASIO link libraries, no ASIO-SDK download step (that's
only pulled by the Windows example path). Requires the `asio-standalone` submodule for
header-only networking, already vendored (`modules/asio-standalone`) — needs
`git submodule update --init --recursive` if not already checked out.

```
cmake -S tools/linkcli -B tools/linkcli/build
cmake --build tools/linkcli/build
```

## Verification

1. **Standalone smoke test** (no device needed): run
   `./tools/linkcli/build/linkcli --bpm 120 --start --json --duration 5` and confirm
   JSON status lines print with `numPeers:0` (no other Link peers on the network) —
   validates the binary builds and runs the session-state loop correctly in isolation.
2. **End-to-end against the device**: flash/run `X32Link` on the same LAN. Start
   `linkcli --bpm 128 --start --json` (left running). Poll
   `curl http://<device-ip>/status` repeatedly:
   - `bpm` should read ~128 almost immediately (gossip-driven).
   - `valid` should flip to `true` within roughly the 250ms measurement window once
     the device discovers the peer's `mep4` endpoint.
   Send `tempo 100` on `linkcli`'s stdin and confirm the device's polled `bpm` updates.
   Send `quit` and confirm the device's peer count eventually drops (~15s TTL,
   `PEER_TTL_MS`).
3. This becomes the repeatable manual check for any future `link_protocol.c` /
   `link_measurement*` change — no Ableton Live install required.
