# XR18 Meter Subscriptions via OSC

Real-time meter data for input monitoring, output level detection, and frequency spectrum analysis (RTA) — the input side needed for a feedback detector.

---

## Overview

The XR18 streams meter data as OSC blob packets at approximately **50ms intervals (20 Hz)** once subscribed. All meter values are **signed 16-bit integers** (little-endian), where:

```
dBFS = int16_value / 256.0
```

| int16 value | dBFS    | meaning                                      |
|-------------|---------|----------------------------------------------|
| -32768      | -128.00 | channel fully silent / disconnected          |
| 0           | 0.00    | channel active in signal path, no audio      |
| -19456      | -076.00 | normal signal at -76 dBFS                   |

---

## Subscription Protocol

### 1. Connect

Send `/xinfo` and wait for the reply (standard XR18 handshake).

### 2. Subscribe

```
/meters ,siii /meters/{N} 0 0 0
```

`{N}` = meter set (see table below). The three int args can specify a channel index for per-channel meters — send `0 0 0` for aggregate sets.

### 3. Keep alive (required)

Send `/xremote` every **9 seconds or less**. Without this the XR18 stops sending meter data.

### 4. Receive

Each incoming packet has OSC address `/meters/{N}` with a blob payload structured as:

```
[4 bytes big-endian]    total blob byte count
[4 bytes little-endian] number of int16 values (N)
[N × 2 bytes]           int16 values, little-endian, signed
```

---

## Meter Set Summary

| Set          | Values | Notes                                              |
|--------------|--------|----------------------------------------------------|
| /meters/0    | 8      | Output levels: LR + aux buses                      |
| /meters/1    | 40     | Channel + bus levels                               |
| /meters/2    | 36     | Mix/output aggregate                               |
| /meters/3    | 56     | Mixed — contains status flags (0.00) and dBFS      |
| **/meters/4**| **100**| **RTA spectrum — 100 frequency bins, 20Hz–~21.6kHz** |
| /meters/5    | 44     | Level meters at multiple signal path stages        |
| /meters/6    | 39     | Per-channel gate/comp; use channel index arg       |
| /meters/7    | 16     | 16-channel insert levels                           |
| /meters/8    | 4      | Small output aggregate                             |

---

## /meters/4 — RTA Spectrum (100 bins)

This is the primary meter for a feedback detector. It provides a 100-bin real-time frequency spectrum of the configured RTA source channel.

### Configure the RTA source

```
/-stat/rta/source ,i {channel}
```

Channel index corresponds to input channels 1–18. Query the current state:

```
/node ,s -stat/rta
```

Returns a string like `Ch01 PRE` (source channel and pre/post-EQ position).

Set RTA prefs (detection mode and decay):

```
/-prefs/rta ,if {det} {decay}
```

- `det`: `0` = PEAK, `1` = RMS
- `decay`: float (e.g. `0.25`)

### Frequency bin mapping

Bins are logarithmically spaced. The scale constant was empirically calibrated using a pistonphone at exactly 1000 Hz — peak landed at bin [56], giving a calibrated constant of **3.0339** (the RTA runs to ~21.6 kHz, not exactly 20 kHz):

```
f(n)  = 20 × 10^(n / 100 × 3.0339)          Hz for bin n
n(f)  = round(100 / 3.0339 × log10(f / 20)) bin for frequency f
```

Calibration data point: **pistonphone 1000 Hz @ 94 dB SPL → peak at bin [56], +63 dB above noise floor.**
Second harmonic at ~2kHz confirmed at bin [66] (-81 dBFS).

| Bin   | Frequency  |
|-------|------------|
| [00]  | 20 Hz      |
| [03]  | 25 Hz      |
| [07]  | 31 Hz      |
| [10]  | 40 Hz      |
| [13]  | 50 Hz      |
| [16]  | 63 Hz      |
| [20]  | 80 Hz      |
| [23]  | 100 Hz     |
| [26]  | 126 Hz     |
| [30]  | 160 Hz     |
| [33]  | 200 Hz     |
| [36]  | 252 Hz     |
| [39]  | 315 Hz     |
| [43]  | 400 Hz     |
| [46]  | 500 Hz     |
| [49]  | 630 Hz     |
| [53]  | 800 Hz     |
| **[56]** | **1000 Hz** |
| [59]  | 1250 Hz    |
| [63]  | 1600 Hz    |
| [66]  | 2000 Hz    |
| [70]  | 2518 Hz    |
| [73]  | 3098 Hz    |
| [77]  | 4084 Hz    |
| [80]  | 5024 Hz    |
| [83]  | 6181 Hz    |
| [87]  | 8148 Hz    |
| [90]  | 10024 Hz   |
| [93]  | 12332 Hz   |
| [97]  | 16257 Hz   |
| [99]  | 18665 Hz   |

