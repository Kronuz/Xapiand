#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""stall_load.py -- Phase 7c K3: does a reactor stall under heavy load, and what does light latency cost?

One node, two kinds of load, measured together:

  light  open loop at a fixed rate on `accounts`: GET by id, single-term searches, and 10% write-then-GET
         pairs (the GET awaits a refresh). Latency runs from each request's scheduled send time, so a
         slow server or a slow generator shows up in the percentiles, and the achieved rate is printed
         and asserted within 5% of the target.
  heavy  4x the reactor count in closed-loop clients: exhaustive searches with aggregations, wildcard
         and phrase searches, and a RESTORE stream into its own index `restore-target`
         (--restore-into accounts writes `accounts` itself, which is Q6's cost).
  held   T8's overload: rounds of F x the in-flight cap in requests (single-term searches, or heavy ones),
         each head sent with its body withheld until the round's 503s show the cap refused the excess; the
         heavy overload runs beside light at --rate, which meets the demoted cap's 503.

`accounts` holds --accounts-docs documents (default 1,000,000), loaded in 20,000-document RESTOREs.
Every phase writes light p50/p99/p99.9, per-loop lag deltas from :metrics, 503 and error counts,
in-flight and demoted peaks, segment count and merge backlog, RSS and phase times as JSON.

  stall_load.py --bin build/bin/xapiand --out run.json                  unloaded then loaded
  stall_load.py --bin BIN --phase loaded --restore-into accounts        T5's same-index run
  stall_load.py --bin BIN --saturate                                    find the saturating light rate
  stall_load.py --bin BIN --overload light 1.25  |  --overload heavy 1.25   T8: hold 1.25x the in-flight cap
  stall_load.py --bin BIN --dry-run                                     20,000 documents, short phases

--bin starts a node in --workdir and removes it (process and files) on exit;
--target HOST:PORT uses a node that is already running. Run it under a hold: quiet strict NAME MIN
"what" -- harness/stall_load.py ...
"""

from __future__ import annotations

import argparse
import asyncio
import atexit
import contextlib
import json
import math
import os
import random
import re
import resource
import shutil
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import Any

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATASET = os.path.join(REPO, "docs", "public", "assets", "accounts.ndjson")
CHUNK_DOCS = 20_000
JSON_CT = "application/json"
NDJSON_CT = "application/x-ndjson"
INFLIGHT_CAP = 1_000  # requests in flight per reactor before a 503 (src/manager.cc, D2)

# Heavy bodies: exhaustive aggregations, wildcard and phrase searches, a text scan.
HEAVY_QUERIES: list[tuple[str, dict[str, Any]]] = [
    (
        "agg_state",
        {
            "_query": {"gender": "male"},
            "_limit": 0,
            "_aggs": {"by_state": {"_values": {"_field": "contact.state"}}},
        },
    ),
    (
        "agg_age",
        {"_query": "*", "_limit": 0, "_aggs": {"ages": {"_terms": {"_field": "age"}}}},
    ),
    ("wildcard", {"_query": {"favoriteFruit": {"_wildcard": "ba"}}, "_limit": 100}),
    (
        "phrase",
        {"_query": {"personality": {"_phrase": "loyal and confident"}}, "_limit": 100},
    ),
    ("text", {"_query": {"personality": "elegant"}, "_limit": 100}),
]


def log(msg: str) -> None:
    print(f"[stall_load {time.strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------------------------
# A minimal keep-alive HTTP/1.1 client


class HttpError(Exception):
    pass


class Conn:
    split_write = False  # P1 H1: write a large body apart from the head (no 20 MB concatenation on the loop)

    def __init__(self, host: str, port: int) -> None:
        self.host, self.port = host, port
        self.reader: asyncio.StreamReader | None = None
        self.writer: asyncio.StreamWriter | None = None
        self.server_s = 0.0  # P1 probe: the last response's Response-Time (server begins->ends), seconds
        self.sent_at = 0.0  # P1 probe: loop time the last request was handed to the socket

    async def close(self) -> None:
        if self.writer is not None:
            self.writer.close()
            with contextlib.suppress(Exception):
                await self.writer.wait_closed()
        self.reader = self.writer = None

    async def request(
        self,
        method: str,
        path: str,
        body: bytes = b"",
        content_type: str = JSON_CT,
        timeout: float = 300.0,
    ) -> tuple[int, bytes]:
        try:
            return await asyncio.wait_for(self._request(method, path, body, content_type), timeout)
        except BaseException:
            await self.close()
            raise

    async def _request(self, method: str, path: str, body: bytes, content_type: str) -> tuple[int, bytes]:
        await self.connect()
        head = self.head(method, path, len(body), content_type)
        if Conn.split_write and len(body) > 65536:  # no 20 MB concatenation on the loop
            await self.send(head, body)
        else:
            await self.send(head + body)
        return await self.response()

    async def connect(self) -> None:
        if self.writer is None or self.reader is None:
            self.reader, self.writer = await asyncio.open_connection(self.host, self.port)

    def head(self, method: str, path: str, length: int, content_type: str = JSON_CT) -> bytes:
        head = f"{method} {path} HTTP/1.1\r\nHost: {self.host}:{self.port}\r\nContent-Length: {length}\r\n"
        if length:
            head += f"Content-Type: {content_type}\r\n"
        return head.encode() + b"\r\n"

    async def send(self, data: bytes, more: bytes = b"") -> None:
        assert self.writer is not None
        self.sent_at = asyncio.get_running_loop().time()
        self.writer.write(data)
        if more:
            self.writer.write(more)
        await self.writer.drain()

    async def response(self) -> tuple[int, bytes]:
        """Read one response; a `Connection: close` or an unframed body closes the connection after it."""
        assert self.reader is not None
        self.server_s = 0.0
        status_line = await self.reader.readline()
        if not status_line:
            raise HttpError("connection closed")
        status = int(status_line.split()[1])
        length = -1
        chunked = False
        close = False
        while True:
            line = await self.reader.readline()
            if line in (b"\r\n", b"\n", b""):
                break
            k, _, v = line.decode("latin-1").partition(":")
            k = k.strip().lower()
            v = v.strip().lower()
            if k == "content-length":
                length = int(v)
            elif k == "transfer-encoding" and "chunked" in v:
                chunked = True
            elif k == "connection" and "close" in v:
                close = True
            elif k == "response-time":
                with contextlib.suppress(ValueError):
                    self.server_s = float(v)
        if chunked:
            parts: list[bytes] = []
            while True:
                size = int((await self.reader.readline()).split(b";")[0].strip() or b"0", 16)
                if size == 0:
                    await self.reader.readline()
                    break
                parts.append(await self.reader.readexactly(size))
                await self.reader.readline()
            data = b"".join(parts)
        elif length >= 0:
            data = await self.reader.readexactly(length)
        else:
            data = await self.reader.read()
            close = True
        if close:
            await self.close()
        return status, data


# ---------------------------------------------------------------------------------------------
# The node under test


def port_in_use(host: str, port: int) -> bool:
    with socket.socket() as s:
        s.settimeout(1)
        return s.connect_ex((host, port)) == 0


class Node:
    """A node this script started (and removes) or one that is already running."""

    def __init__(self, args: argparse.Namespace) -> None:
        self.host, _, port = args.target.partition(":")
        self.port = int(port)
        self.proc: subprocess.Popen[bytes] | None = None
        self.workdir = ""
        if args.bin:
            if port_in_use(self.host, self.port):
                raise SystemExit(f"{self.host}:{self.port} is already in use; pick another --target")
            self.workdir = os.path.abspath(args.workdir)
            shutil.rmtree(self.workdir, ignore_errors=True)
            os.makedirs(os.path.join(self.workdir, "data"))
            env = dict(os.environ)
            logf = open(os.path.join(self.workdir, "node.log"), "wb")
            cmd = [
                os.path.abspath(args.bin),
                "--port",
                str(self.port),
                "--name",
                "stall",
                "-D",
                os.path.join(self.workdir, "data"),
            ]
            if args.processors > 0:
                cmd += ["--processors", str(args.processors)]
            if args.index_size:
                # Its own cgroup (systemd-run --scope execs in place, so the pid is the node's): T6 caps its memory.
                cmd = ["systemd-run", "--user", "--scope", "--quiet", "--collect", "--", *cmd]
            self.proc = subprocess.Popen(cmd, stdout=logf, stderr=logf, env=env, start_new_session=True)
            atexit.register(self.stop)
            for sig in (signal.SIGINT, signal.SIGTERM):
                signal.signal(sig, self._die)
            self.wait_up()

    def _die(self, *_: object) -> None:
        # Stopping the node first: an exception raised from a signal handler can be swallowed by a task.
        self.stop()
        os._exit(130)

    def wait_up(self) -> None:
        import http.client

        for _ in range(240):
            if self.proc is not None and self.proc.poll() is not None:
                raise SystemExit(f"the node exited with {self.proc.returncode}; see {self.workdir}/node.log")
            try:
                c = http.client.HTTPConnection(self.host, self.port, timeout=3)
                c.request("GET", "/")
                c.getresponse().read()
                c.close()
                return
            except OSError:
                time.sleep(0.5)
        raise SystemExit("the node did not come up in 120 s")

    def stop(self) -> None:
        if self.proc is not None and self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
                self.proc.wait(timeout=30)
            except Exception:
                with contextlib.suppress(Exception):
                    os.killpg(self.proc.pid, signal.SIGKILL)
                    self.proc.wait(timeout=10)
        self.proc = None
        if self.workdir:
            shutil.rmtree(self.workdir, ignore_errors=True)
            self.workdir = ""

    @property
    def pid(self) -> int | None:
        return self.proc.pid if self.proc is not None else None

    def rss_kb(self) -> int | None:
        if self.proc is None:
            return None
        out = subprocess.run(
            ["ps", "-o", "rss=", "-p", str(self.proc.pid)], capture_output=True, text=True, check=False
        ).stdout.strip()
        return int(out) if out else None

    def cpu_s(self) -> float | None:
        """The node's user + system CPU seconds so far."""
        if self.proc is None:
            return None
        return process_cpu_s(self.proc.pid)


def process_cpu_s(pid: int) -> float | None:
    """User + system CPU seconds of `pid`: /proc's clock ticks on Linux, `ps -o time` (centiseconds) elsewhere."""
    try:
        with open(f"/proc/{pid}/stat") as f:
            fields = f.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
    except OSError:
        pass
    out = subprocess.run(["ps", "-o", "time=", "-p", str(pid)], capture_output=True, text=True, check=False).stdout
    if not out.strip():
        return None
    secs = 0.0
    for part in out.strip().split(":"):
        secs = secs * 60 + float(part)
    return secs


def thread_major_faults(pid: int | None) -> dict[int, tuple[str, int]] | None:
    """Each thread's (name, major faults) from /proc/PID/task; None off Linux."""
    if pid is None or not os.path.isdir(f"/proc/{pid}/task"):
        return None
    out: dict[int, tuple[str, int]] = {}
    for tid in os.listdir(f"/proc/{pid}/task"):
        with contextlib.suppress(OSError):
            with open(f"/proc/{pid}/task/{tid}/stat") as f:
                text = f.read()
            name = text[text.index("(") + 1 : text.rindex(")")]
            out[int(tid)] = (name, int(text.rsplit(")", 1)[1].split()[9]))
    return out


def faults_delta(
    before: dict[int, tuple[str, int]] | None, after: dict[int, tuple[str, int]] | None, queries: int
) -> dict[str, Any] | None:
    """Major faults per thread name over a phase, and per 1,000 queries (T6)."""
    if before is None or after is None:
        return None
    by_name: dict[str, int] = {}
    for tid, (name, n) in after.items():
        by_name[name] = by_name.get(name, 0) + n - before.get(tid, (name, 0))[1]
    total = sum(by_name.values())
    return {
        "by_thread": dict(sorted(by_name.items())),
        "total": total,
        "queries": queries,
        "per_1000_queries": round(total * 1000 / queries, 3) if queries else None,
    }


def cap_memory(node: Node, factor: float) -> dict[str, Any]:
    """T6's "index at FACTOR x RAM": the node's cgroup gets its anonymous memory plus 1/FACTOR of the index in page cache."""
    pid = node.pid
    if pid is None or not os.path.isdir("/proc"):
        raise SystemExit("--index-size needs a node this script started, on Linux")
    with open(f"/proc/{pid}/cgroup") as f:
        cg = "/sys/fs/cgroup" + f.read().strip().split("::", 1)[1]
    index_bytes = sum(
        os.path.getsize(os.path.join(d, n)) for d, _, ns in os.walk(os.path.join(node.workdir, "data")) for n in ns
    )

    def stat() -> dict[str, int]:
        with open(os.path.join(cg, "memory.stat")) as f:
            return {k: int(v) for k, v in (line.split() for line in f)}

    anon = stat()["anon"]
    limit = anon + int(index_bytes / factor)
    with open(os.path.join(cg, "memory.max"), "w") as f:
        f.write(str(limit))
    after = stat()
    return {
        "cgroup": cg,
        "index_bytes": index_bytes,
        "anon_bytes": anon,
        "memory_max": limit,
        "file_bytes_after_cap": after["file"],
    }


# ---------------------------------------------------------------------------------------------
# The dataset


@dataclass
class Rows:
    lines: list[bytes]
    states: list[str]


def load_rows() -> Rows:
    with open(DATASET, "rb") as fh:
        lines = [ln for ln in fh.read().split(b"\n") if ln.strip()]
    states = sorted({json.loads(ln)["contact"]["state"] for ln in lines})
    return Rows(lines, states)


def restore_body(rows: Rows, first_id: int, count: int) -> bytes:
    """`count` accounts with explicit ids first_id.. (`_id` last in the object, where RESTORE reads it)."""
    n = len(rows.lines)
    return (
        b"\n".join(rows.lines[(first_id + i) % n][:-1] + b',"_id":%d}' % (first_id + i) for i in range(count)) + b"\n"
    )


async def load_accounts(host: str, port: int, rows: Rows, total: int, index: str = "accounts") -> float:
    t0 = time.perf_counter()
    conn = Conn(host, port)
    done = 0
    while done < total:
        count = min(CHUNK_DOCS, total - done)
        status, data = await conn.request(
            "RESTORE",
            f"/{index}/?commit",
            restore_body(rows, done + 1, count),
            NDJSON_CT,
        )
        if status >= 300:
            raise SystemExit(f"loading {index}: HTTP {status}: {data[:200]!r}")
        done += count
        if done % (CHUNK_DOCS * 10) == 0 or done == total:
            log(f"loaded {done}/{total} documents into {index} ({time.perf_counter() - t0:.0f} s)")
    await conn.close()
    return time.perf_counter() - t0


# ---------------------------------------------------------------------------------------------
# Metrics


_SAMPLE = re.compile(r"^([a-z_]+)(?:\{([^}]*)\})?\s+(\S+)$")


def parse_metrics(text: str) -> dict[str, dict[str, Any]]:
    """Prometheus text -> {"lag": {loop: {"edges": [(le_us, cumulative)], "count", "sum_us", "max_us"}}, "gauges": {}}."""
    lag: dict[str, dict[str, Any]] = {}
    gauges: dict[str, float] = {}
    for line in text.splitlines():
        if line.startswith("#"):
            continue
        m = _SAMPLE.match(line)
        if not m:
            continue
        name, labels, value = m.group(1), m.group(2) or "", m.group(3)
        lab = dict(re.findall(r'(\w+)="([^"]*)"', labels))
        if name.startswith("xapiand_reactor_lag") and "loop" in lab:
            loop = lag.setdefault(lab["loop"], {"edges": [], "count": 0, "sum_us": 0.0, "max_us": 0.0})
            if name == "xapiand_reactor_lag_seconds_bucket":
                le = lab["le"]
                edge = math.inf if le == "+Inf" else float(le) * 1e6
                loop["edges"].append((edge, int(float(value))))
            elif name == "xapiand_reactor_lag_seconds_count":
                loop["count"] = int(float(value))
            elif name == "xapiand_reactor_lag_seconds_sum":
                loop["sum_us"] = float(value) * 1e6
            elif name == "xapiand_reactor_lag_max_seconds":
                loop["max_us"] = float(value) * 1e6
        else:
            gauges[name] = float(value) if value not in ("NaN", "+Inf", "-Inf") else math.nan
    return {"lag": lag, "gauges": gauges}


def lag_delta(before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    """Per-loop lag between two scrapes: wakes, p50, p99, p99.9 (bucket upper edges, us) and the max so far."""
    out: dict[str, Any] = {}
    for loop, post in after["lag"].items():
        pre = before["lag"].get(loop, {"edges": [], "count": 0})
        pre_at = {e: c for e, c in pre["edges"]}
        diffs: list[tuple[float, int]] = []
        prev_post = prev_pre = 0
        for edge, cum in post["edges"]:
            pre_cum = pre_at.get(edge, prev_pre)
            diffs.append((edge, (cum - prev_post) - (pre_cum - prev_pre)))
            prev_post, prev_pre = cum, pre_cum
        total = sum(c for _, c in diffs)

        def q(frac: float, d: list[tuple[float, int]] = diffs, t: int = total) -> float | None:
            if t == 0:
                return None
            need = max(1, math.ceil(frac * t))
            seen = 0
            for edge, c in d:
                seen += c
                if seen >= need:
                    return edge
            return math.inf

        out[loop] = {
            "wakes": total,
            "p50_us": q(0.50),
            "p99_us": q(0.99),
            "p999_us": q(0.999),
            "max_us_so_far": post["max_us"],
            "p99_within_1ms": (q(0.99) or 0) <= 1024,
        }
    return out


async def scrape(conn: Conn) -> dict[str, Any]:
    status, data = await conn.request("GET", "/:metrics")
    if status != 200:
        return {"lag": {}, "gauges": {}}
    return parse_metrics(data.decode("utf-8", "replace"))


def segment_files(datadir: str, index: str) -> int | None:
    """Live segment files (seg-*.sx) across the index's shards, counted on disk; None for a node we did not start."""
    root = os.path.join(datadir, index)
    if not datadir or not os.path.isdir(root):
        return None
    return sum(1 for _d, _n, files in os.walk(root) for f in files if f.startswith("seg-") and f.endswith(".sx"))


async def settle_merges(node: Node, index: str, quiet_s: float = 5.0, limit_s: float = 240.0) -> float:
    """Seconds spent waiting, after a load, for the index's segment count to hold still (merges done)."""
    datadir = os.path.join(node.workdir, "data") if node.workdir else ""
    start = time.monotonic()
    last = segment_files(datadir, index)
    if last is None:
        return 0.0
    still_since = time.monotonic()
    while time.monotonic() - start < limit_s and time.monotonic() - still_since < quiet_s:
        await asyncio.sleep(0.5)
        now = segment_files(datadir, index)
        if now != last:
            last, still_since = now, time.monotonic()
    log(f"{index}: {last} segments after {time.monotonic() - start:.0f} s of settling")
    return time.monotonic() - start


async def index_stats(node: Node, index: str, settle: bool) -> dict[str, Any]:
    """Segment count at the end of a phase and the merge backlog: the segments merging removes once the load stops.

    The node reports neither, so the count is read from the data directory; with `settle` the count is watched
    until it holds still for 2 s (at most 15 s) and the backlog is what the wait removed.
    """
    datadir = os.path.join(node.workdir, "data") if node.workdir else ""
    ended = segment_files(datadir, index)
    if ended is None:
        return {"segments": None, "merge_backlog": None, "settled_segments": None}
    settled = ended
    if settle:
        deadline = time.monotonic() + 15
        still_since = time.monotonic()
        while time.monotonic() < deadline and time.monotonic() - still_since < 2:
            await asyncio.sleep(0.25)
            now = segment_files(datadir, index)
            if now is not None and now != settled:
                settled, still_since = now, time.monotonic()
    return {"segments": ended, "merge_backlog": ended - settled, "settled_segments": settled}


# ---------------------------------------------------------------------------------------------
# Light load


def pct(values: list[int], p: float) -> float | None:
    if not values:
        return None
    s = sorted(values)
    return float(s[min(len(s) - 1, max(0, math.ceil(round(p / 100.0 * len(s), 9)) - 1))])


def summarize(values: list[int]) -> dict[str, Any]:
    return {
        "n": len(values),
        "p50_us": pct(values, 50),
        "p99_us": pct(values, 99),
        "p999_us": pct(values, 99.9),
        "max_us": float(max(values)) if values else None,
    }


def refusal_reason(body: bytes) -> str:
    """A 503's cause: a JSON error's message (the engine's reason), else the plain-text body."""
    text = body.decode("utf-8", "replace").strip()
    try:
        message = json.loads(text).get("message", text)
    except (ValueError, AttributeError):
        message = text
    if isinstance(message, list):
        message = "; ".join(str(m) for m in message)
    return str(message)[:80]


@dataclass
class Counts:
    ok: int = 0
    s503: int = 0
    other: int = 0
    exceptions: int = 0
    first_errors: list[str] = field(default_factory=list)
    reasons: dict[str, int] = field(default_factory=dict)  # 503s by body: which admission cap refused them

    def add(self, status: int, what: str = "", body: bytes = b"") -> None:
        if status < 300 or status == 404 and what == "get":
            self.ok += 1
        elif status == 503:
            self.s503 += 1
            reason = refusal_reason(body)
            self.reasons[reason] = self.reasons.get(reason, 0) + 1
        else:
            self.other += 1
            if len(self.first_errors) < 5:
                self.first_errors.append(f"{what}: HTTP {status}")

    def exc(self, e: BaseException, what: str) -> None:
        self.exceptions += 1
        if len(self.first_errors) < 5:
            self.first_errors.append(f"{what}: {e!r}")

    def as_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "http_503": self.s503,
            "http_503_by_reason": self.reasons,
            "http_other": self.other,
            "exceptions": self.exceptions,
            "first": self.first_errors,
        }


@dataclass
class LightResult:
    classes: dict[str, list[int]]
    counts: Counts
    issued: int
    completed: int
    target_rate: float
    achieved_rate: float
    window_s: float
    unfinished: int
    probe: dict[str, Any] = field(default_factory=dict)

    def as_dict(self) -> dict[str, Any]:
        everything = [v for vs in self.classes.values() for v in vs]
        query = self.classes["get"] + self.classes["search"]
        return {
            "target_rate": self.target_rate,
            "achieved_rate": round(self.achieved_rate, 2),
            "rate_error": round(abs(self.achieved_rate - self.target_rate) / self.target_rate, 4)
            if self.target_rate
            else None,
            "issued": self.issued,
            "completed": self.completed,
            "unfinished_at_drain_cutoff": self.unfinished,
            "completed_rate": round(self.completed / self.window_s, 2),
            "window_s": round(self.window_s, 3),
            "all": summarize(everything),
            "query": summarize(query),
            "get": summarize(self.classes["get"]),
            "search": summarize(self.classes["search"]),
            "pair": summarize(self.classes["pair"]),
            "errors": self.counts.as_dict(),
            "probe": self.probe,
        }


async def run_light(
    host: str,
    port: int,
    rows: Rows,
    accounts: int,
    rate: float,
    duration: float,
    conns: int,
    seed: int,
    warmup: float = 1.0,
    drain: float = 10.0,
) -> LightResult:
    loop = asyncio.get_running_loop()
    rng = random.Random(seed)
    pool: asyncio.Queue[Conn] = asyncio.Queue()
    for _ in range(conns):
        pool.put_nowait(Conn(host, port))
    classes: dict[str, list[int]] = {"get": [], "search": [], "pair": []}
    counts = Counts()
    pair_base = accounts + 10_000_000 + seed * 1_000_000
    body_row = rows.lines[0]
    total = max(1, int(rate * duration))
    t0 = loop.time() + 0.3
    last_done = [t0]
    completed = [0]
    inflight: dict[int, tuple[float, str]] = {}
    probe_rows: list[tuple[str, float, float, float, float]] = []  # P1: kind, sched, first send, end, server s
    gen_lag: list[tuple[float, float]] = []  # P1: (loop time, lateness s) of a 1 ms sleep in the generator
    probing = [True]

    async def lag_probe() -> None:
        while probing[0]:
            t = loop.time()
            await asyncio.sleep(0.001)
            gen_lag.append((t, loop.time() - t - 0.001))

    probe_task = loop.create_task(lag_probe())

    async def unit(i: int, sched: float, kind: int, doc_id: int, state: str) -> None:
        inflight[i] = (sched, ("get", "search", "pair")[kind])
        conn = await pool.get()
        try:
            server = 0.0
            if kind == 0:
                what = "get"
                status, body = await conn.request("GET", f"/accounts/{doc_id}")
                server = conn.server_s
            elif kind == 1:
                what = "search"
                status, body = await conn.request(
                    "SEARCH",
                    "/accounts/",
                    json.dumps({"_query": {"contact.state": state}, "_limit": 10}).encode(),
                )
                server = conn.server_s
            else:
                what = "pair"
                pid = pair_base + i
                status, body = await conn.request("PUT", f"/accounts/{pid}", body_row)
                server = conn.server_s
                first_send = conn.sent_at
                if status < 300:
                    status, body = await conn.request("GET", f"/accounts/{pid}")
                    server += conn.server_s
            end = loop.time()
            if kind != 2:
                first_send = conn.sent_at
            if sched - t0 >= warmup:
                probe_rows.append((what, sched, first_send, end, server))
            counts.add(status, what, body)
            if sched - t0 >= warmup:
                classes[what].append(int((end - sched) * 1e6))
            completed[0] += 1
            last_done[0] = end
            inflight.pop(i, None)
        except Exception as e:
            counts.exc(e, "light")
            conn = Conn(host, port)
            inflight.pop(i, None)
        finally:
            pool.put_nowait(conn)

    tasks: list[asyncio.Task[None]] = []
    for i in range(total):
        sched = t0 + i / rate
        r = rng.random()
        kind = 2 if r < 0.10 else (0 if r < 0.55 else 1)
        delay = sched - loop.time()
        if delay > 0.0005:
            await asyncio.sleep(delay)
        elif i % 64 == 0:
            await asyncio.sleep(0)
        tasks.append(loop.create_task(unit(i, sched, kind, rng.randint(1, accounts), rng.choice(rows.states))))
    issue_span = max(1e-9, loop.time() - t0)
    # A saturated server leaves a backlog; wait `drain` seconds for it, then count what is left at its age.
    _done, pending = await asyncio.wait(tasks, timeout=drain)
    cutoff = loop.time()
    for task in pending:
        task.cancel()
    await asyncio.gather(*pending, return_exceptions=True)
    for sched, what in inflight.values():
        if sched - t0 >= warmup:
            classes[what].append(int((cutoff - sched) * 1e6))
    unfinished = len(inflight)
    probing[0] = False
    await probe_task
    window = max(1e-9, last_done[0] - t0)
    for _ in range(pool.qsize()):
        await pool.get_nowait().close()
    # The window is the schedule's span: the last request is scheduled at (total - 1) / rate.
    # Achieved is the rate the generator really sent at: units issued over the span it took to issue them. A
    # generator that falls behind its schedule misses the target; a slow server shows in the latencies instead.
    achieved = max(0, total - 1) / issue_span if total > 1 else rate
    res = LightResult(classes, counts, total, completed[0], rate, achieved, window, unfinished)
    res.probe = probe_split(probe_rows, gen_lag, t0)
    return res


def probe_split(
    rows: list[tuple[str, float, float, float, float]], gen_lag: list[tuple[float, float]], t0: float
) -> dict[str, Any]:
    """P1: per light class, client latency split into generator wait (sched->send), server (Response-Time) and the
    rest (sockets, the server loop before parse and after `ends`, the generator reading the response)."""
    out: dict[str, Any] = {}
    stalls = [(t, d) for t, d in gen_lag if d > 0.005]
    for kind in ("get", "search", "pair", "all"):
        rs = [r for r in rows if kind == "all" or r[0] == kind]
        if not rs:
            continue
        lat = [int((e - s) * 1e6) for _, s, _, e, _ in rs]
        cut = pct(lat, 99) or 0
        tail = [r for r, x in zip(rs, lat) if x >= cut]

        def parts(sel: list[tuple[str, float, float, float, float]]) -> dict[str, Any]:
            gw = [int((sd - s) * 1e6) for _, s, sd, _, _ in sel]
            sv = [int(sv * 1e6) for *_, sv in sel]
            rest = [int((e - sd - sv) * 1e6) for _, _, sd, e, sv in sel]
            return {"gen_wait": summarize(gw), "server": summarize(sv), "rest": summarize(rest)}

        # A tail request overlaps a generator stall when a >5 ms stall began inside its [sched - 50 ms, end].
        hit = sum(1 for _, s, _, e, _ in tail if any(s - 0.05 <= t <= e for t, _ in stalls))
        out[kind] = {
            "n": len(rs),
            "client": summarize(lat),
            "parts": parts(rs),
            "tail_n": len(tail),
            "tail_parts": parts(tail),
            "tail_with_gen_stall": hit,
        }
    out["t0"] = t0
    lags = [int(d * 1e6) for _, d in gen_lag]
    out["generator_lag"] = {
        **summarize(lags),
        "stalls_over_5ms": len(stalls),
        "stalls_over_20ms": sum(1 for _, d in stalls if d > 0.02),
        "stalled_s": round(sum(d for _, d in stalls), 3),
        "stall_times_s": [[round(t - t0, 2), round(d * 1e3, 1)] for t, d in stalls if d > 0.02][:60],
    }
    return out


# ---------------------------------------------------------------------------------------------
# Heavy load


@dataclass
class HeavyResult:
    searches: int
    restored_docs: int
    restore_requests: int
    counts: Counts
    by_kind: dict[str, int]
    latencies: dict[str, list[int]]
    restore_log: list[tuple[float, float, float, float]] = field(default_factory=list)

    def as_dict(self) -> dict[str, Any]:
        return {
            "restore_log": self.restore_log,
            "searches": self.searches,
            "restored_docs": self.restored_docs,
            "restore_requests": self.restore_requests,
            "by_kind": self.by_kind,
            "latency": {k: summarize(v) for k, v in self.latencies.items()},
            "errors": self.counts.as_dict(),
        }


async def run_heavy(
    host: str,
    port: int,
    rows: Rows,
    clients: int,
    duration: float,
    restore_index: str,
    restore_first_id: int,
    prebuilt: dict[int, bytes] | None = None,
) -> HeavyResult:
    """Closed-loop heavy clients; one is the RESTORE stream."""
    loop = asyncio.get_running_loop()
    end_at = loop.time() + duration
    counts = Counts()
    by_kind: dict[str, int] = {}
    lat: dict[str, list[int]] = {}
    restored = [0, 0]
    restore_log: list[tuple[float, float, float, float]] = []  # P1: (start, body build s, round trip s, server s)

    async def search_client(n: int) -> None:
        conn = Conn(host, port)
        i = n
        while loop.time() < end_at:
            kind, q = HEAVY_QUERIES[i % len(HEAVY_QUERIES)]
            i += 1
            t = loop.time()
            try:
                status, body = await conn.request("SEARCH", "/accounts/", json.dumps(q).encode())
                counts.add(status, kind, body)
                by_kind[kind] = by_kind.get(kind, 0) + 1
                lat.setdefault(kind, []).append(int((loop.time() - t) * 1e6))
            except Exception as e:
                counts.exc(e, kind)
                conn = Conn(host, port)
        await conn.close()

    async def restore_client() -> None:
        conn = Conn(host, port)
        next_id = restore_first_id
        while loop.time() < end_at:
            try:
                tb = loop.time()
                chunk = (
                    prebuilt.pop(next_id)
                    if prebuilt and next_id in prebuilt
                    else restore_body(rows, next_id, CHUNK_DOCS)
                )
                tr = loop.time()
                status, body = await conn.request("RESTORE", f"/{restore_index}/", chunk, NDJSON_CT)
                restore_log.append(
                    (round(tb, 3), round(tr - tb, 4), round(loop.time() - tr, 3), round(conn.server_s, 3))
                )
                counts.add(status, "restore", body)
                if status < 300:
                    restored[0] += CHUNK_DOCS
                    restored[1] += 1
                next_id += CHUNK_DOCS
            except Exception as e:
                counts.exc(e, "restore")
                conn = Conn(host, port)
        await conn.close()

    jobs = [search_client(n) for n in range(max(1, clients - 1))]
    jobs.append(restore_client())
    await asyncio.gather(*jobs)
    res = HeavyResult(sum(by_kind.values()), restored[0], restored[1], counts, by_kind, lat)
    res.restore_log = restore_log
    return res


# ---------------------------------------------------------------------------------------------
# Overload: more requests in flight than the admission cap


@dataclass
class HeldResult:
    held: int
    cap: int
    rounds: list[dict[str, Any]]
    counts: Counts
    answered_us: list[int]
    refused_us: list[int]
    outstanding_peak: int

    @property
    def enforced(self) -> bool:
        return bool(self.rounds) and all(r["enforced"] for r in self.rounds)

    def as_dict(self) -> dict[str, Any]:
        return {
            "held": self.held,
            "cap": self.cap,
            "enforced": self.enforced,
            "outstanding_peak": self.outstanding_peak,
            "in_flight_at_barrier_peak": max((r["in_flight_at_barrier"] for r in self.rounds), default=0),
            "rounds": self.rounds,
            "answered": summarize(self.answered_us),
            "refused_at_barrier": summarize(self.refused_us),
            "errors": self.counts.as_dict(),
        }


async def run_held(
    host: str,
    port: int,
    requests: list[tuple[str, str, str, bytes]],
    held: int,
    cap: int,
    duration: float,
    settle: float = 0.25,
    barrier_timeout: float = 30.0,
) -> HeldResult:
    """Rounds of `held` requests, each one's head sent and its body withheld until the round's barrier opens.

    The server admits a request at headers-complete and holds it until its response is settled, so while the
    bodies wait at most `cap` requests are admitted and the other `held - cap` must be refused with a 503: the cap
    is exceeded by construction, on any host, whatever the server's or this client's speed. The barrier opens once
    that many 503s came and no more for `settle` s, or after `barrier_timeout` s (the round is then not enforced).
    `requests` are (what, method, path, body) cycled over the connections; rounds repeat for `duration` s.
    """
    loop = asyncio.get_running_loop()
    conns = [Conn(host, port) for _ in range(held)]
    opener = asyncio.Semaphore(64)  # a listen backlog is commonly 128
    counts = Counts()
    rounds: list[dict[str, Any]] = []
    answered: list[int] = []
    refused_us: list[int] = []
    outstanding = [0, 0]  # heads sent and not answered: now, peak
    end_at = loop.time() + duration

    async def one(i: int, n: int, release: asyncio.Event, tally: dict[str, Any]) -> None:
        conn = conns[i]
        what, method, path, body = requests[(n * held + i) % len(requests)]
        try:
            async with opener:
                await conn.connect()
            await conn.send(conn.head(method, path, len(body)))
        except Exception as e:
            counts.exc(e, what)
            await conn.close()
            tally["sent"] += 1
            return
        t_sent = loop.time()
        outstanding[0] += 1
        outstanding[1] = max(outstanding[1], outstanding[0])
        tally["sent"] += 1
        reply = asyncio.ensure_future(conn.response())
        gate = asyncio.ensure_future(release.wait())
        try:
            await asyncio.wait({reply, gate}, return_when=asyncio.FIRST_COMPLETED)
            early = reply.done()
            if not early:
                await conn.send(body)
            status, data = await asyncio.wait_for(reply, 300.0)
            counts.add(status, what, data)
            if early:
                tally["early"] += 1
                if status == 503:
                    tally["refused"] += 1
                    tally["last"] = loop.time()
                    refused_us.append(int((loop.time() - t_sent) * 1e6))
            else:
                answered.append(int((loop.time() - tally["released"]) * 1e6))
        except Exception as e:
            counts.exc(e, what)
            await conn.close()
        finally:
            gate.cancel()
            reply.cancel()
            outstanding[0] -= 1

    n = 0
    while not rounds or loop.time() < end_at:
        t0 = loop.time()
        release = asyncio.Event()
        tally: dict[str, Any] = {"sent": 0, "early": 0, "refused": 0, "last": t0, "released": 0.0}
        tasks = [loop.create_task(one(i, n, release, tally)) for i in range(held)]
        deadline = t0 + barrier_timeout
        while loop.time() < deadline and (
            tally["sent"] < held or tally["refused"] < held - cap or loop.time() - tally["last"] < settle
        ):
            await asyncio.sleep(0.02)
        at_barrier = outstanding[0]
        tally["released"] = loop.time()
        release.set()
        await asyncio.gather(*tasks)
        rounds.append(
            {
                "round": n,
                "barrier_s": round(tally["released"] - t0, 3),
                "seconds": round(loop.time() - t0, 3),
                "in_flight_at_barrier": at_barrier,
                "refused_at_barrier": tally["refused"],
                "other_early_answers": tally["early"] - tally["refused"],
                # At most `cap` held in flight and the rest refused, so the 503s come from exceeding the cap.
                "enforced": tally["refused"] >= held - cap and at_barrier <= cap,
            }
        )
        n += 1
    for conn in conns:
        await conn.close()
    return HeldResult(held, cap, rounds, counts, answered, refused_us, outstanding[1])


def held_requests(kind: str, rows: Rows) -> list[tuple[str, str, str, bytes]]:
    """T8's held requests: single-term searches for the light overload, K3's heavy searches for the heavy one."""
    if kind == "light":
        return [
            ("search", "SEARCH", "/accounts/", json.dumps({"_query": {"contact.state": st}, "_limit": 10}).encode())
            for st in rows.states
        ]
    return [(k, "SEARCH", "/accounts/", json.dumps(q).encode()) for k, q in HEAVY_QUERIES]


# ---------------------------------------------------------------------------------------------
# Phases


@dataclass
class Ctx:
    args: argparse.Namespace
    node: Node
    rows: Rows
    accounts: int
    reactors: int
    ctl: Conn
    results: list[dict[str, Any]] = field(default_factory=list)
    args_rate_note: str = ""
    assertions: list[dict[str, Any]] = field(default_factory=list)

    @property
    def restore_index(self) -> str:
        return "accounts" if self.args.restore_into == "accounts" else "restore-target"

    @property
    def restore_first_id(self) -> int:
        return self.accounts + 1 if self.restore_index == "accounts" else 1


def census(node: Node) -> dict[str, Any] | None:
    if node.pid is None:
        return None
    script = os.path.join(REPO, "harness", "thread_census.sh")
    out = subprocess.run([script, str(node.pid), "--json"], capture_output=True, text=True, check=False)
    try:
        return json.loads(out.stdout)
    except ValueError:
        return {"error": out.stderr.strip()[:200]}


async def phase(
    ctx: Ctx,
    name: str,
    light_rate: float,
    heavy: bool,
    duration: float,
    assert_rate: bool = True,
    settle: bool = True,
    held: str = "",
) -> dict[str, Any]:
    """One phase: light at `light_rate` (0: none), heavy clients if `heavy`, a held overload of kind `held`."""
    a = ctx.args
    # P1 H1: the RESTORE bodies built before the clock starts, so the generator's loop never builds one mid-phase.
    prebuilt = (
        {
            ctx.restore_first_id + k * CHUNK_DOCS: restore_body(
                ctx.rows, ctx.restore_first_id + k * CHUNK_DOCS, CHUNK_DOCS
            )
            for k in range(a.restore_prebuilt)
        }
        if heavy and a.restore_prebuilt > 0
        else None
    )
    before = await scrape(ctx.ctl)
    cpu_before = ctx.node.cpu_s()
    faults_before = thread_major_faults(ctx.node.pid)
    t_start = time.time()
    heavy_clients = 4 * ctx.reactors if a.heavy_clients <= 0 else a.heavy_clients
    jobs: list[asyncio.Future[Any]] = []
    held_task = None
    if held:
        cap = INFLIGHT_CAP * ctx.reactors
        count = math.ceil(a.overload_factor * cap)
        log(
            f"{name}: {count} {held} requests held against an in-flight cap of {INFLIGHT_CAP} x {ctx.reactors} reactors"
        )
        held_task = asyncio.ensure_future(
            run_held(ctx.node.host, ctx.node.port, held_requests(held, ctx.rows), count, cap, duration)
        )
        jobs.append(held_task)
    light_task = None
    if light_rate > 0:
        light_task = asyncio.ensure_future(
            run_light(
                ctx.node.host,
                ctx.node.port,
                ctx.rows,
                ctx.accounts,
                light_rate,
                duration,
                a.light_conns,
                a.seed,
            )
        )
        jobs.append(light_task)
    heavy_task = None
    if heavy:
        heavy_task = asyncio.ensure_future(
            run_heavy(
                ctx.node.host,
                ctx.node.port,
                ctx.rows,
                heavy_clients,
                duration + 0.3,
                ctx.restore_index,
                ctx.restore_first_id,
                prebuilt=prebuilt,
            )
        )
        jobs.append(heavy_task)
    census_result: dict[str, Any] | None = None
    if a.census and heavy:
        await asyncio.sleep(duration / 2)
        census_result = await asyncio.get_running_loop().run_in_executor(None, census, ctx.node)
    await asyncio.gather(*jobs)
    cpu_after = ctx.node.cpu_s()
    faults_after = thread_major_faults(ctx.node.pid)
    after = await scrape(ctx.ctl)
    entry: dict[str, Any] = {
        "name": name,
        "started": t_start,
        "seconds": round(time.time() - t_start, 3),
        "duration_asked": duration,
        "heavy_clients": heavy_clients if heavy else 0,
        "restore_index": ctx.restore_index if heavy else None,
        "lag": lag_delta(before, after) if before["lag"] and after["lag"] else None,
        "peaks": {
            "inflight": after["gauges"].get("xapiand_admission_inflight_peak"),
            "demoted": after["gauges"].get("xapiand_scheduler_demoted_peak"),
        },
        "rss_kb": ctx.node.rss_kb(),
        "cpu_s": round(cpu_after - cpu_before, 3) if cpu_before is not None and cpu_after is not None else None,
        "index": {
            "accounts": await index_stats(ctx.node, "accounts", settle),
            **(
                {"restore-target": await index_stats(ctx.node, "restore-target", settle)}
                if heavy and ctx.restore_index != "accounts"
                else {}
            ),
        },
        "census": census_result,
    }
    if light_task is not None:
        res = light_task.result()
        entry["light"] = res.as_dict()
        if assert_rate:
            err = entry["light"]["rate_error"]
            ok = err is not None and err <= 0.05
            ctx.assertions.append(
                {
                    "phase": name,
                    "what": "light achieved rate within 5% of target",
                    "ok": ok,
                    "target": light_rate,
                    "achieved": entry["light"]["achieved_rate"],
                }
            )
            log(
                f"{name}: light target {light_rate:.0f}/s achieved {entry['light']['achieved_rate']:.1f}/s ({'ok' if ok else 'OUTSIDE 5%'})"
            )
        q = entry["light"]["query"]
        log(
            f"{name}: light query p50 {q['p50_us']} us, p99 {q['p99_us']} us, p99.9 {q['p999_us']} us over {q['n']}; errors {entry['light']['errors']['http_503']} 503, {entry['light']['errors']['http_other']} other, {entry['light']['errors']['exceptions']} exceptions"
        )
        if entry["cpu_s"] is not None and res.issued and not heavy and not held:
            entry["cpu_us_per_light"] = round(entry["cpu_s"] * 1e6 / res.issued, 1)
            log(f"{name}: node CPU {entry['cpu_s']} s, {entry['cpu_us_per_light']} us per light request")
    if held_task is not None:
        res_h = held_task.result()
        entry["held"] = res_h.as_dict()
        ctx.assertions.append(
            {
                "phase": name,
                "what": "every round held at most the in-flight cap and the rest were refused with 503",
                "ok": res_h.enforced,
                "held": res_h.held,
                "cap": res_h.cap,
            }
        )
        rs = res_h.rounds
        e = entry["held"]["errors"]
        log(
            f"{name}: {len(rs)} rounds of {res_h.held} held, cap {res_h.cap}: in flight at the barrier "
            f"{[r['in_flight_at_barrier'] for r in rs]}, refused {[r['refused_at_barrier'] for r in rs]} "
            f"(peak outstanding {res_h.outstanding_peak}; {'enforced' if res_h.enforced else 'NOT ENFORCED'}); "
            f"answered p50 {entry['held']['answered']['p50_us']} us; 503s {e['http_503_by_reason']}, "
            f"{e['http_other']} other, {e['exceptions']} exceptions"
        )
    if heavy_task is not None:
        entry["heavy"] = heavy_task.result().as_dict()
        h = entry["heavy"]
        log(
            f"{name}: heavy {h['searches']} searches, {h['restored_docs']} restored docs, errors {h['errors']['http_503']} 503, {h['errors']['http_other']} other"
        )
    queries = (entry.get("light") or {}).get("completed", 0) + (entry.get("heavy") or {}).get("searches", 0)
    entry["major_faults"] = faults_delta(faults_before, faults_after, queries)
    if entry["major_faults"]:
        mf = entry["major_faults"]
        log(f"{name}: major faults {mf['total']} over {queries} queries ({mf['per_1000_queries']} per 1,000)")
    if entry["lag"]:
        worst = max((v["p99_us"] or 0, k) for k, v in entry["lag"].items())
        log(f"{name}: worst loop p99 lag {worst[0]} us ({worst[1]})")
    ctx.results.append(entry)
    return entry


async def saturate(ctx: Ctx) -> dict[str, Any]:
    """The saturating light rate: the highest rate whose query p99 stays within 10x the unloaded p99.

    The unloaded p99 is the lower of the p99s at --saturate-start and twice it (one slow step is not a baseline).
    The rate doubles from there until a step is bad (p99 over the limit, the rate missed, a 503, or a backlog left
    at the drain cutoff), then bisects --bisect-steps times between the last good rate and the first bad one.
    """
    a = ctx.args
    trace: list[dict[str, Any]] = []
    refs: list[float] = []
    for rate in (a.saturate_start, a.saturate_start * 2):
        e = await phase(ctx, f"saturate-{rate:.0f}", rate, False, a.step_secs, settle=False)
        p99 = e["light"]["query"]["p99_us"] or 1.0
        refs.append(p99)
        trace.append({"rate": rate, "p99_us": p99, "good": True})
    p99_0 = max(1000.0, min(refs))
    limit = 10.0 * p99_0

    async def good(rate: float) -> bool:
        e = await phase(ctx, f"saturate-{rate:.0f}", rate, False, a.step_secs, assert_rate=False, settle=False)
        q = e["light"]["query"]
        ok = (
            (q["p99_us"] or math.inf) <= limit
            and e["light"]["rate_error"] is not None
            and e["light"]["rate_error"] <= 0.05
            and not e["light"]["errors"]["http_503"]
            and e["light"]["unfinished_at_drain_cutoff"] == 0
        )
        trace.append({"rate": rate, "p99_us": q["p99_us"], "good": ok})
        return ok

    lo = a.saturate_start * 2
    hi = lo * 2
    while hi <= a.saturate_max and await good(hi):
        lo, hi = hi, hi * 2
    for _ in range(a.bisect_steps):
        mid = (lo + hi) / 2
        if await good(mid):
            lo = mid
        else:
            hi = mid
    result = {
        "unloaded_p99_us": p99_0,
        "limit_p99_us": limit,
        "saturating_rate": lo,
        "first_bad_rate": hi,
        "trace": trace,
    }
    log(f"saturating light rate {lo:.0f}/s (p99 limit {limit:.0f} us from unloaded {p99_0:.0f} us)")
    return result


async def amain(args: argparse.Namespace) -> int:
    node = Node(args)
    rows = load_rows()
    ctl = Conn(node.host, node.port)
    t_boot = time.time()
    if args.dry_run:
        args.accounts_docs = 20_000
        args.duration = min(args.duration, 6.0)
        args.rate = min(args.rate, 200.0)
        args.step_secs = min(args.step_secs, 3.0)
    load_s = await load_accounts(node.host, node.port, rows, args.accounts_docs)
    settled_s = await settle_merges(node, "accounts")
    metrics = await scrape(ctl)
    reactors = (
        int(metrics["gauges"].get("xapiand_servers_running", 0))
        or len([k for k in metrics["lag"] if k.startswith("http")])
        or (os.cpu_count() or 1)
    )
    ctx = Ctx(args, node, rows, args.accounts_docs, reactors, ctl)
    log(
        f"{reactors} reactors; accounts {args.accounts_docs} documents loaded in {load_s:.1f} s; rate {args.rate}/s for {args.duration} s"
    )
    extra: dict[str, Any] = {}
    if args.index_size:
        factor = float(args.index_size.lower().removesuffix("xram"))
        extra["index_size"] = {"requested": args.index_size, **cap_memory(node, factor)}
        log(f"index at {args.index_size}: {json.dumps(extra['index_size'])}")

    if args.suite:
        # One load of `accounts` for the whole gate sequence: saturate, T5 (unloaded, loaded, same-index), T8.
        sat = await saturate(ctx)
        extra["saturate"] = sat
        args.rate = max(10.0, math.floor(sat["saturating_rate"] / 2 / 5) * 5)
        ctx.args_rate_note = f"light rate {args.rate:.0f}/s is half the saturating rate, rounded down to 5"
        log(ctx.args_rate_note)
        await phase(ctx, "unloaded", args.rate, False, args.duration)
        await phase(ctx, "loaded", args.rate, True, args.duration)
        args.restore_into = "accounts"
        await phase(ctx, "loaded-same-index", args.rate, True, args.duration)
        args.restore_into = "restore-target"
        await phase(ctx, "overload-light", 0.0, False, args.duration, held="light")
        await phase(ctx, "overload-heavy", args.rate, False, args.duration, held="heavy")
    elif args.saturate:
        extra["saturate"] = await saturate(ctx)
    elif args.overload:
        # Heavy runs beside a light stream at --rate, whose requests meet the demoted cap's 503.
        kind = args.overload[0]
        await phase(ctx, f"overload-{kind}", args.rate if kind == "heavy" else 0.0, False, args.duration, held=kind)
    else:
        if args.phase in ("unloaded", "both"):
            await phase(ctx, "unloaded", args.rate, False, args.duration)
        if args.phase in ("loaded", "both"):
            await phase(ctx, "loaded", args.rate, True, args.duration)

    if args.lua_loop:
        extra["lua_loop"] = {
            "requested": args.lua_loop,
            "status": "not run here: harness/lua_bounds.py --loops N runs the bounds and the slice check (T10)",
        }

    out: dict[str, Any] = {
        "meta": {
            "bin": args.bin,
            "target": args.target,
            "commit": subprocess.run(
                ["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True, text=True, check=False
            ).stdout.strip(),
            "host_cpus": os.cpu_count(),
            "reactors": reactors,
            "accounts_docs": args.accounts_docs,
            "rate": args.rate,
            "duration": args.duration,
            "restore_into": ctx.restore_index,
            "seed": args.seed,
            "rate_note": ctx.args_rate_note,
            "argv": sys.argv[1:],
            "started": t_boot,
            "load_accounts_s": round(load_s, 2),
            "settle_after_load_s": round(settled_s, 2),
            "total_s": round(time.time() - t_boot, 2),
        },
        "phases": ctx.results,
        "assertions": ctx.assertions,
        **extra,
    }
    if args.dry_run:
        out["dry_run_checks"] = dry_checks(out)
    text = json.dumps(out, indent=1)
    if args.out:
        with open(args.out, "w") as fh:
            fh.write(text + "\n")
        log(f"wrote {args.out}")
    else:
        print(text)
    await ctl.close()
    node.stop()
    bad = [x for x in ctx.assertions if not x["ok"]]
    bad += [c for c in out.get("dry_run_checks", []) if not c["ok"]]
    if bad and not args.no_assert:
        log(f"FAILED: {bad}")
        return 1
    return 0


def dry_checks(out: dict[str, Any]) -> list[dict[str, Any]]:
    """A dry run must measure something: every result below is non-empty."""
    checks: list[dict[str, Any]] = []
    for p in out["phases"]:
        light = p.get("light")
        if light:
            checks.append(
                {
                    "what": f"{p['name']}: light queries measured",
                    "ok": light["query"]["n"] > 0,
                }
            )
            checks.append(
                {
                    "what": f"{p['name']}: no light errors",
                    "ok": light["errors"]["http_other"] == 0 and light["errors"]["exceptions"] == 0,
                }
            )
        held = p.get("held")
        if held:
            checks.append({"what": f"{p['name']}: held requests answered", "ok": held["answered"]["n"] > 0})
            checks.append(
                {
                    "what": f"{p['name']}: no held errors",
                    "ok": held["errors"]["http_other"] == 0 and held["errors"]["exceptions"] == 0,
                }
            )
        heavy = p.get("heavy")
        if heavy:
            checks.append(
                {
                    "what": f"{p['name']}: heavy searches ran",
                    "ok": heavy["searches"] > 0,
                }
            )
            checks.append(
                {
                    "what": f"{p['name']}: the RESTORE stream wrote documents",
                    "ok": heavy["restored_docs"] > 0,
                }
            )
            checks.append(
                {
                    "what": f"{p['name']}: no heavy errors",
                    "ok": heavy["errors"]["http_other"] == 0 and heavy["errors"]["exceptions"] == 0,
                }
            )
        checks.append(
            {
                "what": f"{p['name']}: lag deltas per loop",
                "ok": bool(p["lag"]) and all(v["wakes"] > 0 for v in p["lag"].values()),
            }
        )
    return checks


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", help="start a solo strata node from this binary (removed on exit)")
    ap.add_argument(
        "--target",
        default="127.0.0.1:18980",
        help="HOST:PORT (the port --bin listens on)",
    )
    ap.add_argument("--workdir", default=os.path.join(REPO, ".scratch", "stall_load"))
    ap.add_argument("--out")
    ap.add_argument("--accounts-docs", type=int, default=1_000_000)
    ap.add_argument("--phase", choices=("unloaded", "loaded", "both"), default="both")
    ap.add_argument(
        "--rate",
        type=float,
        default=500.0,
        help="light requests per second (open loop)",
    )
    ap.add_argument("--duration", type=float, default=30.0)
    ap.add_argument("--light-conns", type=int, default=64)
    ap.add_argument(
        "--restore-prebuilt",
        type=int,
        default=0,
        help="P1 H1: build this many RESTORE bodies before each heavy phase and write large bodies apart from their head",
    )
    ap.add_argument("--heavy-clients", type=int, default=0, help="default: 4x the reactor count")
    ap.add_argument(
        "--restore-into",
        choices=("restore-target", "accounts"),
        default="restore-target",
    )
    ap.add_argument(
        "--suite", action="store_true", help="saturate, unloaded, loaded, same-index and both overloads on one load"
    )
    ap.add_argument("--saturate", action="store_true")
    ap.add_argument("--saturate-start", type=float, default=25.0)
    ap.add_argument("--saturate-max", type=float, default=20000.0)
    ap.add_argument("--bisect-steps", type=int, default=4)
    ap.add_argument("--step-secs", type=float, default=10.0)
    ap.add_argument(
        "--overload",
        nargs=2,
        metavar=("light|heavy", "F"),
        help="hold F x the in-flight cap in requests (F > 1; the suite holds 1.25x)",
    )
    ap.add_argument("--processors", type=int, default=0, help="the node's reactor count (--processors; 0: its default)")
    ap.add_argument("--lua-loop", type=int, default=0, help="looping scripts (run by harness/lua_bounds.py)")
    ap.add_argument(
        "--index-size",
        default="",
        help="2xram: on Linux, cap the node's cgroup so the index is 2x the page cache it may hold (T6)",
    )
    ap.add_argument(
        "--census",
        action="store_true",
        help="run thread_census.sh mid-phase on a node this script started",
    )
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--no-assert", action="store_true")
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="20,000 documents and short phases; fails if a result is empty",
    )
    args = ap.parse_args()
    args.overload_factor = 1.25
    if args.overload:
        if args.overload[0] not in ("light", "heavy"):
            ap.error("--overload takes light or heavy")
        args.overload_factor = float(args.overload[1])
        if args.overload_factor <= 1.0:
            ap.error("--overload F must exceed 1: the held requests must outnumber the cap")
    if not args.bin and not args.target:
        ap.error("give --bin or --target")
    # Held requests take a socket each, here and in the node this starts (it inherits the limit).
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    want = math.ceil(args.overload_factor * INFLIGHT_CAP * (args.processors or os.cpu_count() or 1)) + 1024
    if soft != resource.RLIM_INFINITY and soft < want:  # a refusal shows later as connection errors
        with contextlib.suppress(ValueError, OSError):
            resource.setrlimit(
                resource.RLIMIT_NOFILE, (want if hard == resource.RLIM_INFINITY else min(want, hard), hard)
            )
    Conn.split_write = args.restore_prebuilt > 0
    return asyncio.run(amain(args))


if __name__ == "__main__":
    sys.exit(main())
