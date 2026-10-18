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
Cases 4 to 7 run at once. Exits 0 when every case holds. ONLY=name,... runs just those
cases, by function name (fdlimit, handshakes, connections, idle_after_handshake, ...).
OUT=FILE writes one JSON record per row checked: case, bound, measured, unit, pass.
"""

from __future__ import annotations

import contextlib
import fcntl
import http.client
import json
import os
import re
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import termios
import threading
import time
import urllib.request
from collections.abc import Callable

BIN = sys.argv[1]
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 18895
HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.environ.get("WORK") or os.path.join(
    os.path.dirname(HERE), ".scratch", "tls_bounds"
)
CAP = 200  # --tls-max-connections for case 2
SLACK = (
    2.5  # seconds a bound may run late: timer granularity and the client's own polling
)

failures: list[str] = []
rows: list[dict[str, object]] = []
lock = threading.Lock()


def report(
    ok: bool,
    what: str,
    case: str,
    bound: float | None = None,
    measured: float | None = None,
    unit: str = "s",
    extra: dict[str, object] | None = None,
) -> None:
    with lock:
        print(("  PASS  " if ok else "  FAIL  ") + what, flush=True)
        rows.append(
            {
                "case": case,
                "what": what,
                "bound": bound,
                "measured": measured,
                "unit": unit,
                "pass": ok,
                **(extra or {}),
            }
        )
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
        tls = [
            "--tls-cert",
            os.path.join(certs, "server.pem"),
            "--tls-key",
            os.path.join(certs, "server.key"),
        ]

        command = [
            BIN,
            "--port",
            str(PORT),
            "--name",
            name,
            "--solo",
            "-D",
            data,
            *tls,
            *flags,
        ]
        if nofile:  # the shell sets the limit, so no preexec_fn runs beside threads
            command = [
                "/bin/sh",
                "-c",
                f'ulimit -n {nofile} && exec "$@"',
                "sh",
                *command,
            ]
        with open(self.log, "w") as out:
            self.proc = subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT)

    def ready(self) -> bool:
        for _ in range(120):
            if self.proc.poll() is not None:
                return False
            url = f"https://localhost:{PORT}/"
            with (
                contextlib.suppress(OSError, http.client.HTTPException),
                urllib.request.urlopen(url, context=context(), timeout=2) as r,
            ):  # an error means not up yet
                if r.status == 200:
                    return True
            time.sleep(0.5)
        return False

    def rss_kb(self) -> int:
        out = subprocess.run(
            ["ps", "-o", "rss=", "-p", str(self.proc.pid)],
            capture_output=True,
            text=True,
            check=False,
        )
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


def closed_at(
    sock: socket.socket | ssl.SSLSocket, start: float, limit: float
) -> float | None:
    """Reads until EOF or a reset; returns the seconds from start, or None past limit."""
    sock.settimeout(1)
    while time.monotonic() - start < limit:
        try:
            if not sock.recv(65536):
                return time.monotonic() - start
        except TimeoutError:
            continue
        except (ssl.SSLError, OSError):
            return time.monotonic() - start
    return None


def refusals() -> dict[str, float]:
    """The xapiand_tls_refused_connections counters from /:metrics, by reason."""
    with urllib.request.urlopen(
        f"https://localhost:{PORT}/:metrics", context=context(), timeout=5
    ) as r:
        text = r.read().decode()
    found = re.findall(
        r'^xapiand_tls_refused_connections\{[^}]*reason="(\w+)"[^}]*\} (\S+)$',
        text,
        re.MULTILINE,
    )
    return {reason: float(value) for reason, value in found}


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
    report(
        t is not None and t < 1,
        f"65th stalled handshake refused at once ({fmt(t)})",
        "handshake_cap",
        0,
        t,
    )
    times = [closed_at(s, start, 12) for s in stalled]
    late = [t for t in times if not near(t, 5)]
    done = [t for t in times if t is not None]
    first, last = (min(done), max(done)) if len(done) == len(times) else (None, None)
    report(
        not late,
        f"64 stalled handshakes dropped at 5 s (first {fmt(first)}, last {fmt(last)}, "
        f"{len(late)} outside 4 to {5 + SLACK} s)",
        "handshake_deadline",
        5,
        last,
    )
    counts = refusals()
    report(
        counts.get("handshakes", 0) >= 1 and counts.get("deadline", 0) >= 64,
        f"metrics count them: handshakes {counts.get('handshakes', 0):.0f}, deadline {counts.get('deadline', 0):.0f}",
        "handshake_metrics",
        64,
        counts.get("deadline", 0),
        "refusals",
    )


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
    time.sleep(1)  # the control-plane check reads reactor's rejected count every second
    report(
        refused and node.logged("refused at accept: --tls-max-connections reached"),
        f"connection {CAP + 1} refused at --tls-max-connections {CAP}, logged as cap",
        "connection_cap",
        CAP,
        CAP if refused else None,
        "connections",
    )
    per = (at_cap - base) / CAP
    print(
        f"        RSS {base} KB idle, {at_cap} KB with {CAP} TLS connections: {per:.1f} KB per connection",
        flush=True,
    )
    with lock:
        rows.append(
            {
                "case": "rss_per_connection",
                "what": f"RSS {base} KB idle, {at_cap} KB with {CAP}",
                "bound": None,
                "measured": round(per, 1),
                "unit": "KB",
                "pass": None,
            }
        )
    for s in held:
        s.close()
    time.sleep(1)
    cap = refusals().get("cap", 0)
    report(
        cap >= 1,
        f"metrics count it: cap {cap:.0f}",
        "connection_cap_metric",
        1,
        cap,
        "refusals",
    )


def idle_after_handshake() -> None:
    s = tls_connect()
    t = closed_at(s, time.monotonic(), 90)
    report(
        near(t, 75),
        f"idle after the handshake closed at 75 s ({fmt(t)})",
        "idle_after_handshake",
        75,
        t,
    )


def idle_after_reply() -> None:
    s = tls_connect()
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    s.settimeout(10)
    got = b""
    while b"\r\n\r\n" not in got:
        got += s.recv(65536)
    head, _, body = got.partition(b"\r\n\r\n")
    length = int(
        next(
            line.split(b":")[1]
            for line in head.split(b"\r\n")
            if line.lower().startswith(b"content-length")
        )
    )
    while len(body) < length:
        body += s.recv(65536)
    t = closed_at(s, time.monotonic(), 90)
    report(
        near(t, 75),
        f"idle after a reply closed at 75 s ({fmt(t)})",
        "idle_after_reply",
        75,
        t,
    )


def stalled_body() -> None:
    s = tls_connect()
    s.sendall(
        b"PUT /bounds/1 HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
        b'Content-Length: 1000\r\n\r\n{"a":'
    )
    t = closed_at(s, time.monotonic(), 75)
    report(
        near(t, 60), f"a body stopped for 60 s closed ({fmt(t)})", "stalled_body", 60, t
    )


def server_queues(client_port: int) -> tuple[int, int] | None:
    """The server end's (Recv-Q, Send-Q) while the connection from client_port is open."""
    darwin = sys.platform == "darwin"
    command = ["netstat", "-an", "-p", "tcp"] if darwin else ["ss", "-tnH"]
    out = subprocess.run(command, capture_output=True, text=True, check=False).stdout
    sep = "." if darwin else ":"
    local, peer = f"127.0.0.1{sep}{PORT}", f"127.0.0.1{sep}{client_port}"
    for line in out.splitlines():
        f = line.split()
        if darwin and len(f) >= 6 and f[3:6] == [local, peer, "ESTABLISHED"]:
            return int(f[1]), int(f[2])
        if not darwin and len(f) >= 5 and f[0] == "ESTAB" and f[3:5] == [local, peer]:
            return int(f[1]), int(f[2])
    return None


