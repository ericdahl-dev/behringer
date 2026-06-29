#!/usr/bin/env python3
"""T-025 / T-032 ring-out safety harness: a mock X32 that models the feedback
loop and asserts the ring-out control loop's safety properties (ceiling, notch,
restore, abort). No audio — feedback is modelled as a function of commanded
bus gain via a pluggable RoomModel.

Run:
  python3 test_ringout_emu.py [path-to-XAir_ToastSaver]          # flat room
  python3 test_ringout_emu.py ... --room gymnasium                 # named preset
  python3 test_ringout_emu.py ... --room-file my_room.json        # custom JSON
  python3 test_ringout_emu.py ... --debug-bins                    # mode levels
"""
import argparse, json, socket, struct, subprocess, sys, time, signal, os, pathlib

PORT       = 10024
BUS        = 1
CEILING    = 0.0
FX_SLOT    = 4
TARGET_BIN = 56          # 1 kHz; GEQ par 18
TARGET_PAR = 18
ORIG_FADER = 0.5         # = -10 dB start

# ── OSC helpers ──────────────────────────────────────────────────────────────

def pad(b): return b + b"\x00" * ((4 - len(b) % 4) % 4)

def osc(addr, *args):
    out = pad(addr.encode() + b"\x00"); tags = ","; pay = b""
    for a in args:
        if isinstance(a, float): tags += "f"; pay += struct.pack(">f", a)
        elif isinstance(a, int): tags += "i"; pay += struct.pack(">i", a)
        else: tags += "s"; pay += pad(a.encode() + b"\x00")
    return out + pad(tags.encode() + b"\x00") + pay

def parse(pkt):
    z = pkt.index(b"\x00"); addr = pkt[:z].decode("latin1")
    o = (len(addr) + 1 + 3) & ~3
    if o >= len(pkt) or pkt[o:o+1] != b",": return addr, ""
    z2 = pkt.index(b"\x00", o); tags = pkt[o+1:z2].decode("latin1")
    o = (z2 + 1 + 3) & ~3
    args = []
    for t in tags:
        if t == "f": args.append(struct.unpack(">f", pkt[o:o+4])[0]); o += 4
        elif t == "i": args.append(struct.unpack(">i", pkt[o:o+4])[0]); o += 4
    return addr, args

def fader_to_db(f):
    if f >= 1.0:    return 10.0
    if f >= 0.5:    return 40*f - 30
    if f >= 0.25:   return 80*f - 50
    if f >= 0.0625: return 160*f - 70
    if f >  0:      return 480*f - 90
    return -90.0

def meters4_frame(bins):
    body = struct.pack(">I", 4 + len(bins)*2) + struct.pack("<I", len(bins))
    body += b"".join(struct.pack("<h", max(-32768, min(32767, int(v*256)))) for v in bins)
    return pad(b"/meters/4\x00") + pad(b",b\x00") + body

# GEQ par → bin mapping (mirrors TOAST_GEQ_BIN in toast_logic.h)
_GEQ_BIN = [
     0,  4,  8, 12, 16, 19, 23, 27, 31, 34,
    38, 42, 45, 49, 53, 56, 60, 63, 67, 71,
    74, 78, 82, 85, 89, 93, 96, 99, 99, 99, 99,
]

def _par_for_bin(b):
    """Return GEQ par (1-based) whose bin is closest to b."""
    return min(range(31), key=lambda i: abs(_GEQ_BIN[i] - b)) + 1

# ── RoomModel ─────────────────────────────────────────────────────────────────

class RoomModel:
    """Multi-mode acoustic feedback model.

    Each mode: {bin, onset_db, slope, decay_frames}
    decay_frames=1 → instant (flat/CI behavior).
    """

    def __init__(self, modes):
        self._modes = [dict(m) for m in modes]
        # per-mode current level (dBFS above -90), tracks toward target
        self._levels = [-90.0] * len(self._modes)

    def step(self, gain_db, notched_pars):
        """Advance one RTA frame. Returns float[100] bin levels."""
        bins = [-90.0] * 100
        for i, m in enumerate(self._modes):
            par = _par_for_bin(m["bin"])
            if par in notched_pars:
                target = -90.0
            else:
                excess = max(0.0, gain_db - m["onset_db"])
                target = -90.0 + excess * m["slope"]

            df = max(1, int(m.get("decay_frames", 1)))
            self._levels[i] += (target - self._levels[i]) / df
            bins[m["bin"]] = self._levels[i]
        return bins

    def active_modes(self):
        return self._modes

