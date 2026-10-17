#!/usr/bin/env python3
"""tls_bounds.py BINARY [PORT]

HTTPS bounds (docs/design/transport-security.md, T3), against solo nodes it starts:
  1. 65 connections stalled mid-handshake: the 65th refused, the 64 dropped at 5 s
  2. at --tls-max-connections, the next connection refused; RSS per connection stated
  3. --tls-max-connections above the open-files limit refused at startup
  4. idle 75 s after the handshake, and 75 s after a reply: closed
  5. a body that stops for 60 s, and a client that stops reading for 60 s: closed
  6. headers trickled past 30 s: closed
  7. a RESTORE body streamed for longer than 120 s: completes
Cases 4 to 7 run at once. Exits 0 when every case holds.
"""

from __future__ import annotations

import os
import resource
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.request

BIN = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18895
HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(HERE), ".scratch", "tls_bounds")
CAP = 200  # --tls-max-connections for case 2
SLACK = 2.5  # seconds a bound may run late: timer granularity and the client's own polling

failures: list[str] = []
lock = threading.Lock()


def report(ok: bool, what: str) -> None:
    with lock:
        print(("  PASS  " if ok else "  FAIL  ") + what, flush=True)
        if not ok:
            failures.append(what)


def context() -> ssl.SSLContext:
    ctx = ssl.create_default_context(cafile=os.path.join(WORK, "certs", "ca.pem"))
    ctx.check_hostname = False
    return ctx


class Node:
    def __init__(self, name: str, *flags: str, nofile: int | None = None) -> None:
        data = os.path.join(WORK, name)
        shutil.rmtree(data, ignore_errors=True)
        os.makedirs(data)
        self.log = os.path.join(WORK, name + ".log")
        certs = os.path.join(WORK, "certs")
        tls = ["--tls-cert", os.path.join(certs, "server.pem"), "--tls-key", os.path.join(certs, "server.key")]

        def limit() -> None:
            if nofile:
                resource.setrlimit(resource.RLIMIT_NOFILE, (nofile, nofile))

        with open(self.log, "w") as out:
            self.proc = subprocess.Popen(
                [BIN, "--port", str(PORT), "--name", name, "--solo", "-D", data, *tls, *flags],
                stdout=out, stderr=subprocess.STDOUT, preexec_fn=limit)

    def ready(self) -> bool:
        for _ in range(120):
            if self.proc.poll() is not None:
                return False
            try:
                with urllib.request.urlopen(f"https://localhost:{PORT}/", context=context(), timeout=2) as r:
                    if r.status == 200:
                        return True
            except Exception:
                pass
            time.sleep(0.5)
        return False

    def rss_kb(self) -> int:
        out = subprocess.run(["ps", "-o", "rss=", "-p", str(self.proc.pid)], capture_output=True, text=True)
        return int(out.stdout.strip() or 0)

    def logged(self, text: str) -> bool:
        time.sleep(1)
        with open(self.log, errors="replace") as f:
            return text in f.read()

    def stop(self) -> None:
        self.proc.terminate()
        try:
            self.proc.wait(30)
        except subprocess.TimeoutExpired:
            self.proc.kill()


def closed_at(sock: socket.socket | ssl.SSLSocket, start: float, limit: float) -> float | None:
    """Reads until EOF or a reset; returns the seconds from start, or None past limit."""
    sock.settimeout(1)
    while time.monotonic() - start < limit:
        try:
            if not sock.recv(65536):
                return time.monotonic() - start
        except (TimeoutError, socket.timeout):
            continue
        except (ssl.SSLError, OSError):
            return time.monotonic() - start
    return None


def tls_connect() -> ssl.SSLSocket:
    raw = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    return context().wrap_socket(raw, server_hostname="localhost")


def near(t: float | None, bound: float) -> bool:
    return t is not None and bound - 1 <= t <= bound + SLACK


def fmt(t: float | None) -> str:
    return "never" if t is None else f"{t:.1f} s"


def handshakes() -> None:
    # A ClientHello's first bytes, so each connection is mid-handshake.
    stalled = []
    start = time.monotonic()
    for _ in range(64):
        s = socket.create_connection(("127.0.0.1", PORT), timeout=10)
        s.sendall(b"\x16\x03\x01\x00")
        stalled.append(s)
    time.sleep(0.5)
    extra = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    extra.sendall(b"\x16\x03\x01\x00")
    t = closed_at(extra, time.monotonic(), 4)
    report(t is not None and t < 1, f"65th stalled handshake refused at once ({fmt(t)})")
    times = [closed_at(s, start, 12) for s in stalled]
    late = [t for t in times if not near(t, 5)]
    report(not late, f"64 stalled handshakes dropped at 5 s (first {fmt(min(t or 99 for t in times))}, "
           f"last {fmt(max(t or 99 for t in times))}, {len(late)} outside 4 to {5 + SLACK} s)")


