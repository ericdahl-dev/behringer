#!/usr/bin/env python3
"""T-025 ring-out safety harness: a mock X32 that models the feedback loop and
asserts the ring-out control loop's safety properties (ceiling, notch, restore,
abort). No audio — feedback is modelled as a function of commanded bus gain.

Run:  python3 test_ringout_emu.py [path-to-XAir_ToastSaver]
"""
import socket, struct, subprocess, sys, time, signal, os, pathlib

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/XAir_ToastSaver"
PORT      = 10024
BUS       = 1
CEILING   = 0.0          # --ringout-ceiling
FX_SLOT   = 4
TARGET_BIN = 56          # 1 kHz; GEQ par 18 (TOAST_GEQ_BIN[17])
TARGET_PAR = 18
ONSET_DB  = -6.0         # feedback starts as gain rises past this
SLOPE     = 10.0         # dB of ring per dB of gain over onset
NOTCH_DB  = 35.0         # how much a notch removes
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

# X32 fader law (matches fader_db.c)
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

# ── mock state ───────────────────────────────────────────────────────────────
st = {"gain_db": fader_to_db(ORIG_FADER), "fader": ORIG_FADER,
      "max_gain": -200.0, "notched": False, "final_fader": None,
      "notch_par": None, "client": None, "streaming": False}

def model_bins():
    bins = [-90.0]*100
    ring = max(0.0, (st["gain_db"] - ONSET_DB)) * SLOPE
    if st["notched"]: ring -= NOTCH_DB
    bins[TARGET_BIN] = -90.0 + max(0.0, ring)
    return bins

def handle(addr, args, sock, who):
    st["client"] = who
    if addr == "/xinfo":
        sock.sendto(osc("/xinfo", "127.0.0.1", "MockX32", "X32", "1.0"), who)
    elif addr == "/meters":
        st["streaming"] = True            # only stream once the app subscribes
    elif addr == "/xremote" or addr.startswith("/-stat") or addr.startswith("/-prefs") \
         or "/insert/" in addr:
        pass  # accepted, no reply needed
    elif addr == f"/bus/{BUS}/mix/fader":
        if args:                                   # SET
            st["fader"] = args[0]; st["gain_db"] = fader_to_db(args[0])
            st["max_gain"] = max(st["max_gain"], st["gain_db"])
            st["final_fader"] = args[0]
        else:                                       # QUERY
            sock.sendto(osc(f"/bus/{BUS}/mix/fader", st["fader"]), who)
    elif addr == f"/fx/{FX_SLOT}/par/{TARGET_PAR:02d}":
        if args and args[0] < 0.49:                # a cut on the fed-back band
            st["notched"] = True; st["notch_par"] = TARGET_PAR

# ── run one scenario ─────────────────────────────────────────────────────────
def run(ceiling, abort_after_s):
    st.update({"gain_db": fader_to_db(ORIG_FADER), "fader": ORIG_FADER,
               "max_gain": -200.0, "notched": False, "final_fader": None,
               "notch_par": None, "client": None, "streaming": False})
    import select
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("127.0.0.1", PORT)); sock.setblocking(False)
    proc = subprocess.Popen(
        [BIN, "-i", "127.0.0.1", "--ringout", "--no-preflight",
         "--ringout-bus", str(BUS), "--ringout-ceiling", str(int(ceiling)),
         "--ringout-step", "2", "--ringout-margin", "6",
         "--ringout-max-notches", "8", "-s", str(FX_SLOT), "-c", "1", "-t", "18"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    last_meter = 0.0; t0 = time.time(); aborted = False
    while time.time() - t0 < 25:
        if proc.poll() is not None: break
        r, _, _ = select.select([sock], [], [], 0.02)
        if r:
            try:
                pkt, who = sock.recvfrom(2048); a, args = parse(pkt); handle(a, args, sock, who)
            except BlockingIOError: pass
        now = time.time()
        if now - last_meter >= 0.04 and st["client"] and st["streaming"]:
            sock.sendto(meters4_frame(model_bins()), st["client"]); last_meter = now
        if abort_after_s and not aborted and now - t0 >= abort_after_s and st["streaming"]:
            proc.send_signal(signal.SIGINT); aborted = True
    try: out, _ = proc.communicate(timeout=5)
    except Exception: proc.kill(); out = ""
    sock.close()
    return out, dict(st)

def main():
    P = 0; F = 0
    def check(name, ok):
        nonlocal P, F
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")
        if ok: P += 1
        else: F += 1

    print("══ scenario 1: closed-loop ring-out to completion ══")
    out, r = run(ceiling=0.0, abort_after_s=None)
    for ln in out.splitlines():
        if "ring-out" in ln: print("  " + ln)
    check("gain never exceeded ceiling (0 dB)", r["max_gain"] <= 0.05)
    check("fed-back 1 kHz band notched (par 18)", r["notched"] and r["notch_par"] == 18)
    check("fader restored on completion", r["final_fader"] is not None and abs(r["final_fader"] - ORIG_FADER) < 1e-3)
    check("terminated (not left running)", "stopped" in out or "complete" in out)

    print("\n══ scenario 2: Ctrl-C mid-ramp ══")
    out, r = run(ceiling=10.0, abort_after_s=2.0)   # high ceiling so it keeps ramping
    for ln in out.splitlines():
        if "raise" in ln or "restored" in ln or "stopped" in ln: print("  " + ln)
    check("SIGINT restored fader to original", r["final_fader"] is not None and abs(r["final_fader"] - ORIG_FADER) < 1e-3)
    check("gain never exceeded ceiling (10 dB)", r["max_gain"] <= 10.05)

    print(f"\n{P} passed, {F} failed")
    sys.exit(1 if F else 0)

if __name__ == "__main__":
    main()