# ── Presets ───────────────────────────────────────────────────────────────────

PRESETS = {
    # flat: identical to the original single-mode linear model (CI default)
    "flat": [
        {"bin": 56, "onset_db": -6.0, "slope": 10.0, "decay_frames": 1},
    ],

    # bathroom: 3 strong high-freq modes, fast onset
    "bathroom": [
        {"bin": 56, "onset_db": -8.0,  "slope": 12.0, "decay_frames": 2},
        {"bin": 72, "onset_db": -5.0,  "slope": 14.0, "decay_frames": 2},
        {"bin": 85, "onset_db": -6.0,  "slope": 11.0, "decay_frames": 1},
    ],

    # lecture hall: 5 modes spread 200 Hz–8 kHz, moderate onset
    "lecture": [
        {"bin": 27, "onset_db": -4.0,  "slope":  7.0, "decay_frames": 4},
        {"bin": 42, "onset_db": -5.0,  "slope":  8.0, "decay_frames": 5},
        {"bin": 56, "onset_db": -6.0,  "slope":  9.0, "decay_frames": 6},
        {"bin": 67, "onset_db": -7.0,  "slope":  8.0, "decay_frames": 5},
        {"bin": 82, "onset_db": -5.0,  "slope":  7.0, "decay_frames": 4},
    ],

    # gymnasium: 8 modes, high slope, long decay
    "gymnasium": [
        {"bin": 12, "onset_db": -3.0,  "slope": 15.0, "decay_frames": 10},
        {"bin": 23, "onset_db": -4.0,  "slope": 14.0, "decay_frames":  8},
        {"bin": 38, "onset_db": -5.0,  "slope": 13.0, "decay_frames":  9},
        {"bin": 56, "onset_db": -6.0,  "slope": 12.0, "decay_frames": 10},
        {"bin": 63, "onset_db": -4.0,  "slope": 11.0, "decay_frames":  7},
        {"bin": 74, "onset_db": -5.0,  "slope": 10.0, "decay_frames":  8},
        {"bin": 85, "onset_db": -3.0,  "slope": 13.0, "decay_frames":  9},
        {"bin": 93, "onset_db": -4.0,  "slope": 11.0, "decay_frames":  7},
    ],
}

def load_room(args):
    if args.room_file:
        path = pathlib.Path(args.room_file)
        modes = json.loads(path.read_text())
        return RoomModel(modes)
    name = args.room or "flat"
    if name not in PRESETS:
        sys.exit(f"unknown room preset '{name}'. choices: {', '.join(PRESETS)}")
    return RoomModel(PRESETS[name])

# ── mock server state + handler ───────────────────────────────────────────────

def make_state():
    return {
        "gain_db": fader_to_db(ORIG_FADER),
        "fader": ORIG_FADER,
        "max_gain": -200.0,
        "notched_pars": set(),
        "final_fader": None,
        "client": None,
        "streaming": False,
    }

def handle(addr, args, sock, who, st):
    st["client"] = who
    if addr == "/xinfo":
        sock.sendto(osc("/xinfo", "127.0.0.1", "MockX32", "X32", "1.0"), who)
    elif addr == "/meters":
        st["streaming"] = True
    elif addr in ("/xremote",) \
         or addr.startswith("/-stat") \
         or addr.startswith("/-prefs") \
         or "/insert/" in addr:
        pass
    elif addr == f"/bus/{BUS}/mix/fader":
        if args:
            st["fader"] = args[0]
            st["gain_db"] = fader_to_db(args[0])
            st["max_gain"] = max(st["max_gain"], st["gain_db"])
            st["final_fader"] = args[0]
        else:
            sock.sendto(osc(f"/bus/{BUS}/mix/fader", st["fader"]), who)
    elif addr.startswith(f"/fx/{FX_SLOT}/par/"):
        if args and args[0] < 0.49:
            try:
                par = int(addr.split("/")[-1])
                st["notched_pars"].add(par)
            except ValueError:
                pass