### Noise floor behavior

When the RTA source is set to `PRE` (pre-EQ, pre-mute), the preamp's own electronic noise is visible even when the channel is muted. Typical preamp noise floor is **-80 to -97 dBFS** in the bins. Audio signals appear above this floor. Feedback signals appear as sudden large spikes — typically **30–50 dB above the noise floor** — in one or a few adjacent bins.

---

## GEQ Band → RTA Bin Mapping

For feedback destruction: detect the peak bin in /meters/4, map it to the nearest GEQ `par` number, apply a notch.

Calibrated using a pistonphone (1000 Hz, 94 dB SPL) — bin [56] confirmed as 1 kHz.

| GEQ par | Center freq | RTA bin | OSC path (FX slot 4)          |
|---------|-------------|---------|-------------------------------|
| 01      | 20 Hz       | [00]    | `/fx/4/par/01 ,f {val}`       |
| 02      | 25 Hz       | [03]    | `/fx/4/par/02 ,f {val}`       |
| 03      | 31.5 Hz     | [07]    | `/fx/4/par/03 ,f {val}`       |
| 04      | 40 Hz       | [10]    | `/fx/4/par/04 ,f {val}`       |
| 05      | 50 Hz       | [13]    | `/fx/4/par/05 ,f {val}`       |
| 06      | 63 Hz       | [16]    | `/fx/4/par/06 ,f {val}`       |
| 07      | 80 Hz       | [20]    | `/fx/4/par/07 ,f {val}`       |
| 08      | 100 Hz      | [23]    | `/fx/4/par/08 ,f {val}`       |
| 09      | 125 Hz      | [26]    | `/fx/4/par/09 ,f {val}`       |
| 10      | 160 Hz      | [30]    | `/fx/4/par/10 ,f {val}`       |
| 11      | 200 Hz      | [33]    | `/fx/4/par/11 ,f {val}`       |
| 12      | 250 Hz      | [36]    | `/fx/4/par/12 ,f {val}`       |
| 13      | 315 Hz      | [39]    | `/fx/4/par/13 ,f {val}`       |
| 14      | 400 Hz      | [43]    | `/fx/4/par/14 ,f {val}`       |
| 15      | 500 Hz      | [46]    | `/fx/4/par/15 ,f {val}`       |
| 16      | 630 Hz      | [49]    | `/fx/4/par/16 ,f {val}`       |
| 17      | 800 Hz      | [53]    | `/fx/4/par/17 ,f {val}`       |
| **18**  | **1 kHz**   | **[56]**| `/fx/4/par/18 ,f {val}`  ✓ calibrated |
| 19      | 1.25 kHz    | [59]    | `/fx/4/par/19 ,f {val}`       |
| 20      | 1.6 kHz     | [63]    | `/fx/4/par/20 ,f {val}`       |
| 21      | 2 kHz       | [66]    | `/fx/4/par/21 ,f {val}`       |
| 22      | 2.5 kHz     | [69]    | `/fx/4/par/22 ,f {val}`       |
| 23      | 3.15 kHz    | [72]    | `/fx/4/par/23 ,f {val}`       |
| 24      | 4 kHz       | [76]    | `/fx/4/par/24 ,f {val}`       |
| 25      | 5 kHz       | [79]    | `/fx/4/par/25 ,f {val}`       |
| 26      | 6.3 kHz     | [82]    | `/fx/4/par/26 ,f {val}`       |
| 27      | 8 kHz       | [86]    | `/fx/4/par/27 ,f {val}`       |
| 28      | 10 kHz      | [89]    | `/fx/4/par/28 ,f {val}`       |
| 29      | 12.5 kHz    | [92]    | `/fx/4/par/29 ,f {val}`       |
| 30      | 16 kHz      | [96]    | `/fx/4/par/30 ,f {val}`       |
| 31      | 20 kHz      | [99]    | `/fx/4/par/31 ,f {val}`       |

**GEQ notch value encoding** (see `xr18-geq-osc.md`):
```
float_val = (dB + 15) / 30       # 0.5 = flat, 0.3 = -6 dB, 0.2 = -9 dB
```

---

## /meters/0 — Output Levels (8 values)

