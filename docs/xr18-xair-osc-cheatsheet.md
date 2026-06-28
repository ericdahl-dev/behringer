# XR18 / XAir OSC Cheat-Sheet (control nodes)

The OSC node families used by the ESP32 control firmwares (FaderDisp, SafeMutes,
MidiOscIttt), distilled from the **XAir OSC cheat-sheet** for use on the XR18.

> **Source / attribution.** Node tree from *"XR18/16/12 1.14 (2016-12-16)"*,
> originally by **Patrick-Gilles Maillot**, amended by **Dave Meadowcroft
> (2017-02-02)**, published on Maillot's X32 page
> (<https://sites.google.com/site/patrickmaillot/x32>, "XAir data … Cheat-sheet").
> This file reproduces only the node families our firmware needs, plus the
> value-encoding formulas. The full cheat-sheet (all FX params, routing, etc.)
> and the broader **X32/M32 OSC protocol document** are linked from
> [`x32-osc-protocol.md`](x32-osc-protocol.md).

---

## Transport

- **OSC port: `10024`** (XR18/XAir). *X32 uses `10023`.*
- Handshake: send `/xinfo`, wait for reply.
- **Subscribe to changes:** send `/xremote` — the mixer then pushes every
  parameter change to your client as an OSC message. **Re-send `/xremote` every
  ≤9 s** or the mixer stops sending (same keepalive as `/meters`, see
  [`xr18-meters-osc.md`](xr18-meters-osc.md)).
- Strings are NUL-terminated, padded to 4-byte boundaries (standard OSC).

---

## Channel addressing

All strip types share the same sub-node layout. `NN` is **two digits**.

| Prefix          | Range        | Notes                                   |
|-----------------|--------------|-----------------------------------------|
| `/ch/NN/`       | `01`..`16`   | input channels                          |
| `/rtn/aux/`     | —            | aux return (≈ `ch[16]`)                 |
| `/rtn/N/`       | `1`..`4`     | FX returns (≈ `ch[17..20]`)             |
| `/bus/N/`       | `1`..`6`     | bus masters (≈ `ch[21..26]`)            |
| `/fxsend/N/`    | `1`..`4`     | FX send masters (≈ `ch[27..30]`)        |
| `/lr/`          | —            | main L/R (≈ `ch[31]`)                    |
| `/dca/N/`       | `1`..`4`     | DCA groups                              |

> When a node takes a channel **index argument**, the arrays are **zero-based**
> (channel 1 → `i = 0`).

---

## Nodes used by the firmwares

### Mix (fader / mute)

```
/ch/NN/mix/on      ,i  [0,1]            // OFF, ON  — channel on/off (ON=1 = UNMUTED)
/ch/NN/mix/fader   ,f  [0.0,1.0]        // fader(1024) → -oo..+10 dB
/ch/NN/mix/pan     ,f  [0.0,1.0]        // -100..100
/ch/NN/mix/lr      ,i  [0,1]            // route to LR
```

> **Mute semantics are inverted:** `mix/on = 0` is **muted**, `mix/on = 1` is
> **unmuted**. SafeMutes locks on this node.

### Scribble / labels (FaderDisp — X32 only, see note)

```
/ch/NN/config/name   ,s  (16)           // channel name, max 16 chars
/ch/NN/config/color  ,i  [0,15]
/dca/N/config/name   ,s  (16)
```

> The XR18 has **no physical scribble strips or faders** — it is a rackmount
> controlled from an app. FaderDisp (writing live dB onto the scribble) only has
> a visible effect on an **X32/M32** console (port `10023`). The nodes above are
> identical on X32; the firmware just targets a different port/model.

### Headamp gain (VoiceTrax-style control)

```
/headamp/NN/gain     ,f  [0.0,1.0]      // lin(145) → -12..+60 dB  (NN = 01..16)
/headamp/NN/phantom  ,i  [0,1]
```

### FX (delay/tempo bridges, reference)

```
/fx/N/type           ,i  [0,60]         // algorithm
/fx/N/par/MM         ,f                  // MM = 01..64 (per-algorithm parameter)
```

### Snapshots (scene recall, reference)

```
/-snap/load          ,i  [1,64]         // triggers snapshot load
/-snap/save          ,i  [1,64]
/-snap/NN/name       ,s  (31)
```

---

## Value encoding

### Linear / log parameters

```
linear:  value = fmin + (fmax - fmin) * f          f = (value - fmin) / (fmax - fmin)
log:     value = fmin * exp(log(fmax/fmin) * f)     f = log(value/fmin) / log(fmax/fmin)
```

### Fader / level: float `f` ↔ dB `d`

This is the conversion **FaderDisp** needs (OSC float → displayed dB):

```
// float (f, 0..1) → dB (d)
d = +10            if f >= 1
d = (40 * f) - 30  if f >= 0.5
d = (80 * f) - 50  if f >= 0.25
d = (160 * f) - 70 if f >= 0.0625
d = (480 * f) - 90 if f >= 0
d = -90 (-oo)      if f <  0

// dB (d) → float (f)   (inverse — for sending a target dB)
f = 1              if d >= +10
f = (d + 30) / 40  if d >= -10
f = (d + 50) / 80  if d >= -30
f = (d + 70) / 160 if d >= -60
f = (d + 90) / 480 if d >= -90
f = 0              if d <  -90
```