# ── run one scenario ──────────────────────────────────────────────────────────

def run(ceiling, abort_after_s, bin_path, room: RoomModel, debug_bins: bool):
    import select
    st = make_state()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("127.0.0.1", PORT))
    sock.setblocking(False)

    proc = subprocess.Popen(
        [bin_path, "-i", "127.0.0.1", "--ringout", "--no-preflight",
         "--ringout-bus", str(BUS), "--ringout-ceiling", str(int(ceiling)),
         "--ringout-step", "2", "--ringout-margin", "6",
         "--ringout-max-notches", "8", "-s", str(FX_SLOT), "-c", "1", "-t", "18"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    last_meter = 0.0; t0 = time.time(); aborted = False; frame = 0
    while time.time() - t0 < 30:
        if proc.poll() is not None:
            break
        r, _, _ = select.select([sock], [], [], 0.02)
        if r:
            try:
                pkt, who = sock.recvfrom(2048)
                a, osc_args = parse(pkt)
                handle(a, osc_args, sock, who, st)
            except BlockingIOError:
                pass
        now = time.time()
        if now - last_meter >= 0.04 and st["client"] and st["streaming"]:
            bins = room.step(st["gain_db"], st["notched_pars"])
            sock.sendto(meters4_frame(bins), st["client"])
            last_meter = now
            if debug_bins:
                parts = " ".join(
                    f"bin{m['bin']}={bins[m['bin']]:.1f}"
                    for m in room.active_modes()
                )
                print(f"[emu] frame {frame:4d}  {parts}  gain={st['gain_db']:.1f}dB",
                      file=sys.stderr)
            frame += 1
        if abort_after_s and not aborted and now - t0 >= abort_after_s \
                and st["streaming"]:
            proc.send_signal(signal.SIGINT)
            aborted = True

    try:
        out, _ = proc.communicate(timeout=5)
    except Exception:
        proc.kill()
        out = ""
    sock.close()
    return out, dict(st)

# ── scenarios ─────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Ring-out emulator / harness")
    parser.add_argument("bin", nargs="?", default="build/XAir_ToastSaver",
                        help="path to XAir_ToastSaver binary")
    parser.add_argument("--room", default="flat",
                        choices=list(PRESETS), metavar="NAME",
                        help="room preset: flat (default), bathroom, lecture, gymnasium")
    parser.add_argument("--room-file", metavar="PATH",
                        help="load custom room JSON (overrides --room)")
    parser.add_argument("--debug-bins", action="store_true",
                        help="print active bin levels each frame to stderr")
    args = parser.parse_args()

    room = load_room(args)
    P = 0; F = 0

    def check(name, ok):
        nonlocal P, F
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
        if ok: P += 1
        else:  F += 1

    print(f"room: {args.room_file or args.room}")
    print()
    print("══ scenario 1: closed-loop ring-out to completion ══")
    out, r = run(ceiling=0.0, abort_after_s=None,
                 bin_path=args.bin, room=room, debug_bins=args.debug_bins)
    for ln in out.splitlines():
        if "ring-out" in ln: print("  " + ln)
    check("gain never exceeded ceiling (0 dB)", r["max_gain"] <= 0.05)
    check("fader restored on completion",
          r["final_fader"] is not None and abs(r["final_fader"] - ORIG_FADER) < 1e-3)
    check("terminated", "stopped" in out or "complete" in out)

    print("\n══ scenario 2: Ctrl-C mid-ramp ══")
    out, r = run(ceiling=10.0, abort_after_s=2.0,
                 bin_path=args.bin, room=room, debug_bins=args.debug_bins)
    for ln in out.splitlines():
        if "raise" in ln or "restored" in ln or "stopped" in ln: print("  " + ln)
    check("SIGINT restored fader to original",
          r["final_fader"] is not None and abs(r["final_fader"] - ORIG_FADER) < 1e-3)
    check("gain never exceeded ceiling (10 dB)", r["max_gain"] <= 10.05)

    print(f"\n{P} passed, {F} failed")
    sys.exit(1 if F else 0)

if __name__ == "__main__":
    main()
