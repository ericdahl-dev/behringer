#!/usr/bin/env python3
"""
Verify two fixes:
1. IP bind fix (en0 -> first non-loopback): emulator starts and accepts UDP
2. function_node_single fix: /node single-arg returns a response
"""
import socket
import struct
import subprocess
import time
import sys
import os

HOST = "127.0.0.1"
PORT = 10023

def pad4(b):
    b += b"\x00"
    while len(b) % 4:
        b += b"\x00"
    return b

def osc(address, *args):
    data = pad4(address.encode())
    if not args:
        data += b",\x00\x00\x00"
        return data
    type_tag = ","
    arg_bytes = b""
    for a in args:
        if isinstance(a, str):
            type_tag += "s"
            arg_bytes += pad4(a.encode())
        elif isinstance(a, float):
            type_tag += "f"
            arg_bytes += struct.pack(">f", a)
        elif isinstance(a, int):
            type_tag += "i"
            arg_bytes += struct.pack(">i", a)
    data += pad4(type_tag.encode())
    data += arg_bytes
    return data

passed = failed = 0

def ok(name):
    global passed
    passed += 1
    print(f"PASS  {name}")

def fail(name, reason):
    global failed
    failed += 1
    print(f"FAIL  {name}: {reason}")

proc = subprocess.Popen(
    ["./build/X32", "-i", HOST, "-v", "0"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
time.sleep(0.3)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2.0)

# Test 1: /info -> emulator responds (verifies bind/IP fix)
try:
    sock.sendto(osc("/info"), (HOST, PORT))
    data, _ = sock.recvfrom(4096)
    if len(data) > 0:
        ok("/info responds (IP bind fix)")
    else:
        fail("/info responds (IP bind fix)", "empty response")
except Exception as e:
    fail("/info responds (IP bind fix)", str(e))

# Test 2: /node ,s ch/01/config -> response contains "node" (verifies function_node_single fix)
try:
    sock.sendto(osc("/node", "ch/01/config"), (HOST, PORT))
    data, _ = sock.recvfrom(4096)
    if b"node" in data:
        ok("/node single-arg responds (function_node_single fix)")
    else:
        fail("/node single-arg responds (function_node_single fix)", f"got: {data!r}")
except Exception as e:
    fail("/node single-arg responds (function_node_single fix)", str(e))

sock.close()
proc.terminate()
proc.wait()

print(f"\n{passed} passed, {failed} failed")
sys.exit(failed)
