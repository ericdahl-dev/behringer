# 2. The TUI is gated on isatty() with a plain-text fallback

Date: 2026-06-29
Status: accepted

## Context

T-031 replaces the raw-ANSI display with an ncurses TUI. But
`XAir_ToastSaver`'s stdout is a consumed interface: the T-025 ring-out safety
harness (`test_ringout_emu.py`) parses `[ring-out]` log lines, and operators
pipe the tool through `grep`. An ncurses TUI takes over the terminal and emits
no parseable stdout — it would break the harness and all piping.

## Decision

Detect the output target with `isatty(STDOUT_FILENO)`:

- **TTY** → the ncurses TUI (rich panels, live spectrum, key handling).
- **non-TTY** (pipe, redirect, CI) or `-v` → a **plain-text** renderer that
  prints the same `toast_step`/ring-out event stream as one line per event,
  with no `\033[` cursor positioning.

Both renderers consume the identical event stream. The auto pre-flight runs
before either renderer starts: pass → enter the renderer, fail → plain-text
error and exit (pre-flight is never an ncurses screen).

## Consequences

- The harness and `| grep` keep working unchanged (they are non-TTY).
- The plain renderer must be kept alive forever — deleting it to "simplify"
  re-breaks the harness. This ADR is the reminder.
- Two render paths to maintain, but they share one event stream, so the
  divergence is only formatting.
- Satisfies T-031's "no raw `\033[` escapes" — the fallback uses plain
  `printf`, the TUI uses ncurses; neither hand-positions the cursor.
