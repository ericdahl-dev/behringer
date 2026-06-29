# Ring-out assistant — design note

Design for the **proactive ring-out** capability added to ToastSaver. Worked out
first in the **CLI** (`cli/XAir_ToastSaver`, pure logic host-tested), then the
proven core ports to iOS (`ToastSaverCore`) and ESP32 (`TSV-`). Tasks: root
`tasks/` `T-021`..`T-025`.

## One pipeline, two modes

Proactive and reactive are **not two apps** — they are two ends of one pipeline
with a handoff:

- **Proactive (ring-out)** — the app *drives a monitor's gain up* in a controlled
  ramp, finds each feedback frequency as it starts to ring, notches it, and
  continues until a gain ceiling / target headroom / max-notch limit. Output: a
  **static notch profile + the measured gain-before-feedback margin**.
- **Reactive (protect)** — starts *from that profile* (static notches pre-placed)
  and dynamically defends the remaining margin during the show. This is the
  existing ToastSaver behaviour, plus a new "load a profile at startup" step.

> The proactive pass *produces* what the reactive pass *consumes*. "React as
> planned" = the planned notches are already in when the show starts; live mode
> only handles the delta (mic moved, temperature, a body in front of a wedge).

| | Reactive (have) | Proactive ring-out (new) |
|---|---|---|
| Who drives gain | the engineer | **the app** ramps the monitor bus |
| Detector | `detect_peak` + `is_narrow_peak` | **same** primitives |
| EQ action | place/release notches live | place **static** notches, build a profile |
| Termination | runs forever | gain ceiling **or** target margin **or** max notches |
| Output | maintains | notch profile + headroom margin → feeds reactive |

## The new piece: a pure `ringout_step()` controller

Everything dangerous (driving a system toward feedback) lives in **one pure,
host-testable step function** — same discipline as the rest of `toast_logic`:

```c
typedef enum { RO_RAISE_GAIN, RO_PLACE_NOTCH, RO_BACK_OFF,
               RO_HOLD, RO_DONE, RO_ABORT } RingoutOp;

typedef struct { RingoutOp op; float gain_db; int geq_par; float cut_db; } RingoutAction;

/* Pure: given the latest RTA frame + baseline + current state, decide the next
 * action. No sockets, no time calls — caller supplies frame index / elapsed. */
RingoutAction ringout_step(RingoutState *st, const float *bins,
                           const float *baseline);
```

The platform layer executes the action (ramp the bus fader, send a notch via the
existing `send_teq_band`) and feeds back the next `/meters/4` frame. Unit tests
inject synthetic RTA frames with a rising narrowband peak and assert the action
sequence — no hardware, no risk (mirrors `test_toast_logic.c`).

## Safety (non-negotiable — proactive deliberately approaches feedback)

1. **Hard gain ceiling** — `ringout_step` never emits `RO_RAISE_GAIN` past the
   configured max; reaching it ends the run (`RO_DONE`).
2. **Instant cut on confirmed ring** — notch + `RO_BACK_OFF` the moment a ring is
   confirmed; never let it sustain.
3. **Panic/abort** — `RO_ABORT` (and operator Ctrl-C) snaps the driven gain back
   to its start value immediately.
4. **Ramp rate** slow enough to catch onset before it's painful; **one ring at a
   time** (notch the loudest, back off, continue).
5. **Supervised** — operator initiates and can stop/approve; a human stays in the
   loop. (CLI: prompt/keypress between steps in `--ringout-step` mode.)

## Targeting — ring-out is per-monitor

Each wedge/IEM has its own mic→speaker loop and rings differently, so a run
targets **one output at a time**:

- **Gain drive:** ramp the monitor bus fader — `/bus/N/mix/fader ,f` (XAir;
  see [`xr18-xair-osc-cheatsheet.md`](xr18-xair-osc-cheatsheet.md)).
- **Notch:** reuse the existing FX-slot TEQ — `/fx/{slot}/par/{nn} ,f`
  (flat = 0.5). The TEQ must be **inserted on the monitor bus under test**
  (`/bus/N/insert/...`), so the notches act on that wedge.
- **RTA source:** `/meters/4` must be tapping that monitor's mic. v1 can require
  the operator to set the RTA tap; later versions select it automatically.

## Reuse map

| Need | Reuse |
|---|---|
| RTA parse | `parse_meters4_blob` |
| Noise floor | `update_baseline` |
| Find a ring | `detect_peak` + `is_narrow_peak` |
| Bin → band, dB → float | `bin_to_geq_par`, `db_to_geq_float` |
| Apply / release notch | `send_teq_band` (existing) |
| **New** | `ringout_logic.{c,h}` (controller) + gain-drive + profile I/O |

## Portability

`ringout_logic.c` is pure C with no I/O — the same contract ports to
`ToastSaverCore` (Swift) and the ESP32 `TSV-` firmware. Only the action-executor
(OSC send + RTA receive) is platform-specific, exactly like today's split.