def unread_bytes(sock: socket.socket) -> int:
    """Bytes the kernel holds for this socket that it has not read."""
    return struct.unpack(
        "i", fcntl.ioctl(sock.fileno(), termios.FIONREAD, b"\0\0\0\0")
    )[0]


def unread_reply() -> None:
    # Pipelines requests and never reads: the server's replies fill the buffers, it stops
    # reading, and this writer blocks. The close shows as a write error. The bound counts
    # from the server's last byte read or written, seen in its socket's queues; the
    # client's last write and last arrival are recorded beside it.
    s = tls_connect()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    s.settimeout(None)
    client_port = s.getsockname()[1]
    request = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" * 16
    wrote, moved, arrived = [time.monotonic()], [0.0], [0.0]
    closed: list[float] = []
    changes: list[tuple[float, int, int]] = []

    def writer() -> None:
        try:
            while True:
                s.sendall(request)
                wrote[0] = time.monotonic()
        except (ssl.SSLError, OSError):
            closed.append(time.monotonic())

    def sampler() -> None:
        queues, held = None, -1
        while not closed:
            now = time.monotonic()
            q = server_queues(client_port)
            if q is not None and q != queues:
                # The server read (Recv-Q fell) or wrote (Send-Q rose); the peer's
                # zero-window probes raise Recv-Q and the kernel's sends lower Send-Q.
                if queues is None or q[0] < queues[0] or q[1] > queues[1]:
                    moved[0] = now
                    changes.append((now, *q))
                queues = q
            with contextlib.suppress(OSError):
                n = unread_bytes(s)
                if n != held:
                    arrived[0], held = now, n
            time.sleep(0.2)

    thread = threading.Thread(target=writer, daemon=True)
    watch = threading.Thread(target=sampler, daemon=True)
    thread.start()
    watch.start()
    thread.join(150)
    if not closed:
        socket.socket.shutdown(
            s, socket.SHUT_RDWR
        )  # the raw socket, so the writer returns
        report(
            False,
            "a client not reading its replies for 60 s closed (never)",
            "unread_reply",
            60,
        )
        return
    watch.join(5)
    extra = {
        "after_client_write": closed[0] - wrote[0],
        "after_last_arrival": closed[0] - arrived[0] if arrived[0] else None,
        "server_changes": [
            [round(closed[0] - at, 2), r, w] for at, r, w in changes[-8:]
        ],
    }
    if not moved[0]:
        report(
            False,
            "a client not reading its replies: the server's queues were never seen",
            "unread_reply",
            60,
            None,
            extra=extra,
        )
        return
    t = closed[0] - moved[0]
    report(
        near(t, 60),
        f"a client not reading its replies for 60 s closed ({fmt(t)} after the server's last read or write; "
        f"{fmt(extra['after_client_write'])} after the client's last write)",
        "unread_reply",
        60,
        t,
        extra=extra,
    )


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
        except (TimeoutError, ssl.SSLWantReadError):
            pass
        except (ssl.SSLError, OSError):
            t = time.monotonic() - start
            break
        time.sleep(2)
    report(
        near(t, 30),
        f"headers trickled past 30 s closed ({fmt(t)})",
        "trickled_headers",
        30,
        t,
    )