Useful for detecting overall output level spikes (feedback onset) without needing spectral analysis.

Observed layout (no audio):
```
[0-1]  -128 dBFS   (silent)
[2-3]   0.00       (LR L and R — active bus, no signal = int16 zero)
[4-7]  -128 dBFS   (aux buses, silent)
```

---

## /meters/1 — Channel + Bus Levels (40 values)

Observed layout with channel 1 active:
```
[00]   channel 1 input level
[01]   channel 1 (secondary — post-EQ or R side)
[02-15] channels 2–16 (silent)
[16-17] LR L + R
[18-23] aux buses 1–6
[24-25] unknown
[36-39] FX returns (duplicated pairs)
```

---

## /meters/5 — Multi-Stage Level Meters (44 values)

NOT spectral — confirmed by testing. Shows the same channel's signal at multiple stages of the signal path simultaneously. Positions [26-41] do not respond to audio content changes.

Active positions when channel 1 has signal:
```
[00] [06] [42]  ~same level  (channel 1 at multiple routing stages)
[08] [24]       ~same level  (LR bus or summing stage)
[01] [07] [43]  secondary (lower level)
[09] [25]       secondary
```

---

## RTA Configuration Paths

| Path                  | Type | Description                          |
|-----------------------|------|--------------------------------------|
| `/-stat/rta`          | node | `Ch01 PRE` — source and position      |
| `/-stat/rta/source`   | ,i   | Channel index (1–18)                 |
| `/-prefs/rta`         | ,if  | [det_mode][decay] — detection + decay |
| `/-prefs/rta/det`     | ,i   | 0=PEAK, 1=RMS                        |
| `/-prefs/rta/decay`   | ,f   | Decay rate                           |

---

## Feedback Detector Workflow

```
1. Connect:     send /xinfo, wait for reply
2. Configure:   /-stat/rta/source ,i {output_channel}
3. Subscribe:   /meters ,siii /meters/4 0 0 0
4. Keep-alive:  /xremote every 9s (timer)
5. Receive:     100 int16 values at 20 Hz
6. Baseline:    average first N frames to establish noise floor per bin
7. Detect:      when any bin exceeds (noise_floor + threshold), feedback detected
8. Identify:    peak bin index → GEQ par number (from table above)
9. Notch:       /fx/{slot}/par/{nn} ,f {cut_value}   (e.g. 0.3 = -6 dB)
               or /lr/geq/{freq} ,f {cut_value}
10. Monitor:    watch for level drop confirming the notch worked
11. Release:    slowly restore the notch over time if level stays low
```

---

## Value Encoding — Code Reference

```python
# Parse a /meters/4 blob
data = osc_blob_payload          # raw bytes after OSC address and ",b" type tag
blob_size   = int.from_bytes(data[0:4], 'big')
bin_count   = int.from_bytes(data[4:8], 'little')   # = 100
bins = []
for i in range(bin_count):
    raw = int.from_bytes(data[8 + i*2 : 10 + i*2], 'little', signed=True)
    bins.append(raw / 256.0)    # dBFS value

# Find peak bin
peak_bin = max(range(100), key=lambda i: bins[i])

# Map bin to GEQ par number
# Scale constant 3.0339 calibrated with pistonphone 1kHz @ 94dB SPL (bin [56] = 1kHz)
import math
SCALE = 3.0339
def bin_to_geq_par(bin_idx):
    geq_freqs = [20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160,
                 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600,
                 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000]
    def bin_for(f):
        return round(100 / SCALE * math.log10(max(f, 20) / 20))
    return min(range(31), key=lambda i: abs(bin_for(geq_freqs[i]) - bin_idx)) + 1

par = bin_to_geq_par(peak_bin)  # 1–31
```

```c
/* C — parse /meters/4 blob */
int16_t raw;
float bins[100];
uint8_t *p = blob_data + 8;      /* skip 4-byte big-endian size + 4-byte LE count */
for (int i = 0; i < 100; i++) {
    memcpy(&raw, p + i*2, 2);    /* little-endian already on x86/ARM */
    bins[i] = (float)raw / 256.0f;
}
```

---

## Quick Reference

```bash
# Set RTA source to channel 1 and subscribe
(
  echo 'xremote on'
  sleep 0.2
  echo '/-stat/rta/source ,i 1'
  sleep 0.2
  echo '/meters ,siii /meters/4 0 0 0'
  sleep 10
  echo 'exit'
) | ./build/XAir_Command -i 192.168.1.146
```
