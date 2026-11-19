#!/usr/bin/env python3
"""A client that half-closes after its request still gets its response (HTTP allows shutdown(SHUT_WR)).

    wide_search_halfclose.py BIN OUT PORT

Starts a node, writes a document, and sends each read plainly once and half-closed five times: every half-closed
reply must carry the plain one's status. Exits 1 on a difference.
"""

from __future__ import annotations

import os
import shutil
import signal
import socket
import subprocess
import sys
import time

BIN, OUT, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])


def send(method: str, target: str, body: bytes = b"", half_close: bool = False) -> str:
    head = f"{method} {target} HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
    if body:
        head += f"Content-Type: application/json\r\nContent-Length: {len(body)}\r\n"
    with socket.create_connection(("127.0.0.1", PORT), timeout=10) as s:
        s.sendall(head.encode() + b"\r\n" + body)
        if half_close:
            s.shutdown(socket.SHUT_WR)
        reply = b""
        while chunk := s.recv(65536):
            reply += chunk
    return reply.split(b"\r\n", 1)[0].decode(errors="replace")


shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(OUT)
node = subprocess.Popen(
    [BIN, "--port", str(PORT), "--name", "halfclose", "-D", os.path.join(OUT, "data"), "--processors", "2"],
    stdout=open(os.path.join(OUT, "node.log"), "wb"),
    stderr=subprocess.STDOUT,
)
try:
    for _ in range(240):
        try:
            if send("GET", "/").startswith("HTTP/1.1 200"):
                break
        except OSError:
            time.sleep(0.5)
    else:
        raise SystemExit("the server did not start")
    print("PUT", send("PUT", "/hc/1?commit", b'{"marker": "half-way"}'))
    misses = []
    for method, target in [("GET", "/hc/1"), ("GET", "/hc/:search?q=marker:half-way"), ("GET", "/hc/:count")]:
        plain = send(method, target)
        halves = [send(method, target, half_close=True) for _ in range(5)]
        ok = all(h.split(" ")[1:2] == plain.split(" ")[1:2] for h in halves)
        print(f"{'ok  ' if ok else 'MISS'} {method} {target}: plain {plain!r}, half-closed x5 {halves!r}")
        if not ok:
            misses.append(target)
finally:
    node.send_signal(signal.SIGTERM)
    node.wait(60)
print("PASS" if not misses else "FAIL: " + ", ".join(misses))
sys.exit(1 if misses else 0)
