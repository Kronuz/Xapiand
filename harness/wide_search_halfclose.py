#!/usr/bin/env python3
"""A client that half-closes after its request still gets its response (HTTP allows shutdown(SHUT_WR)).

    wide_search_halfclose.py BIN OUT PORT [--baseline BASE_BIN]

Starts a node, writes a document, and sends each read plainly once and half-closed five times: a read misses when a
half-closed reply's status differs from the plain one's. Alone it exits 1 on any miss; with --baseline it runs the
same reads on BASE_BIN too and exits 1 only when BIN misses a read the baseline answers (a regression).
"""

from __future__ import annotations

import argparse
import os
import shutil
import signal
import socket
import subprocess
import time

READS = [("GET", "/hc/1"), ("GET", "/hc/:search?q=marker:half-way"), ("GET", "/hc/:count")]

ap = argparse.ArgumentParser()
ap.add_argument("bin")
ap.add_argument("out")
ap.add_argument("port", type=int)
ap.add_argument("--baseline")
args = ap.parse_args()


def send(port: int, method: str, target: str, body: bytes = b"", half_close: bool = False) -> str:
    head = f"{method} {target} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
    if body:
        head += f"Content-Type: application/json\r\nContent-Length: {len(body)}\r\n"
    with socket.create_connection(("127.0.0.1", port), timeout=10) as s:
        s.sendall(head.encode() + b"\r\n" + body)
        if half_close:
            s.shutdown(socket.SHUT_WR)
        reply = b""
        while chunk := s.recv(65536):
            reply += chunk
    return reply.split(b"\r\n", 1)[0].decode(errors="replace")


def status(line: str) -> str:
    return line.split(" ")[1] if " " in line else line


def run(name: str, binary: str, out: str, port: int) -> dict[str, bool]:
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    node = subprocess.Popen(
        [binary, "--port", str(port), "--name", "halfclose", "-D", os.path.join(out, "data"), "--processors", "2"],
        stdout=open(os.path.join(out, "node.log"), "wb"),
        stderr=subprocess.STDOUT,
    )
    ok: dict[str, bool] = {}
    try:
        for _ in range(240):
            try:
                if send(port, "GET", "/").startswith("HTTP/1.1 200"):
                    break
            except OSError:
                time.sleep(0.5)
        else:
            raise SystemExit(f"{name}: the server did not start")
        put = send(port, "PUT", "/hc/1?commit", b'{"marker": "half-way"}')
        print(f"{name}: PUT {put}")
        for method, target in READS:
            plain = status(send(port, method, target))
            halves = [status(send(port, method, target, half_close=True)) for _ in range(5)]
            ok[target] = all(h == plain for h in halves)
            print(f"{name}: {'ok  ' if ok[target] else 'MISS'} {method} {target}: plain {plain}, half-closed {halves}")
    finally:
        node.send_signal(signal.SIGTERM)
        node.wait(60)
    return ok


got = run("bin", args.bin, os.path.join(args.out, "bin"), args.port)
if args.baseline:
    base = run("baseline", args.baseline, os.path.join(args.out, "baseline"), args.port + 1)
    misses = [t for t in got if base[t] and not got[t]]
    print("PASS: no read the baseline answers is missed" if not misses else "FAIL (regressions): " + ", ".join(misses))
else:
    misses = [t for t in got if not got[t]]
    print("PASS" if not misses else "FAIL: " + ", ".join(misses))
raise SystemExit(1 if misses else 0)
