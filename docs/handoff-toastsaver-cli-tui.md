# Handoff — ToastSaver CLI → TUI batch

**Goal:** finish the ToastSaver CLI, which is becoming a TUI. Full batch, agreed
with the user 2026-06-29. Read `CONTEXT.md` (glossary) and
`docs/adr/0001`, `docs/adr/0002` first — they hold the load-bearing decisions.

## Build order (vertical slices, one PR each, TDD where pure logic exists)

1. **T-029 — `toast_step()`** ← start here
2. **T-028 — `osc_io`**
3. **T-031 — ncurses TUI**
4. **T-025 tail** — profile-handoff harness scenario + doc default-tuning
5. **T-009 / T-010 / T-011** — re-scoped (see below)

## Decisions of record

### T-029 — toast_step (ADR-0001: event stream)
- The pure primitives already live in `cli/toast_logic.c` (detect_peak,
  is_narrow_peak, update_baseline, parse_meters4_blob, db_to_geq_float,
  bin_to_geq_par). What's missing is the **orchestration** — currently inline in
  `run_detection()` in `cli/XAir_ToastSaver.c` (~line 500-590), untested.
- Signature:
  `int toast_step(ToastState*, const float *bins, const float *baseline, const ToastConfig*, ToastAction *out, int max)` → event count.
- `ToastAction = { ToastOp op; int par; float val; int bin; float level; }`,
  `op ∈ {TOAST_NOTCH, TOAST_RELEASE_RAMP, TOAST_RELEASE_DONE}`.
- `ToastState` owns `notches[31]` + `confirm_hold[31]` (remove from `AppState`).
  Frame counters only — **time-free** (no `now_sec()`). Caller stamps wall time,
  rolls the baseline (`update_baseline`, alpha 0.005, already done at line 624),
  sends OSC for every event `val`, logs only NOTCH/RELEASE_DONE.
- `ToastConfig` = pure params (threshold_db, cut_db, confirm_frames,
  release_frames, release_thr ratio 0.5, narrow_skip/span/min_db).
- Pin profile notches via a `from_profile` flag in `ToastState` (never released).
- Tests: extend `cli/test_toast_logic.c` with the 7 cases in `tasks/T-029.md`.

### T-028 — osc_io
- New `cli/osc_io.{h,c}`: `OscConn { int fd; struct sockaddr *xip_addr; socklen_t xip_len; }`
  + `osc_conn_open / osc_handshake / osc_send / osc_query_float / osc_query_int /
  osc_recv`, and pure `osc_locate_blob()` / `osc_build()`.
- Kills the `((strlen+1+3)&~3)` align dup (7×) and the `/xinfo` handshake (2×) in
  `XAir_ToastSaver.c`. Tests: `cli/test_osc_io.c` (locate_blob too-short → NULL;
  correct offset on a hand-built `/meters/4`; handshake over loopback if cheap).
- **Folds T-009 (UDP session) + T-010 (OSC intent helpers)** — close them as
  superseded when this lands.

### T-031 — TUI (ADR-0002: isatty fallback)
- ncurses, **gated on `isatty(STDOUT_FILENO)`**. TTY → rich TUI; non-TTY / `-v`
  → plain-text event log (one line per event, no `\033[`). **The plain path is
  mandatory** — `cli/test_ringout_emu.py` and `| grep` parse stdout; an
  unconditional TUI breaks them. Both paths consume the same event stream.
- `cli/tui.{h,c}` + `cli/log_buf.{h,c}` (200-entry ring, shared by reactive +
  ring-out, fed by the event stream).
- Doctor → TUI panel with `f` = fix, for explicit `--doctor`. The **auto
  pre-flight stays a pre-TUI gate**: pass → enter renderer, fail → text error +
  exit. Never an ncurses screen.
- `-v` = plain + raw RTA numbers (not "disable display"). SIGWINCH redraw. Clean
  teardown: fader restored, notches handled, cursor restored, terminal usable.
- Link `-lncurses` on the `XAir_ToastSaver` line only (test binaries don't need it).
- The current `print_static_display`/`draw_display` raw-ANSI go away; their
  event-logging role survives as the plain fallback.

### T-025 tail
- Add harness scenario 3: `--ringout --save-profile X`, then reactive
  `--load-profile X`, assert the pinned notch is pre-placed (initial TEQ par set).
- Write the validated ring-out defaults into `docs/ringout-design.md`.

### T-009 / T-010 / T-011
- T-009 (UDP session) + T-010 (OSC intent helpers) → **folded into T-028**.
- T-011 (build-target catalog) → keep separate, low priority.

## State as of handoff
- **Done & merged:** T-030 (doctor + auto pre-flight, verified live on the XR18),
  FaderDisp FDR-001/002, LNK-013 osc_in.
- **T-025:** safety harness `cli/test_ringout_emu.py` passes 6/6
  (`make test-ringout-emu`); only the profile-handoff scenario + doc-tuning remain.
- `make check` = 127 host tests green; all firmwares + CLI build clean.

## Gotchas
- XR18 DHCP-roams; it was `.1.146`, now `.2.146`. Re-discover before live tests.
- X32 `/node` reply address is **`node`** (no leading slash) — see `osc_query_node`.
- Mixer must have GEQ/TEQ on the target output or notches no-op — that's what
  doctor/pre-flight guards.
- Branch protection: PR workflow only (no direct push to master).
