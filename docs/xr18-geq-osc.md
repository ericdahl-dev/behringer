# XR18 GEQ / TEQ OSC Control Reference

This document covers all OSC paths for controlling graphic EQ bands on the XR18, across both the FX rack and the built-in EQ slots.

---

## Where GEQ/TEQ Lives

| Location          | GEQ/TEQ available? | OSC system       |
|-------------------|--------------------|------------------|
| Channel 1–16      | No (PEQ only)      | —                |
| Aux Bus 1–6       | Yes (mode switch)  | `/bus/{N}/geq/`  |
| Main LR           | Yes (mode switch)  | `/lr/geq/`       |
| FX Slot 1–4       | Yes (effect type)  | `/fx/{N}/par/`   |

---

## Value Encoding (all systems)

All GEQ band values are floats in `[0.0, 1.0]` mapped linearly over `[-15 dB, +15 dB]`:

```
float_val = (dB + 15) / 30
dB        = (float_val * 30) - 15
```

| dB     | float |
|--------|-------|
| −15 dB | 0.000 |
| −12 dB | 0.100 |
| −9 dB  | 0.200 |
| −6 dB  | 0.300 |
| −3 dB  | 0.400 |
|   0 dB | 0.500 |
|  +3 dB | 0.600 |
|  +6 dB | 0.700 |
|  +9 dB | 0.800 |
| +12 dB | 0.900 |
| +15 dB | 1.000 |

---

## Built-in GEQ — Main LR

The LR master has a built-in EQ that can be switched between PEQ, GEQ, and TEQ modes.

### Set EQ mode

```
/lr/eq/mode ,i {mode}
```

| value | mode |
|-------|------|
| 0     | PEQ  |
| 1     | GEQ  |
| 2     | TEQ  |

### Control bands

```
/lr/geq/{freq} ,f {float_val}
```

Bands are addressed by frequency name:

| OSC path         | Frequency |
|------------------|-----------|
| `/lr/geq/20`     | 20 Hz     |
| `/lr/geq/25`     | 25 Hz     |
| `/lr/geq/31.5`   | 31.5 Hz   |
| `/lr/geq/40`     | 40 Hz     |
| `/lr/geq/50`     | 50 Hz     |
| `/lr/geq/63`     | 63 Hz     |
| `/lr/geq/80`     | 80 Hz     |
| `/lr/geq/100`    | 100 Hz    |
| `/lr/geq/125`    | 125 Hz    |
| `/lr/geq/160`    | 160 Hz    |
| `/lr/geq/200`    | 200 Hz    |
| `/lr/geq/250`    | 250 Hz    |
| `/lr/geq/315`    | 315 Hz    |
| `/lr/geq/400`    | 400 Hz    |
| `/lr/geq/500`    | 500 Hz    |
| `/lr/geq/630`    | 630 Hz    |
| `/lr/geq/800`    | 800 Hz    |
| `/lr/geq/1k`     | 1 kHz     |
| `/lr/geq/1k25`   | 1.25 kHz  |
| `/lr/geq/1k6`    | 1.6 kHz   |
| `/lr/geq/2k`     | 2 kHz     |
| `/lr/geq/2k5`    | 2.5 kHz   |
| `/lr/geq/3k15`   | 3.15 kHz  |
| `/lr/geq/4k`     | 4 kHz     |
| `/lr/geq/5k`     | 5 kHz     |
| `/lr/geq/6k3`    | 6.3 kHz   |
| `/lr/geq/8k`     | 8 kHz     |
| `/lr/geq/10k`    | 10 kHz    |
| `/lr/geq/12k5`   | 12.5 kHz  |
| `/lr/geq/16k`    | 16 kHz    |
| `/lr/geq/20k`    | 20 kHz    |

### Examples

```
/lr/eq/mode ,i 2          # switch LR to TEQ mode
/lr/geq/1k ,f 0.6         # 1 kHz +3 dB
/lr/geq/1k ,f 0.5         # 1 kHz flat
/lr/geq/1k ,f 0.3         # 1 kHz -6 dB
/lr/geq/1k                # read current 1 kHz value
```

---

## Built-in GEQ — Aux Bus 1–6

Same mode switch and same band addressing as LR, with bus number in the path.

### Set EQ mode

```
/bus/{N}/eq/mode ,i {mode}
```

`{N}` = bus number 1–6. Mode values: `0`=PEQ, `1`=GEQ, `2`=TEQ.

### Control bands

```
/bus/{N}/geq/{freq} ,f {float_val}
```

