#!/usr/bin/env python3
"""Live /meters/4 RTA peak-bin probe for X32/XAir — field tool for T-027.

Subscribe to the 100-bin RTA, print the peak bin (and its modelled frequency
from cli/rta_bins.c). Use with a known tone to verify the bin->Hz table:
a 1 kHz pistonphone must light bin 56; a console sine sweep verifies the rest.

Usage:  rta_probe.py <xr18_ip> [seconds]
"""
import socket, struct, sys, time, re, pathlib

IP = sys.argv[1] if len(sys.argv) > 1 else None
SECS = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
if not IP:
    sys.exit("usage: rta_probe.py <xr18_ip> [seconds]")
PORT = 10024

# modelled bin->Hz, parsed from the C table so the two never drift
try:
    txt = pathlib.Path(__file__).with_name("rta_bins.c").read_text()
    BIN_FREQ = [float(x) for x in re.findall(r"([0-9]+\.[0-9]+)f", txt)[:100]]
except Exception:
    BIN_FREQ = None


def pad(b):
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


def osc(addr, *args):
    out = pad(addr.encode() + b"\x00")
    tags, payload = ",", b""
    for a in args:
        if isinstance(a, str):
            tags += "s"; payload += pad(a.encode() + b"\x00")
        else:
            tags += "i"; payload += struct.pack(">i", a)
    return out + pad(tags.encode() + b"\x00") + payload


def parse_meters4(pkt):
    """Return list[100] of dBFS bins, or None. Blob = [BE size][LE count][int16 LE]."""
    if not pkt.startswith(b"/meters/4"):
        return None
    ti = pkt.find(b",b")
    if ti < 0:
        return None
    b = ti
    while pkt[b] != 0:
        b += 1
    b = (b + 4) & ~3                       # start of OSC blob (its 4-byte size)
    if struct.unpack("<I", pkt[b + 4:b + 8])[0] != 100:
        return None                        # count != 100 → not the RTA
    return [struct.unpack("<h", pkt[b + 8 + i * 2:b + 10 + i * 2])[0] / 256.0
            for i in range(100)]


s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("", 0)); s.settimeout(1.0)
s.sendto(osc("/xremote"), (IP, PORT))
s.sendto(osc("/meters", "/meters/4", 0, 0, 0), (IP, PORT))

end, frames, last = time.time() + SECS, 0, None
while time.time() < end:
    if time.time() - (last[2] if last else 0) > 8:   # renew the subscription
        s.sendto(osc("/meters", "/meters/4", 0, 0, 0), (IP, PORT))
    try:
        pkt, _ = s.recvfrom(8192)
    except socket.timeout:
        continue
    bins = parse_meters4(pkt)
    if bins is None:
        continue
    frames += 1
    peak = max(range(100), key=lambda i: bins[i])
    last = (peak, bins[peak], time.time())

if last:
    peak, val, _ = last
    hz = f" ≈ {BIN_FREQ[peak]:.0f} Hz" if BIN_FREQ else ""
    print(f"frames={frames}  PEAK bin {peak}{hz}  ({val:.1f} dBFS)")
else:
    print(f"no RTA frames ({frames}) — check IP/reachability; /xinfo first")