def connections(node: Node) -> None:
    base = node.rss_kb()
    held = []
    for _ in range(CAP):
        held.append(tls_connect())
    time.sleep(1)
    at_cap = node.rss_kb()
    try:
        s = tls_connect()
        t = closed_at(s, time.monotonic(), 4)
        refused = t is not None
    except (ssl.SSLError, OSError):
        refused = True
    report(refused and node.logged("--tls-max-connections reached"),
           f"connection {CAP + 1} refused at --tls-max-connections {CAP}, logged")
    print(f"        RSS {base} KB idle, {at_cap} KB with {CAP} TLS connections: "
          f"{(at_cap - base) / CAP:.1f} KB per connection", flush=True)
    for s in held:
        s.close()


def idle_after_handshake() -> None:
    s = tls_connect()
    t = closed_at(s, time.monotonic(), 90)
    report(near(t, 75), f"idle after the handshake closed at 75 s ({fmt(t)})")


def idle_after_reply() -> None:
    s = tls_connect()
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    s.settimeout(10)
    got = b""
    while b"\r\n\r\n" not in got:
        got += s.recv(65536)
    head, _, body = got.partition(b"\r\n\r\n")
    length = int(next(line.split(b":")[1] for line in head.split(b"\r\n") if line.lower().startswith(b"content-length")))
    while len(body) < length:
        body += s.recv(65536)
    t = closed_at(s, time.monotonic(), 90)
    report(near(t, 75), f"idle after a reply closed at 75 s ({fmt(t)})")


def stalled_body() -> None:
    s = tls_connect()
    s.sendall(b"PUT /bounds/1 HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
              b"Content-Length: 1000\r\n\r\n{\"a\":")
    t = closed_at(s, time.monotonic(), 75)
    report(near(t, 60), f"a body stopped for 60 s closed ({fmt(t)})")


def unread_reply() -> None:
    s = tls_connect()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    request = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    s.sendall(request * 20000)  # far more reply than the socket buffers hold
    start = time.monotonic()
    time.sleep(70)
    t = closed_at(s, start, 85)
    report(t is not None and t < 80, f"a client not reading its replies for 60 s closed (EOF reached at {fmt(t)})")


def trickled_headers() -> None:
    s = tls_connect()
    start = time.monotonic()
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
    s.settimeout(0.1)
    t = None
    while time.monotonic() - start < 45:
        try:
            s.sendall(b"X")
            if s.recv(65536) == b"":
                t = time.monotonic() - start
                break
        except (TimeoutError, socket.timeout, ssl.SSLWantReadError):
            pass
        except (ssl.SSLError, OSError):
            t = time.monotonic() - start
            break
        time.sleep(2)
    report(near(t, 30), f"headers trickled past 30 s closed ({fmt(t)})")


def slow_restore() -> None:
    lines = [b'{"_id": %d, "n": %d}\n' % (i, i) for i in range(14)]
    body = b"".join(lines)
    s = tls_connect()
    s.sendall(b"RESTORE /bounds_restore/ HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-ndjson\r\n"
              b"Content-Length: %d\r\n\r\n" % len(body))
    start = time.monotonic()
    for line in lines:
        s.sendall(line)
        time.sleep(10)
    s.settimeout(60)
    reply = b""
    try:
        while b"\r\n" not in reply:
            chunk = s.recv(65536)
            if not chunk:
                break
            reply += chunk
    except (ssl.SSLError, OSError):
        pass
    status = reply.split(b"\r\n", 1)[0].decode(errors="replace")
    took = time.monotonic() - start
    report(took > 120 and " 2" in status[:13], f"a RESTORE streamed for {took:.0f} s completes ({status or 'no reply'})")


def main() -> int:
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    subprocess.run([os.path.join(HERE, "tls_certs.sh"), os.path.join(WORK, "certs")], check=True, stdout=subprocess.DEVNULL)
    print(f"== [T3] TLS bounds on :{PORT}", flush=True)

    node = Node("fdlimit", "--tls-max-connections", "1000", nofile=512)
    node.proc.wait(30)
    report(node.proc.returncode != 0 and node.logged("does not fit the open-files limit"),
           "--tls-max-connections 1000 under 512 open files refused at startup")

    node = Node("bounds", "--tls-max-connections", str(CAP))
    try:
        if not node.ready():
            report(False, "node did not start")
            return 1
        handshakes()
        time.sleep(6)
        connections(node)
        time.sleep(2)
        cases = [idle_after_handshake, idle_after_reply, stalled_body, unread_reply, trickled_headers, slow_restore]
        threads = [threading.Thread(target=c) for c in cases]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    finally:
        node.stop()
    print("tls_bounds: OK" if not failures else f"tls_bounds: {len(failures)} failed", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