Frequency names are identical to the LR table above (`20`, `25`, `31.5`, ... `20k`).

### Examples

```
/bus/1/eq/mode ,i 1        # switch Bus 1 to GEQ mode
/bus/1/geq/1k ,f 0.6       # Bus 1: 1 kHz +3 dB
/bus/1/geq/1k ,f 0.5       # Bus 1: 1 kHz flat
/bus/2/geq/500 ,f 0.3      # Bus 2: 500 Hz -6 dB
```

---

## FX Rack GEQ / TEQ (Slots 1–4)

GEQ, TEQ, GEQ2, and TEQ2 can be loaded as effects into FX slots 1–4. Unlike the built-in GEQ, bands are addressed by parameter number, not frequency name.

### Effect Types

| Type   | Description            | Parameters   |
|--------|------------------------|--------------|
| `GEQ`  | Stereo Graphic EQ      | par/01–32    |
| `TEQ`  | True Stereo Graphic EQ | par/01–32    |
| `GEQ2` | Dual Graphic EQ        | par/01–64    |
| `TEQ2` | True Dual Graphic EQ   | par/01–64    |

GEQ vs TEQ differs in filter processing (TEQ = linear-phase); the OSC addressing is identical.

### Check what is loaded in a slot

```
/node ,s fx/{slot}
```

Returns the effect type and on/off state, e.g. `TEQ ON` or `GEQ2 OFF`.

### OSC Path

```
/fx/{slot}/par/{nn}
```

- `{slot}` = `1`, `2`, `3`, or `4`
- `{nn}` = two-digit parameter number (`01`–`32` for GEQ/TEQ, `01`–`64` for GEQ2/TEQ2)

### Band → Parameter Table

| par | Frequency |
|-----|-----------|
| 01  | 20 Hz     |
| 02  | 25 Hz     |
| 03  | 31.5 Hz   |
| 04  | 40 Hz     |
| 05  | 50 Hz     |
| 06  | 63 Hz     |
| 07  | 80 Hz     |
| 08  | 100 Hz    |
| 09  | 125 Hz    |
| 10  | 160 Hz    |
| 11  | 200 Hz    |
| 12  | 250 Hz    |
| 13  | 315 Hz    |
| 14  | 400 Hz    |
| 15  | 500 Hz    |
| 16  | 630 Hz    |
| 17  | 800 Hz    |
| 18  | 1 kHz     |
| 19  | 1.25 kHz  |
| 20  | 1.6 kHz   |
| 21  | 2 kHz     |
| 22  | 2.5 kHz   |
| 23  | 3.15 kHz  |
| 24  | 4 kHz     |
| 25  | 5 kHz     |
| 26  | 6.3 kHz   |
| 27  | 8 kHz     |
| 28  | 10 kHz    |
| 29  | 12.5 kHz  |
| 30  | 16 kHz    |
| 31  | 20 kHz    |
| **32** | **Master level** |

### GEQ2 / TEQ2 — Dual side layout

Side A and B each have the same 31 bands plus a master:

| Parameter range | Content           |
|-----------------|-------------------|
| par/01–31       | Side A, bands 1–31 |
| par/32          | Side A master     |
| par/33–63       | Side B, bands 1–31 |
| par/64          | Side B master     |

Side B par = Side A par + 32.

### Examples

```
/node ,s fx/4              # check what is in slot 4
/fx/4/par/18               # read 1 kHz (par 18)
/fx/4/par/18 ,f 0.6        # set 1 kHz to +3 dB
/fx/4/par/18 ,f 0.5        # set 1 kHz flat
/fx/4/par/50 ,f 0.6        # GEQ2 side B: 1 kHz +3 dB (18 + 32 = 50)
```

---

## Sending Commands

Use `XAir_Command` from this project:

```bash
# Interactive
./build/XAir_Command -i 192.168.1.146

# Pipe a sequence (sleep lets responses arrive)
(
  echo '/lr/geq/20 ,f 0.7'
  sleep 2
  echo '/lr/geq/20 ,f 0.3'
  sleep 2
  echo '/lr/geq/20 ,f 0.5'
  sleep 1
  echo 'exit'
) | ./build/XAir_Command -i 192.168.1.146

# Batch file
./build/XAir_Command -i 192.168.1.146 -f settings.txt -k 0
```

In a batch file, use `time {ms}` between commands if you need to receive and process responses:

```
/fx/4/par/18 ,f 0.6
time 50
/fx/4/par/19 ,f 0.4
exit
```