def slow_restore() -> None:
    lines = [b'{"_id": %d, "n": %d}\n' % (i, i) for i in range(14)]
    body = b"".join(lines)
    s = tls_connect()
    s.sendall(
        b"RESTORE /bounds_restore/ HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-ndjson\r\n"
        b"Content-Length: %d\r\n\r\n" % len(body)
    )
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
    report(
        took > 120 and " 2" in status[:13],
        f"a RESTORE streamed for {took:.0f} s completes ({status or 'no reply'})",
        "slow_restore",
        120,
        took,
    )


def main() -> int:
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    subprocess.run(
        [os.path.join(HERE, "tls_certs.sh"), os.path.join(WORK, "certs")],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    print(f"== [T3] TLS bounds on :{PORT}", flush=True)

    only = [c for c in os.environ.get("ONLY", "").split(",") if c]

    def wanted(name: str) -> bool:
        return not only or name in only

    try:
        run(wanted)
    finally:
        out = os.environ.get("OUT")
        if out:
            with open(out, "w") as f:
                json.dump(
                    {"port": PORT, "cap": CAP, "slack": SLACK, "rows": rows},
                    f,
                    indent=2,
                )
    print(
        "tls_bounds: OK" if not failures else f"tls_bounds: {len(failures)} failed",
        flush=True,
    )
    return 1 if failures else 0


def fdlimit() -> None:
    node = Node("fdlimit", "--tls-max-connections", "1000", nofile=512)
    try:
        node.proc.wait(30)
    except subprocess.TimeoutExpired:
        node.stop()
    report(
        node.proc.returncode not in (None, 0)
        and node.logged("does not fit the open-files limit"),
        "--tls-max-connections 1000 under 512 open files refused at startup",
        "fd_limit",
        512 - 64,
        1000,
        "connections",
    )


def run(wanted: Callable[[str], bool]) -> None:
    if wanted("fdlimit"):
        fdlimit()
    node = Node("bounds", "--tls-max-connections", str(CAP))
    try:
        if not node.ready():
            report(False, "node did not start", "startup")
            return
        if wanted("handshakes"):
            handshakes()
            time.sleep(6)
        if wanted("connections"):
            connections(node)
            time.sleep(2)
        cases = [
            idle_after_handshake,
            idle_after_reply,
            stalled_body,
            unread_reply,
            trickled_headers,
            slow_restore,
        ]
        threads = [threading.Thread(target=c) for c in cases if wanted(c.__name__)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    finally:
        node.stop()


if __name__ == "__main__":
    sys.exit(main())
