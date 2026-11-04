#!/usr/bin/env python3
"""systems.py -- one adapter per system under test, for the closing benchmark (docs/design/closing-benchmark.md, D7).

An adapter starts and stops its system, creates an index from a neutral schema, builds the bulk, refresh and search
requests, reads the answers back into one shape, and reports the system's effective settings, its disk bytes and (on
Linux) its cgroup counters. Callers execute the requests with their own HTTP client (`Client` here is a blocking one;
stall_load.py has an asyncio one), so the open-loop and closed-loop generators share one translation.

  Xapiand  (systems S and X: Xapiand on strata and on Xapian)    ElasticSearch (E)    Quickwit (Q)

A neutral query is one JSON object per line (harness/closing/queries/*.jsonl):

  {"class": "phrase", "field": "text", "terms": ["new", "york"], "mode": "phrase"}
  {"class": "term_range", "field": "status", "terms": ["200"], "range": {"field": "@timestamp", "gte": 1, "lte": 2}}
  {"class": "sort", "field": "status", "terms": ["200"], "sort": {"field": "@timestamp", "order": "desc"}}
  {"class": "agg_histogram", "agg": {"kind": "date_histogram", "field": "@timestamp", "interval": 3600}}

`mode` is `all` (default), `any` or `phrase`; `command` is `search` (top 10 and the count) or `count`. `check` below
runs D8's count check, the S against X order check and the aggregation comparison on the live systems:

  systems.py check --queries Q.jsonl --schema logs --out report.json --sys S=xapiand@127.0.0.1:18980/logs ...
"""

from __future__ import annotations

import argparse
import contextlib
import http.client
import json
import os
import random
import shutil
import signal
import socket
import subprocess
import sys
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field, replace
from typing import Any, override

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(HERE, "closing")
JSON_CT = "application/json"
NDJSON_CT = "application/x-ndjson"
FIELD_TYPES = ("text", "keyword", "integer", "date")
# `search` is a top-10 whose total may be a lower bound or an estimate (E track_total_hits false, Q count_all false, X
# without check_at_least); `search_exact` and `count` give the exact total on every system (D8 runs in this mode).
COMMANDS = ("search", "search_exact", "count")
EXACT_AT_LEAST = 4294967295
EMPTY: dict[str, Any] = {}

# ---------------------------------------------------------------------------------------------
# Requests, answers, schemas and queries


@dataclass(frozen=True)
class Request:
    method: str
    path: str
    body: bytes = b""
    content_type: str = JSON_CT


@dataclass
class Answer:
    """A search or count answer in one shape. `count` is the exact number of matches; None on an error."""

    status: int
    count: int | None = None
    ids: list[str] = field(default_factory=list)
    aggs: dict[str, Any] = field(default_factory=dict)
    error: str = ""

    @property
    def ok(self) -> bool:
        return self.status < 300 and not self.error


@dataclass
class Bulk:
    ok: bool
    indexed: int
    error: str = ""


@dataclass(frozen=True)
class Schema:
    """A neutral schema: field name to type (text, keyword, integer or date), the document's id field, if any, and
    the field the time-ordered index is keyed on, if any. Fields in `ignore` are stored and not searchable."""

    name: str
    fields: dict[str, str]
    id_field: str | None = None
    time_field: str | None = None
    ignore: tuple[str, ...] = ()

    @staticmethod
    def load(name: str, path: str = os.path.join(CONFIG, "schemas.json")) -> Schema:
        with open(path) as fh:
            raw = json.load(fh)[name]
        for kind in raw["fields"].values():
            if kind not in FIELD_TYPES:
                raise ValueError(f"schema {name}: unknown type {kind}")
        return Schema(name, raw["fields"], raw.get("id"), raw.get("time"), tuple(raw.get("ignore", ())))


@dataclass(frozen=True)
class Query:
    cls: str
    field: str = ""
    terms: tuple[str, ...] = ()
    mode: str = "all"
    range: tuple[str, float | None, float | None] | None = None  # field, gte, lte
    sort: tuple[str, str] | None = None  # field, asc or desc
    agg: tuple[str, str, int] | None = None  # kind, field, interval
    command: str = "search"
    filter: tuple[str, str] | None = None  # field, value: an exact term ANDed with the rest (a tenant)

    @staticmethod
    def parse(line: str) -> Query:
        raw = json.loads(line)
        rng = raw.get("range")
        srt = raw.get("sort")
        agg = raw.get("agg")
        q = Query(
            raw["class"],
            raw.get("field", ""),
            tuple(str(t) for t in raw.get("terms", ())),
            raw.get("mode", "all"),
            (rng["field"], rng.get("gte"), rng.get("lte")) if rng else None,
            (srt["field"], srt.get("order", "desc")) if srt else None,
            (agg["kind"], agg["field"], int(agg.get("interval", 0))) if agg else None,
            raw.get("command", "search"),
            (raw["filter"]["field"], str(raw["filter"]["term"])) if raw.get("filter") else None,
        )
        if q.mode not in ("all", "any", "phrase"):
            raise ValueError(f"unknown mode {q.mode}")
        if q.command not in COMMANDS:
            raise ValueError(f"unknown command {q.command}")
        if q.agg and q.agg[0] not in ("date_histogram", "terms", "stats"):
            raise ValueError(f"unknown aggregation {q.agg[0]}")
        if q.sort and q.sort[1] not in ("asc", "desc"):
            raise ValueError(f"unknown sort order {q.sort[1]}")
        if not (q.terms or q.range or q.agg or q.sort or q.filter or q.cls == "all"):
            raise ValueError("a query needs terms, a range, a filter, a sort or an aggregation")
        return q

    def dumps(self) -> str:
        raw: dict[str, Any] = {"class": self.cls}
        if self.field:
            raw["field"] = self.field
        if self.terms:
            raw["terms"] = list(self.terms)
            raw["mode"] = self.mode
        if self.range:
            raw["range"] = {"field": self.range[0], "gte": self.range[1], "lte": self.range[2]}
        if self.sort:
            raw["sort"] = {"field": self.sort[0], "order": self.sort[1]}
        if self.agg:
            raw["agg"] = {"kind": self.agg[0], "field": self.agg[1], "interval": self.agg[2]}
        if self.filter:
            raw["filter"] = {"field": self.filter[0], "term": self.filter[1]}
        raw["command"] = self.command
        return json.dumps(raw)

    def as_command(self, command: str) -> Query:
        return replace(self, command=command)

    def vary(self, run: int, seed: int = 7) -> Query:
        """Run `run` of the query with its range bounds shifted by 0 to 9 units drawn from a seeded stream (run 0 is
        the query as written), so a repeated run cannot hit a stored filter bitset."""
        if self.range is None or run == 0:
            return self
        shift = random.Random(f"{seed}:{run}:{self.label}").randrange(10)
        fld, lo, hi = self.range
        return replace(self, range=(fld, None if lo is None else lo + shift, None if hi is None else hi + shift))

    @property
    def label(self) -> str:
        what = " ".join(self.terms) if self.terms else (self.agg or self.sort or self.range or ("", ""))[0]
        suffix = f"@{self.filter[1]}" if self.filter else ""
        return f"{self.cls}:{what}{suffix}"[:80]


def load_queries(path: str) -> list[Query]:
    with open(path) as fh:
        return [Query.parse(ln) for ln in fh if ln.strip()]


# ---------------------------------------------------------------------------------------------
# A blocking keep-alive client


class Client:
    def __init__(self, target: str, timeout: float = 600.0) -> None:
        self.target, self.timeout = target, timeout
        host, _, port = target.partition(":")
        self.host, self.port = host, int(port)
        self.conn: http.client.HTTPConnection | None = None

    def close(self) -> None:
        if self.conn is not None:
            with contextlib.suppress(Exception):
                self.conn.close()
        self.conn = None

    def call(self, req: Request) -> tuple[int, bytes]:
        for attempt in (0, 1):
            try:
                if self.conn is None:
                    self.conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
                self.conn.request(
                    req.method, req.path, body=req.body or None, headers={"Content-Type": req.content_type}
                )
                resp = self.conn.getresponse()
                return resp.status, resp.read()
            except (OSError, http.client.HTTPException):
                self.close()
                if attempt:
                    raise
        raise AssertionError("unreachable")


def dir_bytes(path: str) -> int:
    total = 0
    for root, _dirs, files in os.walk(path):
        for fn in files:
            with contextlib.suppress(OSError):
                total += (
                    os.lstat(os.path.join(root, fn)).st_blocks * 512
                )  # du's measure: a sparse file counts what it holds
    return total


def dir_bytes_settled(path: str, max_wait: float = 20.0, quiet: float = 1.0) -> int:
    """Bytes under `path` once they have held still for `quiet` seconds (a load's flushes and merges have landed)."""
    deadline = time.monotonic() + max_wait
    last = dir_bytes(path)
    stable = time.monotonic()
    while time.monotonic() < deadline and time.monotonic() - stable < quiet:
        time.sleep(0.25)
        cur = dir_bytes(path)
        if cur != last:
            last, stable = cur, time.monotonic()
    return last


class Cgroup:
    """cgroup v2 counters of a process's cgroup (Linux); every reader returns None where the file is absent."""

    def __init__(self, path: str) -> None:
        self.path = path

    @staticmethod
    def of(pid: int) -> Cgroup | None:
        try:
            with open(f"/proc/{pid}/cgroup") as fh:
                rel = next(ln.strip().split("::", 1)[1] for ln in fh if ln.startswith("0::"))
        except (OSError, StopIteration):
            return None
        path = "/sys/fs/cgroup" + rel
        return Cgroup(path) if os.path.isdir(path) else None

    def read(self, name: str) -> str | None:
        try:
            with open(os.path.join(self.path, name)) as fh:
                return fh.read()
        except OSError:
            return None

    def stat(self, name: str) -> dict[str, int]:
        text = self.read(name) or ""
        return {k: int(v) for k, _, v in (ln.partition(" ") for ln in text.splitlines()) if v.strip().isdigit()}

    def snapshot(self) -> dict[str, Any]:
        cur = self.read("memory.current")
        return {
            "memory_current": int(cur) if cur else None,
            "memory_peak": int(p) if (p := self.read("memory.peak")) else None,
            "memory_stat": self.stat("memory.stat"),
            "cpu_usage_usec": self.stat("cpu.stat").get("usage_usec"),
            "cpuset_effective": (self.read("cpuset.cpus.effective") or "").strip() or None,
            "memory_max": (self.read("memory.max") or "").strip() or None,
        }


# ---------------------------------------------------------------------------------------------
# The adapter interface


@dataclass
class Launch:
    """How a system is started: the binary or home, the data directory, ports, and the resource envelope."""

    binary: str
    workdir: str
    port: int
    cpus: int = 8
    memory_bytes: int = 0  # 0: no cap told to the system (the cgroup caps it)
    prefix: tuple[str, ...] = ()  # e.g. systemd-run --scope -p AllowedCPUs=0-7 -p MemoryMax=32G
    memory_lock: bool = False  # E: bootstrap.memory_lock (needs the memlock limit; the closing run sets it)
    shards: int = 1  # S and X: default shards per index (an arm setting: 1 and 5); tenants are always one shard
    caches_on: bool = False  # the probe's default-cache run: Q's partial request cache at its default
    durability: str = "async"  # E: async (the comparison tier) or request (E's default, the report row)
    extra: tuple[str, ...] = ()
    keep: bool = False  # start on the data already in workdir/data (a pristine copy), without wiping it


class System(ABC):
    name = ""

    def __init__(self, target: str) -> None:
        self.target = target
        self.proc: subprocess.Popen[bytes] | None = None
        self.datadir = ""
        self.workdir = ""
        self.launch: Launch | None = None

    # -- lifecycle
    @abstractmethod
    def command(self, launch: Launch) -> tuple[list[str], dict[str, str]]: ...

    @abstractmethod
    def ready_request(self) -> Request: ...

    def start(self, launch: Launch, wait: float = 180.0) -> None:
        self.launch = launch
        self.workdir = os.path.abspath(launch.workdir)
        self.datadir = os.path.join(self.workdir, "data")
        if not launch.keep:
            shutil.rmtree(self.workdir, ignore_errors=True)
        os.makedirs(self.datadir, exist_ok=True)
        host, _, port = self.target.partition(":")
        with socket.socket() as probe:
            probe.settimeout(1)
            if probe.connect_ex((host, int(port))) == 0:
                raise SystemExit(f"{self.target} is already in use; stop what is listening before starting {self.name}")
        cmd, env = self.command(launch)
        full_env = dict(os.environ)
        full_env.update(env)
        log = open(os.path.join(self.workdir, "node.log"), "wb")
        self.proc = subprocess.Popen(
            [*launch.prefix, *cmd], stdout=log, stderr=log, env=full_env, cwd=self.workdir, start_new_session=True
        )
        self.wait_up(wait)

    def wait_up(self, wait: float = 180.0) -> float:
        """Seconds from now until the system answers its readiness request."""
        t0 = time.monotonic()
        client = Client(self.target, timeout=5)
        while time.monotonic() - t0 < wait:
            if self.proc is not None and self.proc.poll() is not None:
                raise SystemExit(f"{self.name} exited with {self.proc.returncode}; see {self.workdir}/node.log")
            try:
                if client.call(self.ready_request())[0] == 200:
                    client.close()
                    return time.monotonic() - t0
            except (OSError, http.client.HTTPException):
                client.close()
            time.sleep(0.1)
        raise SystemExit(f"{self.name} did not come up in {wait:.0f} s")

    def stop(self, kill: bool = False, wait: float = 120.0) -> None:
        if self.proc is None:
            return
        pgid = self.proc.pid
        with contextlib.suppress(OSError):
            os.killpg(pgid, signal.SIGKILL if kill else signal.SIGTERM)
        try:
            self.proc.wait(timeout=wait)
        except subprocess.TimeoutExpired:
            with contextlib.suppress(OSError):
                os.killpg(pgid, signal.SIGKILL)
            self.proc.wait(timeout=30)
        with contextlib.suppress(OSError):  # a member the leader outlived (ES's JVM)
            os.killpg(pgid, signal.SIGKILL)
        self.proc = None

    def spawn(self) -> None:
        """Start the process again on the same data without waiting for it to answer."""
        assert self.launch is not None
        cmd, env = self.command(self.launch)
        full_env = dict(os.environ)
        full_env.update(env)
        log = open(os.path.join(self.workdir, "node.log"), "ab")
        self.proc = subprocess.Popen(
            [*self.launch.prefix, *cmd], stdout=log, stderr=log, env=full_env, cwd=self.workdir, start_new_session=True
        )

    def restart(self, kill: bool = False) -> float:
        """Stop (SIGKILL when `kill`) and start again on the same data; seconds until it answers."""
        self.stop(kill=kill)
        t0 = time.monotonic()
        self.spawn()
        self.wait_up()
        return time.monotonic() - t0

    @property
    def pid(self) -> int | None:
        return self.proc.pid if self.proc is not None else None

    def rss_kb(self) -> int | None:
        """Resident bytes (KiB) of the system's process group."""
        if self.proc is None:
            return None
        out = subprocess.run(["ps", "-A", "-o", "pgid=,rss="], capture_output=True, text=True, check=False).stdout
        total = 0
        for ln in out.splitlines():
            parts = ln.split()
            if len(parts) == 2 and parts[0] == str(self.proc.pid):
                total += int(parts[1])
        return total

    def group_pids(self) -> list[int]:
        if self.proc is None:
            return []
        out = subprocess.run(["ps", "-A", "-o", "pid=,pgid="], capture_output=True, text=True, check=False).stdout
        return [
            int(p)
            for p, g in (ln.split() for ln in out.splitlines() if len(ln.split()) == 2)
            if g == str(self.proc.pid)
        ]

    def fd_count(self) -> int | None:
        """Open descriptors across the system's process group (Linux /proc); None where unreadable."""
        total = 0
        for pid in self.group_pids():
            try:
                total += len(os.listdir(f"/proc/{pid}/fd"))
            except OSError:
                continue
        return total or None

    def cgroup(self) -> Cgroup | None:
        return Cgroup.of(self.proc.pid) if self.proc is not None else None

    # -- indexes and documents
    @abstractmethod
    def create_index(self, index: str, schema: Schema, bulk: bool = False) -> list[Request]: ...

    @abstractmethod
    def bulk_request(self, index: str, schema: Schema, lines: list[bytes], last: bool = False) -> Request: ...

    @abstractmethod
    def parse_bulk(self, status: int, body: bytes, n: int) -> Bulk: ...

    @abstractmethod
    def floor_request(self, index: str) -> Request:
        """The cheapest request the system answers: the client floor published beside T4's times."""

    def segments(self, client: Client, index: str) -> int | None:
        """Segments (E) or splits (Q) the index holds; None where the system reports none (S and X: counted on disk)."""
        return None

    def settle_merges(self, client: Client, index: str, quiet: float = 5.0, limit: float = 900.0) -> dict[str, Any]:
        """Wait until the segment count has held still for `quiet` seconds (the merges a load started are finished),
        so every build ends settled (C7); returns the count and the seconds waited."""
        t0 = time.monotonic()
        last = self.segments(client, index)
        still = time.monotonic()
        while last is not None and time.monotonic() - t0 < limit and time.monotonic() - still < quiet:
            time.sleep(0.5)
            now = self.segments(client, index)
            if now != last:
                last, still = now, time.monotonic()
        return {"segments": last, "waited_s": round(time.monotonic() - t0, 2)}

    def durable(self, client: Client, index: str) -> None:
        """What makes the data on disk final before `du` reads it (D13): a flush where the system has one, then sync."""
        os.sync()

    @abstractmethod
    def commit_requests(self, index: str, bulk: bool = False) -> list[Request]:
        """What makes everything written so far searchable (and, after a `bulk` load, restores the serving settings)."""

    # -- single documents (the mixed-load audit): an id, a document without it, and every id with its `n` back out
    supports_delete = True

    @abstractmethod
    def put_request(self, index: str, doc_id: str, doc: dict[str, Any]) -> Request: ...

    def delete_request(self, index: str, doc_id: str) -> Request:
        raise NotImplementedError(f"{self.name} deletes no document by id")

    @abstractmethod
    def dump(self, client: Client, index: str) -> list[tuple[str, int]]:
        """Every (id, n) in the index, duplicates included."""

    @abstractmethod
    def dump_stream(self, client: Client, index: str, stream: str) -> list[int]:
        """The `seq` of every document whose `stream` field is `stream`, duplicates included (T7's marked writes)."""

    # -- search
    @abstractmethod
    def search_request(self, index: str, schema: Schema, q: Query) -> Request: ...

    @abstractmethod
    def parse_answer(self, q: Query, status: int, body: bytes) -> Answer: ...

    def run(self, client: Client, index: str, schema: Schema, q: Query) -> Answer:
        status, body = client.call(self.search_request(index, schema, q))
        return self.parse_answer(q, status, body)

    def setup(self, client: Client, index: str, schema: Schema, bulk: bool = False) -> None:
        for req in self.create_index(index, schema, bulk):
            status, body = client.call(req)
            if status >= 300:
                raise SystemExit(f"{self.name}: creating {index}: HTTP {status}: {body[:300]!r}")

    def commit(self, client: Client, index: str, bulk: bool = False) -> None:
        for req in self.commit_requests(index, bulk):
            status, body = client.call(req)
            if status >= 300:
                raise SystemExit(f"{self.name}: committing {index}: HTTP {status}: {body[:300]!r}")

    def wait_count(self, client: Client, index: str, schema: Schema, want: int, wait: float = 120.0) -> int:
        """Poll the match-all count until it reaches `want` (writes become searchable); returns the last count."""
        probe = Query("all", command="count")
        deadline = time.monotonic() + wait
        got = -1
        while time.monotonic() < deadline:
            ans = self.run(client, index, schema, probe)
            got = ans.count if ans.count is not None else -1
            if got >= want:
                break
            time.sleep(0.2)
        return got

    # -- measurements
    def disk_bytes(self, settle: bool = True) -> int | None:
        if not self.datadir or not os.path.isdir(self.datadir):
            return None
        return dir_bytes_settled(self.datadir) if settle else dir_bytes(self.datadir)

    @abstractmethod
    def cache_counters(self, client: Client) -> dict[str, Any]:
        """Hit and miss counters of the result caches (`result_*`, which must stay 0) and of the filter caches
        (`filter_*`, left on and stated). A system with no such cache returns an empty `result_hits`."""

    @abstractmethod
    def effective(self, client: Client, index: str | None = None) -> dict[str, Any]:
        """The settings the running system reports, read back (never the ones asked for)."""

    def assert_effective(self, eff: dict[str, Any], cpus: int) -> list[str]:
        """Problems between the settings read back and what D2 asked for (an empty list is a pass)."""
        got = eff.get("processors")
        return [] if got == cpus else [f"{self.name}: processors read back {got}, asked {cpus}"]


def clean_term(term: str) -> str:
    """A query term is letters, digits and the few symbols a log URL carries; nothing a parser reads as syntax."""
    keep = "".join(c for c in term if c.isalnum() or c in "_-./")
    if not keep:
        raise ValueError(f"empty term from {term!r}")
    return keep


def drop_caches() -> bool:
    """Drop the page cache (Linux, as root); False where it cannot be done."""
    try:
        subprocess.run(["sync"], check=False)
        with open("/proc/sys/vm/drop_caches", "w") as fh:
            fh.write("3\n")
    except OSError:
        return False
    return True


def int_text(value: float) -> str:
    return str(int(value))


def rfc3339(seconds: float) -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(seconds))


def bucket(raw: Any) -> str:
    """Bucket keys compare as text: integers without a fraction."""
    if isinstance(raw, float) and raw == int(raw):
        return str(int(raw))
    return str(raw)


def option_lines(name: str) -> list[str]:
    path = os.path.join(CONFIG, name)
    with open(path) as fh:
        return [ln.strip() for ln in fh if ln.strip() and not ln.startswith("#")]


def render(name: str, **values: object) -> str:
    with open(os.path.join(CONFIG, name)) as fh:
        text = fh.read()
    for key, value in values.items():
        text = text.replace("{" + key + "}", str(value))
    return text


# ---------------------------------------------------------------------------------------------
# Xapiand (S and X)


class Xapiand(System):
    name = "xapiand"

    def command(self, launch: Launch) -> tuple[list[str], dict[str, str]]:
        cmd = [
            os.path.abspath(launch.binary),
            "--port",
            str(launch.port),
            "--name",
            "bench",
            "-D",
            self.datadir,
            "--processors",
            str(launch.cpus),
            "--shards",
            str(launch.shards),
            *option_lines("xapiand.opts"),
            *launch.extra,
        ]
        return cmd, {}

    def ready_request(self) -> Request:
        return Request("GET", "/")

    def schema_body(self, schema: Schema) -> dict[str, Any]:
        types = {"text": {"_type": "text"}, "keyword": {"_type": "keyword"}, "integer": {"_type": "integer"}}
        fields: dict[str, Any] = {k: ({"_type": "date"} if v == "date" else types[v]) for k, v in schema.fields.items()}
        for name in schema.ignore:
            fields[name] = {"_type": "keyword", "_index": "none"}
        if schema.id_field:
            fields[schema.id_field] = {"_type": "keyword", "_index": "none"}
        return fields

    def create_index(self, index: str, schema: Schema, bulk: bool = False) -> list[Request]:
        shards = 1 if schema.name.startswith("tenants") else (self.launch.shards if self.launch else 1)
        body = {"_schema": self.schema_body(schema), "_settings": {"number_of_shards": shards}}
        return [Request("PUT", f"/{index}/", json.dumps(body).encode())]

    def bulk_request(self, index: str, schema: Schema, lines: list[bytes], last: bool = False) -> Request:
        if schema.id_field:  # RESTORE reads `_id` last in the object
            out = []
            for ln in lines:
                doc_id = json.loads(ln)[schema.id_field]
                out.append(ln.rstrip()[:-1] + b',"_id":' + json.dumps(str(doc_id)).encode() + b"}")
            lines = out
        return Request("RESTORE", f"/{index}/" + ("?commit" if last else ""), b"\n".join(lines) + b"\n", NDJSON_CT)

    def parse_bulk(self, status: int, body: bytes, n: int) -> Bulk:
        if status >= 300:
            return Bulk(False, 0, f"HTTP {status}: {body[:200]!r}")
        try:
            done = int(json.loads(body).get("indexed", -1))
        except ValueError:
            done = -1
        return Bulk(done == n, done, "" if done == n else f"indexed {done} of {n}")

    def commit_requests(self, index: str, bulk: bool = False) -> list[Request]:
        return [Request("POST", f"/{index}/:commit")]

    @override
    def floor_request(self, index: str) -> Request:
        return Request("GET", f"/{index}/floor-missing-document")

    @override
    def put_request(self, index: str, doc_id: str, doc: dict[str, Any]) -> Request:
        return Request("PUT", f"/{index}/{doc_id}", json.dumps(doc).encode())

    @override
    def delete_request(self, index: str, doc_id: str) -> Request:
        return Request("DELETE", f"/{index}/{doc_id}")

    @override
    def dump(self, client: Client, index: str) -> list[tuple[str, int]]:
        status, body = client.call(Request("DUMP", f"/{index}/"))
        if status != 200:
            raise SystemExit(f"xapiand: DUMP {index}: HTTP {status}")
        return [(str(d["_id"]), int(d["n"])) for d in json.loads(body)]

    @override
    def dump_stream(self, client: Client, index: str, stream: str) -> list[int]:
        out: list[int] = []
        while True:
            body = {"_query": {"stream": stream}, "_sort": [{"seq": "asc"}], "_limit": 1000, "_offset": len(out)}
            status, raw = client.call(
                Request("SEARCH", f"/{index}/?check_at_least={EXACT_AT_LEAST}", json.dumps(body).encode())
            )
            if status != 200:
                raise SystemExit(f"xapiand: dump of stream {stream} in {index}: HTTP {status}: {raw[:200]!r}")
            hits = json.loads(raw).get("hits", [])
            out.extend(int(h["seq"]) for h in hits)
            if len(hits) < 1000:
                return out

    def clause(self, schema: Schema, q: Query) -> dict[str, Any] | None:
        parts: list[dict[str, Any]] = []
        if q.terms:
            if q.mode == "phrase" and len(q.terms) > 1:
                parts.append({q.field: {"_phrase": " ".join(clean_term(t) for t in q.terms)}})
            else:
                terms = [{q.field: clean_term(t)} for t in q.terms]
                joined = terms[0] if len(terms) == 1 else {("_or" if q.mode == "any" else "_and"): terms}
                parts.append(joined)
        if q.filter:
            parts.append({q.filter[0]: q.filter[1]})
        if q.range:
            rng: dict[str, Any] = {}
            if q.range[1] is not None:
                rng["_from"] = q.range[1]
            if q.range[2] is not None:
                rng["_to"] = q.range[2]
            parts.append({q.range[0]: {"_in": {"_range": rng}}})
        if not parts:
            return None
        return parts[0] if len(parts) == 1 else {"_and": parts}

    def search_request(self, index: str, schema: Schema, q: Query) -> Request:
        body: dict[str, Any] = {"_query": self.clause(schema, q) or "*"}
        exact = f"?check_at_least={EXACT_AT_LEAST}" if q.command != "search" else ""
        if q.command == "count":
            return Request("COUNT", f"/{index}/{exact}", json.dumps(body).encode())
        body["_limit"] = 0 if q.agg else 10
        if q.sort:
            body["_sort"] = [{q.sort[0]: q.sort[1]}]
        if q.agg:
            kind, fld, interval = q.agg
            spec = {
                "date_histogram": {"_histogram": {"_field": fld, "_interval": interval}},
                "terms": {"_values": {"_field": fld}},
                "stats": {"_stats": {"_field": fld}},
            }[kind]
            body["_aggs"] = {"a": spec}
        return Request("SEARCH", f"/{index}/{exact}", json.dumps(body).encode())

    def parse_answer(self, q: Query, status: int, body: bytes) -> Answer:
        if status >= 300:
            return Answer(status, error=body[:200].decode("utf-8", "replace"))
        doc = json.loads(body)
        ans = Answer(status, int(doc["total"]), [str(h["_id"]) for h in doc.get("hits", ()) if "_id" in h])
        agg = doc.get("aggregations", {}).get("a")
        if q.agg and isinstance(agg, list):
            ans.aggs = {bucket(b["_key"]): int(b["_doc_count"]) for b in agg}
        elif q.agg and isinstance(agg, dict):
            ans.aggs = {k[1:]: float(v) for k, v in agg.items() if k in ("_count", "_min", "_max", "_avg", "_sum")}
        return ans

    def cache_counters(self, client: Client) -> dict[str, Any]:
        return {
            "result_hits": 0,
            "filter_hits": 0,
            "note": "no search-result or filter cache in src/ (checked by grep)",
        }

    def effective(self, client: Client, index: str | None = None) -> dict[str, Any]:
        status, body = client.call(Request("GET", "/"))
        doc = json.loads(body) if status == 200 else {}
        opts: dict[str, Any] = doc.get("options", EMPTY)
        return {
            "system": "xapiand",
            "version": doc.get("versions", {}).get("Xapiand"),
            "hash": doc.get("server", {}).get("hash"),
            "processors": opts.get("processors"),
            "shards": opts.get("thread_pools", {}).get("num_shards"),
            "database_pool_size": opts.get("cache", {}).get("database_pool_size"),
            "options": opts,
        }

    @override
    def assert_effective(self, eff: dict[str, Any], cpus: int) -> list[str]:
        problems = super().assert_effective({**eff, "processors": int(eff.get("processors") or 0)}, cpus)
        asked = self.launch.shards if self.launch else 1
        if eff.get("shards") != asked:
            problems.append(f"xapiand: default shards read back {eff.get('shards')}, asked {asked}")
        return problems


# ---------------------------------------------------------------------------------------------
# Elasticsearch (E)


class Elasticsearch(System):
    name = "es"

    def command(self, launch: Launch) -> tuple[list[str], dict[str, str]]:
        home = os.path.join(os.path.dirname(os.path.abspath(launch.binary)), "..")
        conf = os.path.join(self.workdir, "conf")
        if not os.path.isdir(conf):
            shutil.copytree(os.path.join(home, "config"), conf)
        with open(os.path.join(home, "config", "elasticsearch.yml")) as base:
            text = base.read()
        values = {
            "port": launch.port,
            "transport": launch.port + 100,
            "data": self.datadir,
            "logs": os.path.join(self.workdir, "logs"),
            "processors": launch.cpus,
            "memory_lock": "true" if launch.memory_lock else "false",
        }
        with open(os.path.join(conf, "elasticsearch.yml"), "w") as fh:
            fh.write(text + "\n" + render("elasticsearch.yml", **values))
        opts = ""
        if launch.memory_bytes:
            heap = f"{max(256, launch.memory_bytes // (1 << 20))}m"
            opts = f"-Xms{heap} -Xmx{heap}"
        return [os.path.abspath(launch.binary), *launch.extra], {"ES_PATH_CONF": conf, "ES_JAVA_OPTS": opts}

    @override
    def start(self, launch: Launch, wait: float = 180.0) -> None:
        self.durability = launch.durability
        super().start(launch, wait)

    durability = "async"

    def ready_request(self) -> Request:
        return Request("GET", "/_cluster/health?wait_for_status=yellow&timeout=5s")

    def create_index(self, index: str, schema: Schema, bulk: bool = False) -> list[Request]:
        types = {"text": {"type": "text"}, "keyword": {"type": "keyword"}, "integer": {"type": "long"}}
        props: dict[str, Any] = {
            k: ({"type": "date", "format": "epoch_second"} if v == "date" else types[v])
            for k, v in schema.fields.items()
        }
        settings = json.loads(render("elasticsearch-index.json", durability=self.durability))
        if bulk:  # the bulk-load guidance: no refresh while loading
            settings["settings"]["refresh_interval"] = "-1"
        settings["mappings"] = {"dynamic": False, "properties": props}
        return [Request("PUT", f"/{index}", json.dumps(settings).encode())]

    def bulk_request(self, index: str, schema: Schema, lines: list[bytes], last: bool = False) -> Request:
        out: list[bytes] = []
        for ln in lines:
            if schema.id_field:
                out.append(b'{"index":{"_id":' + json.dumps(str(json.loads(ln)[schema.id_field])).encode() + b"}}")
            else:
                out.append(b'{"index":{}}')
            out.append(ln.rstrip())
        return Request("POST", f"/{index}/_bulk?filter_path=errors,items.*.error", b"\n".join(out) + b"\n", NDJSON_CT)

    def parse_bulk(self, status: int, body: bytes, n: int) -> Bulk:
        if status >= 300:
            return Bulk(False, 0, f"HTTP {status}: {body[:200]!r}")
        doc = json.loads(body)
        if doc.get("errors"):
            return Bulk(False, 0, json.dumps(doc.get("items", [])[:1])[:300])
        return Bulk(True, n)

    @override
    def floor_request(self, index: str) -> Request:
        return Request("GET", "/")

    @override
    def segments(self, client: Client, index: str) -> int | None:
        status, body = client.call(
            Request("GET", f"/{index}/_stats/segments?filter_path=_all.primaries.segments.count")
        )
        return int(json.loads(body)["_all"]["primaries"]["segments"]["count"]) if status == 200 else None

    @override
    def durable(self, client: Client, index: str) -> None:
        client.call(Request("POST", f"/{index}/_flush?force=true&wait_if_ongoing=true"))
        os.sync()

    request_cache = False  # the probe turns it on once, to show the counter is alive

    @override
    def put_request(self, index: str, doc_id: str, doc: dict[str, Any]) -> Request:
        return Request("PUT", f"/{index}/_doc/{doc_id}", json.dumps(doc).encode())

    @override
    def delete_request(self, index: str, doc_id: str) -> Request:
        return Request("DELETE", f"/{index}/_doc/{doc_id}")

    @override
    def dump(self, client: Client, index: str) -> list[tuple[str, int]]:
        client.call(Request("POST", f"/{index}/_refresh"))
        body = json.dumps({"size": 5000, "_source": ["n"], "query": {"match_all": {}}}).encode()
        status, raw = client.call(Request("POST", f"/{index}/_search?scroll=2m", body))
        out: list[tuple[str, int]] = []
        while status == 200:
            doc = json.loads(raw)
            hits = doc["hits"]["hits"]
            if not hits:
                break
            out.extend((str(h["_id"]), int(h["_source"]["n"])) for h in hits)
            more = json.dumps({"scroll": "2m", "scroll_id": doc["_scroll_id"]}).encode()
            status, raw = client.call(Request("POST", "/_search/scroll", more))
        if status != 200:
            raise SystemExit(f"es: scroll {index}: HTTP {status}")
        return out

    @override
    def dump_stream(self, client: Client, index: str, stream: str) -> list[int]:
        client.call(Request("POST", f"/{index}/_refresh"))
        body = json.dumps({"size": 5000, "_source": ["seq"], "query": {"term": {"stream": stream}}}).encode()
        status, raw = client.call(Request("POST", f"/{index}/_search?scroll=2m", body))
        out: list[int] = []
        while status == 200:
            doc = json.loads(raw)
            hits = doc["hits"]["hits"]
            if not hits:
                break
            out.extend(int(h["_source"]["seq"]) for h in hits)
            more = json.dumps({"scroll": "2m", "scroll_id": doc["_scroll_id"]}).encode()
            status, raw = client.call(Request("POST", "/_search/scroll", more))
        if status != 200:
            raise SystemExit(f"es: scroll of stream {stream} in {index}: HTTP {status}")
        return out

    def commit_requests(self, index: str, bulk: bool = False) -> list[Request]:
        reqs: list[Request] = []
        if bulk:
            body = json.dumps({"index": {"refresh_interval": "1s"}}).encode()
            reqs.append(Request("PUT", f"/{index}/_settings", body))
        reqs.append(Request("POST", f"/{index}/_refresh"))
        return reqs

    def clause(self, schema: Schema, q: Query) -> dict[str, Any]:
        must: list[dict[str, Any]] = []
        filt: list[dict[str, Any]] = []
        if q.terms:
            terms = [clean_term(t) for t in q.terms]
            if schema.fields.get(q.field, "text") != "text":
                leaves = [{"term": {q.field: t}} for t in terms]
                must.append(
                    leaves[0] if len(leaves) == 1 else {"bool": {"must" if q.mode == "all" else "should": leaves}}
                )
            elif q.mode == "phrase" and len(terms) > 1:
                must.append({"match_phrase": {q.field: " ".join(terms)}})
            else:
                must.append(
                    {"match": {q.field: {"query": " ".join(terms), "operator": "or" if q.mode == "any" else "and"}}}
                )
        if q.filter:
            filt.append({"term": {q.filter[0]: q.filter[1]}})
        if q.range:
            rng: dict[str, Any] = {}
            if q.range[1] is not None:
                rng["gte"] = q.range[1]
            if q.range[2] is not None:
                rng["lte"] = q.range[2]
            if schema.fields.get(q.range[0]) == "date":
                rng["format"] = "epoch_second"  # a number in a range is epoch_millis unless the range says otherwise
            filt.append({"range": {q.range[0]: rng}})
        if not must and not filt:
            return {"match_all": {}}
        if len(must) + len(filt) == 1:
            return (must or filt)[0]
        return {"bool": {"must": must, "filter": filt}}

    def search_request(self, index: str, schema: Schema, q: Query) -> Request:
        body: dict[str, Any] = {"query": self.clause(schema, q)}
        if q.command == "count":
            return Request("POST", f"/{index}/_count", json.dumps(body).encode())
        body["size"] = 0 if q.agg else 10
        body["track_total_hits"] = q.command == "search_exact"
        if q.sort:
            body["sort"] = [{q.sort[0]: q.sort[1]}]
        if q.agg:
            kind, fld, interval = q.agg
            spec = {
                "date_histogram": {"date_histogram": {"field": fld, "fixed_interval": f"{interval}s"}},
                "terms": {"terms": {"field": fld, "size": 100}},
                "stats": {"stats": {"field": fld}},
            }[kind]
            body["aggs"] = {"a": spec}
        return Request(
            "POST",
            f"/{index}/_search?request_cache={'true' if self.request_cache else 'false'}",
            json.dumps(body).encode(),
        )

    def parse_answer(self, q: Query, status: int, body: bytes) -> Answer:
        if status >= 300:
            return Answer(status, error=body[:200].decode("utf-8", "replace"))
        doc = json.loads(body)
        if q.command == "count":
            return Answer(status, int(doc["count"]))
        hits = doc["hits"]
        total = hits.get("total")  # absent when track_total_hits is false
        ans = Answer(status, None if total is None else int(total["value"]), [str(h["_id"]) for h in hits["hits"]])
        agg = doc.get("aggregations", {}).get("a")
        if q.agg and agg is not None:
            if q.agg[0] == "stats":
                ans.aggs = {k: float(agg[k]) for k in ("count", "min", "max", "avg", "sum")}
            elif q.agg[0] == "date_histogram":
                ans.aggs = {bucket(int(b["key"]) // 1000): int(b["doc_count"]) for b in agg["buckets"]}
            else:
                ans.aggs = {bucket(b["key"]): int(b["doc_count"]) for b in agg["buckets"]}
        return ans

    def cache_counters(self, client: Client) -> dict[str, Any]:
        path = "/_nodes/stats/indices/request_cache,query_cache?filter_path=nodes.*.indices"
        status, body = client.call(Request("GET", path))
        nodes: dict[str, Any] = json.loads(body).get("nodes", EMPTY) if status == 200 else EMPTY
        idx = next(iter(nodes.values()), {}).get("indices", {})
        rc, qc = idx.get("request_cache", EMPTY), idx.get("query_cache", EMPTY)
        return {
            "result_hits": int(rc.get("hit_count", -1)),
            "result_misses": int(rc.get("miss_count", -1)),
            "filter_hits": int(qc.get("hit_count", -1)),
            "filter_misses": int(qc.get("miss_count", -1)),
        }

    def effective(self, client: Client, index: str | None = None) -> dict[str, Any]:
        path = "/_nodes/_local?filter_path=nodes.*.os.allocated_processors,nodes.*.jvm.mem.heap_max_in_bytes,nodes.*.version,nodes.*.settings.node,nodes.*.settings.bootstrap"
        status, body = client.call(Request("GET", path))
        nodes: dict[str, Any] = json.loads(body).get("nodes", EMPTY) if status == 200 else EMPTY
        node = next(iter(nodes.values()), {})
        out: dict[str, Any] = {
            "system": "elasticsearch",
            "version": node.get("version"),
            "processors": node.get("os", {}).get("allocated_processors"),
            "heap_max_bytes": node.get("jvm", {}).get("mem", {}).get("heap_max_in_bytes"),
            "node_settings": node.get("settings", {}),
        }
        if index:
            status, body = client.call(Request("GET", f"/{index}/_settings?flat_settings=true"))
            if status == 200:
                out["index_settings"] = next(iter(json.loads(body).values()), EMPTY).get("settings", EMPTY)
        return out

    @override
    def assert_effective(self, eff: dict[str, Any], cpus: int) -> list[str]:
        problems = super().assert_effective(eff, cpus)
        index = eff.get("index_settings")
        if index is not None and index.get("index.number_of_shards") != "1":
            problems.append("es: index shards read back not 1")
        if index is not None and index.get("index.requests.cache.enable") != "false":
            problems.append("es: the shard request cache is not off")
        return problems


# ---------------------------------------------------------------------------------------------
# Quickwit (Q)


class Quickwit(System):
    name = "quickwit"
    commit_timeout_secs = 60  # T6 also runs 1
    max_partitions = 200  # the shared tenant index's partition count (Quickwit's default)

    def command(self, launch: Launch) -> tuple[list[str], dict[str, str]]:
        path = os.path.join(self.workdir, "quickwit.yaml")
        with open(path, "w") as fh:
            fh.write(
                render(
                    "quickwit.yaml",
                    port=launch.port,
                    grpc=launch.port + 1,
                    data=self.datadir,
                    cpu_millis=launch.cpus * 1000,
                    partial_cache="64MB" if launch.caches_on else "0",
                )
            )
        return [os.path.abspath(launch.binary), "run", "--config", path, *launch.extra], {"QW_DISABLE_TELEMETRY": "1"}

    def ready_request(self) -> Request:
        return Request("GET", "/health/readyz")

    def create_index(self, index: str, schema: Schema, bulk: bool = False) -> list[Request]:
        mapped: list[dict[str, Any]] = []
        for name, kind in schema.fields.items():
            if kind == "text":
                mapped.append({"name": name, "type": "text", "tokenizer": "default", "record": "position"})
            elif kind == "keyword":
                mapped.append({"name": name, "type": "text", "tokenizer": "raw", "fast": True})
            elif kind == "integer":
                mapped.append({"name": name, "type": "i64", "fast": True})
            else:
                mapped.append(
                    {
                        "name": name,
                        "type": "datetime",
                        "input_formats": ["unix_timestamp"],
                        "fast": True,
                        "fast_precision": "seconds",
                    }
                )
        conf = json.loads(render("quickwit-index.json", index=index))
        conf["indexing_settings"]["commit_timeout_secs"] = self.commit_timeout_secs
        conf["doc_mapping"]["field_mappings"] = mapped
        conf["search_settings"]["default_search_fields"] = [k for k, v in schema.fields.items() if v == "text"]
        if schema.time_field:
            conf["doc_mapping"]["timestamp_field"] = schema.time_field
        if schema.name == "tenants-shared":  # the documented multi-tenant shape: a partition key per tenant
            conf["doc_mapping"]["partition_key"] = "tenant"
            conf["doc_mapping"]["max_num_partitions"] = self.max_partitions
        return [Request("POST", "/api/v1/indexes", json.dumps(conf).encode())]

    @override
    def setup(self, client: Client, index: str, schema: Schema, bulk: bool = False) -> None:
        super().setup(client, index, schema, bulk)
        deadline = time.monotonic() + 60  # the index's ingest queue takes a moment to be assigned a shard
        while time.monotonic() < deadline:
            probe = Request("POST", f"/api/v1/{index}/ingest?commit=auto", b"\n", NDJSON_CT)
            if client.call(probe)[0] < 300:
                return
            time.sleep(0.25)
        raise SystemExit(f"quickwit: {index} never accepted an ingest request")

    def bulk_request(self, index: str, schema: Schema, lines: list[bytes], last: bool = False) -> Request:
        commit = "force" if last else "auto"
        return Request(
            "POST",
            f"/api/v1/{index}/ingest?commit={commit}",
            b"\n".join(ln.rstrip() for ln in lines) + b"\n",
            NDJSON_CT,
        )

    def parse_bulk(self, status: int, body: bytes, n: int) -> Bulk:
        if status >= 300:
            return Bulk(False, 0, f"HTTP {status}: {body[:200]!r}")
        doc = json.loads(body)
        got = int(doc.get("num_ingested_docs", 0))
        return Bulk(got == n and not doc.get("num_rejected_docs"), got, "" if got == n else f"ingested {got} of {n}")

    def commit_requests(self, index: str, bulk: bool = False) -> list[Request]:
        return []  # the last ingest forces the commit

    supports_delete = False

    @override
    def segments(self, client: Client, index: str) -> int | None:
        status, body = client.call(Request("GET", f"/api/v1/indexes/{index}/splits?split_states=Published"))
        if status != 200:
            return None
        doc = json.loads(body)
        return len(doc if isinstance(doc, list) else doc.get("splits", []))

    @override
    def floor_request(self, index: str) -> Request:
        return Request("GET", "/api/v1/version")

    @override
    def put_request(self, index: str, doc_id: str, doc: dict[str, Any]) -> Request:
        body = json.dumps({"id": doc_id, **doc}).encode() + b"\n"
        return Request("POST", f"/api/v1/{index}/ingest?commit=auto", body, NDJSON_CT)

    @override
    def dump(self, client: Client, index: str) -> list[tuple[str, int]]:
        out: list[tuple[str, int]] = []
        while True:
            body = json.dumps({"query": "*", "max_hits": 5000, "start_offset": len(out), "sort_by": "n"}).encode()
            status, raw = client.call(Request("POST", f"/api/v1/{index}/search", body))
            if status != 200:
                raise SystemExit(f"quickwit: dump {index}: HTTP {status}")
            hits = json.loads(raw)["hits"]
            out.extend((str(h["id"]), int(h["n"])) for h in hits)
            if len(hits) < 5000:
                return out

    @override
    def dump_stream(self, client: Client, index: str, stream: str) -> list[int]:
        out: list[int] = []
        while True:
            req = {"query": f'stream:"{stream}"', "max_hits": 5000, "start_offset": len(out), "sort_by": "seq"}
            status, raw = client.call(Request("POST", f"/api/v1/{index}/search", json.dumps(req).encode()))
            if status != 200:
                raise SystemExit(f"quickwit: dump of stream {stream} in {index}: HTTP {status}: {raw[:200]!r}")
            hits = json.loads(raw)["hits"]
            out.extend(int(h["seq"]) for h in hits)
            if len(hits) < 5000:
                return out

    def query_string(self, schema: Schema, q: Query) -> str:
        parts: list[str] = []
        if q.terms:
            terms = [clean_term(t) for t in q.terms]
            kind = schema.fields.get(q.field, "text")
            if q.mode == "phrase" and len(terms) > 1 and kind == "text":
                parts.append(f'{q.field}:"{" ".join(terms)}"')
            else:
                joiner = " OR " if q.mode == "any" else " AND "
                parts.append(
                    "(" + joiner.join(f"{q.field}:{t}" for t in terms) + ")"
                    if len(terms) > 1
                    else f"{q.field}:{terms[0]}"
                )
        if q.filter:
            parts.append(f'{q.filter[0]}:"{q.filter[1]}"')
        if q.range:
            fld, lo, hi = q.range
            conv = rfc3339 if schema.fields.get(fld) == "date" else int_text
            low = "*" if lo is None else conv(lo)
            high = "*" if hi is None else conv(hi)
            parts.append(f"{fld}:[{low} TO {high}]")
        return " AND ".join(parts) if parts else "*"

    def search_request(self, index: str, schema: Schema, q: Query) -> Request:
        body: dict[str, Any] = {
            "query": self.query_string(schema, q),
            "max_hits": 0 if q.agg or q.command == "count" else 10,
        }
        if q.command == "search":
            body["count_all"] = False
        if q.command != "count":
            if q.sort:  # Quickwit sorts descending by default and ascending with a minus
                body["sort_by"] = q.sort[0] if q.sort[1] == "desc" else "-" + q.sort[0]
            elif q.terms and not q.agg:
                body["sort_by"] = "_score"  # Q does not score unless asked
            if q.agg:
                kind, fld, interval = q.agg
                spec = {
                    "date_histogram": {"date_histogram": {"field": fld, "fixed_interval": f"{interval}s"}},
                    "terms": {"terms": {"field": fld, "size": 100}},
                    "stats": {"stats": {"field": fld}},
                }[kind]
                body["aggs"] = {"a": spec}
        if q.range and q.range[0] == schema.time_field:  # the documented split pruning on the timestamp field
            if q.range[1] is not None:
                body["start_timestamp"] = int(q.range[1])
            if q.range[2] is not None:
                body["end_timestamp"] = int(q.range[2]) + 1
        return Request("POST", f"/api/v1/{index}/search", json.dumps(body).encode())

    def parse_answer(self, q: Query, status: int, body: bytes) -> Answer:
        if status >= 300:
            return Answer(status, error=body[:200].decode("utf-8", "replace"))
        doc = json.loads(body)
        ans = Answer(status, int(doc["num_hits"]))
        agg = doc.get("aggregations", {}).get("a")
        if q.agg and agg is not None:
            if q.agg[0] == "stats":
                ans.aggs = {k: float(agg[k]) for k in ("count", "min", "max", "avg", "sum")}
            elif q.agg[0] == "date_histogram":
                ans.aggs = {bucket(int(b["key"]) // 1000): int(b["doc_count"]) for b in agg["buckets"]}
            else:
                ans.aggs = {bucket(b["key"]): int(b["doc_count"]) for b in agg["buckets"]}
        return ans

    def cache_counters(self, client: Client) -> dict[str, Any]:
        status, body = client.call(Request("GET", "/metrics"))
        hits: dict[str, int] = {}
        for ln in body.decode().splitlines() if status == 200 else ():
            if ln.startswith("quickwit_cache_cache_hits_total{") and 'component_name="' in ln:
                hits[ln.split('component_name="')[1].split('"')[0]] = int(float(ln.rsplit(" ", 1)[1]))
        return {
            "result_hits": hits.get("partial_request", -1),
            "filter_hits": hits.get("predicate", -1),
            "all_components": hits,
        }

    def effective(self, client: Client, index: str | None = None) -> dict[str, Any]:
        status, body = client.call(Request("GET", "/api/v1/version"))
        doc = json.loads(body) if status == 200 else {}
        status, body = client.call(Request("GET", "/api/v1/config"))
        conf = json.loads(body) if status == 200 else {}
        cap = str(conf.get("indexer_config", {}).get("cpu_capacity", ""))
        out: dict[str, Any] = {
            "system": "quickwit",
            "version": doc.get("build", {}).get("version"),
            "processors": int(cap[:-1]) // 1000 if cap.endswith("m") and cap[:-1].isdigit() else None,
            "runtime_cpus": doc.get("runtime", {}).get("num_cpus"),
            "partial_request_cache": conf.get("searcher_config", {}).get("partial_request_cache"),
        }
        if index:
            status, body = client.call(Request("GET", f"/api/v1/indexes/{index}"))
            if status == 200:
                conf = json.loads(body).get("index_config", {})
                out["indexing_settings"] = conf.get("indexing_settings")
                out["search_settings"] = conf.get("search_settings")
        return out

    @override
    def assert_effective(self, eff: dict[str, Any], cpus: int) -> list[str]:
        problems = super().assert_effective(eff, cpus)
        if eff.get("runtime_cpus") != cpus:
            problems.append(f"quickwit: runtime sees {eff.get('runtime_cpus')} CPUs, asked {cpus}")
        cache = eff.get("partial_request_cache") or {}
        if not (self.launch and self.launch.caches_on) and str(cache.get("capacity", "")).split(" ")[0] not in (
            "0",
            "0.0",
        ):
            problems.append(f"quickwit: partial request cache capacity {cache.get('capacity')}, asked 0")
        return problems


ADAPTERS: dict[str, type[System]] = {"xapiand": Xapiand, "es": Elasticsearch, "quickwit": Quickwit}


def make(system: str, target: str) -> System:
    try:
        return ADAPTERS[system](target)
    except KeyError:
        raise SystemExit(f"unknown system {system}; choose from {', '.join(ADAPTERS)}") from None


# ---------------------------------------------------------------------------------------------
# D8: correctness beside speed


@dataclass
class Member:
    label: str  # S, X, E or Q
    system: System
    client: Client
    index: str


def relative_gap(counts: list[int]) -> float:
    """The widest disagreement among counts, as a fraction of the largest (0 when all are 0)."""
    hi = max(counts)
    return 0.0 if hi == 0 else (hi - min(counts)) / hi


def check_systems(members: list[Member], schema: Schema, queries: list[Query], tol: float = 0.01) -> dict[str, Any]:
    """D8. Counts must agree within `tol`, S against X must agree on counts and top-10 order, aggregation buckets
    are compared bucket by bucket. Returns the kept and dropped queries, the listings and whether the run must stop."""
    kept: list[Query] = []
    dropped: list[dict[str, Any]] = []
    order_diffs: list[dict[str, Any]] = []
    agg_diffs: list[dict[str, Any]] = []
    errors: list[dict[str, Any]] = []
    by_class: dict[str, list[int]] = {}
    labels = {m.label: m for m in members}
    for q in queries:
        stats = by_class.setdefault(q.cls, [0, 0])
        stats[0] += 1
        answers: dict[str, Answer] = {}
        for m in members:
            answers[m.label] = m.system.run(m.client, m.index, schema, q.as_command("search_exact"))
        bad = {k: a for k, a in answers.items() if not a.ok}
        if bad:
            dropped.append(
                {"query": q.dumps(), "reason": "error", "errors": {k: [a.status, a.error] for k, a in bad.items()}}
            )
            errors.extend({"label": k, "query": q.label, "status": a.status, "error": a.error} for k, a in bad.items())
            stats[1] += 1
            continue
        counts = {k: int(a.count or 0) for k, a in answers.items()}
        gap = relative_gap(list(counts.values()))
        if gap > tol:
            dropped.append({"query": q.dumps(), "reason": f"counts differ by {gap:.1%}", "counts": counts})
            stats[1] += 1
            continue
        kept.append(q)
        if "S" in labels and "X" in labels:
            s, x = answers["S"], answers["X"]
            if s.count != x.count or (s.ids and x.ids and s.ids != x.ids):
                order_diffs.append({"query": q.label, "S": [s.count, s.ids], "X": [x.count, x.ids]})
        if q.agg:
            keyed = {k: a.aggs for k, a in answers.items()}
            ref = next(iter(keyed.values()))
            for k, aggs in keyed.items():
                if aggs != ref:
                    agg_diffs.append({"query": q.label, "system": k, "buckets": aggs, "against": ref})
    class_rates = {c: (v[1] / v[0] if v[0] else 0.0) for c, v in by_class.items()}
    return {
        "kept": [q.dumps() for q in kept],
        "dropped": dropped,
        "class_drop_rate": class_rates,
        "stop": any(rate > 0.05 for rate in class_rates.values()),
        "order_diffs": order_diffs,
        "agg_diffs": agg_diffs,
        "systems": [m.label for m in members],
    }


def probe_caches(members: list[Member], schema: Schema, queries: list[Query], runs: int = 10) -> dict[str, Any]:
    """A T4-style pass (every query `runs` times, as search and as count, range bounds varied per run) with each
    system's cache counters read before and after: the result-cache hits must not move. Filter-cache hits are
    reported."""
    report: dict[str, Any] = {}
    for m in members:
        before = m.system.cache_counters(m.client)
        errors = 0
        for run in range(runs):
            for q in queries:
                for cmd in ("search", "count"):
                    if not m.system.run(m.client, m.index, schema, q.vary(run).as_command(cmd)).ok:
                        errors += 1
        after = m.system.cache_counters(m.client)
        delta = {k: after[k] - before[k] for k in after if isinstance(after[k], int) and isinstance(before.get(k), int)}
        report[m.label] = {
            "before": before,
            "after": after,
            "delta": delta,
            "errors": errors,
            "ok": delta.get("result_hits", 1) == 0 and before.get("result_hits", -1) >= 0 and errors == 0,
        }
    return report


def counters_delta(m: Member, work: Any) -> dict[str, int]:
    before = m.system.cache_counters(m.client)
    work()
    after = m.system.cache_counters(m.client)
    return {k: after[k] - before[k] for k in after if isinstance(after[k], int) and isinstance(before.get(k), int)}


def probe_default(members: list[Member], schema: Schema, query: Query, repeats: int = 10) -> dict[str, Any]:
    """Probe part 1, second half: with the system's default cache settings the result-cache counters rise, so a 0
    under D13's settings is not a dead counter. E is asked for its request cache per request; Q (and the others) are
    probed in a separate run started with `Launch.caches_on`, and report `needs_default_launch` here."""
    report: dict[str, Any] = {}
    for m in members:
        if isinstance(m.system, Elasticsearch):
            m.system.request_cache = True
            try:
                delta = counters_delta(
                    m, lambda m=m: [m.system.run(m.client, m.index, schema, query) for _ in range(repeats)]
                )
            finally:
                m.system.request_cache = False
            report[m.label] = {"delta": delta, "rises": delta.get("result_hits", 0) > 0}
        elif isinstance(m.system, Quickwit) and m.system.launch and m.system.launch.caches_on:
            delta = counters_delta(
                m, lambda m=m: [m.system.run(m.client, m.index, schema, query) for _ in range(repeats)]
            )
            report[m.label] = {"delta": delta, "rises": delta.get("result_hits", 0) > 0}
        else:
            report[m.label] = {"needs_default_launch": isinstance(m.system, Quickwit), "rises": None}
    return report


def probe_ranges(members: list[Member], schema: Schema, template: Query, repeats: int = 10) -> dict[str, Any]:
    """Probe part 2: a range template ten times with one bound, then ten draws; the filter-cache hits of the first
    and of the second are reported (E's query cache and Q's predicate cache are on, as in normal operation). Quickwit
    0.9.3's predicate cache stores whole-query results only for `search_after` requests, plus per-split "term not here"
    entries (leaf.rs), so Q's expected hits are few and come from terms absent in a split, not from a repeated range."""
    report: dict[str, Any] = {}
    for m in members:
        same = counters_delta(
            m, lambda m=m: [m.system.run(m.client, m.index, schema, template) for _ in range(repeats)]
        )
        draws = counters_delta(
            m, lambda m=m: [m.system.run(m.client, m.index, schema, template.vary(r + 1)) for r in range(repeats)]
        )
        report[m.label] = {
            "one_bound_filter_hits": same.get("filter_hits"),
            "ten_draws_filter_hits": draws.get("filter_hits"),
        }
    return report


def probe_counts(members: list[Member], schema: Schema, query: Query) -> dict[str, Any]:
    """Probe part 3: for a term matching more than 10,000 documents, the top-10 mode gives E no exact total and Q an
    underestimate (or equal), and the count mode gives every system the same exact count."""
    top = {m.label: m.system.run(m.client, m.index, schema, query.as_command("search")) for m in members}
    exact = {m.label: m.system.run(m.client, m.index, schema, query.as_command("count")) for m in members}
    counts = {k: a.count for k, a in exact.items()}
    return {
        "top10_totals": {k: a.count for k, a in top.items()},
        "exact_counts": counts,
        "exact_counts_equal": len(set(counts.values())) == 1 and None not in counts.values(),
        "exact_total": next(iter(counts.values()), None),
        "over_10000": (next(iter(counts.values()), 0) or 0) > 10_000,
    }


def members_from(specs: list[str]) -> list[Member]:
    members: list[Member] = []
    for spec in specs:
        label, _, rest = spec.partition("=")
        system, _, where = rest.partition("@")
        target, _, index = where.partition("/")
        members.append(Member(label, make(system, target), Client(target), index))
    return members


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    chk = sub.add_parser("check", help="D8's count, order and aggregation checks on running systems")
    chk.add_argument("--queries", required=True)
    chk.add_argument("--schema", required=True)
    chk.add_argument("--sys", action="append", required=True, metavar="LABEL=SYSTEM@HOST:PORT/INDEX")
    chk.add_argument("--tolerance", type=float, default=0.01)
    chk.add_argument("--out")
    prb = sub.add_parser("probe", help="cache hit counters stay at 0 over a T4-style pass")
    prb.add_argument("--queries", required=True)
    prb.add_argument("--schema", required=True)
    prb.add_argument("--sys", action="append", required=True, metavar="LABEL=SYSTEM@HOST:PORT/INDEX")
    prb.add_argument("--runs", type=int, default=10)
    prb.add_argument("--out")
    args = ap.parse_args()
    schema = Schema.load(args.schema)
    members = members_from(args.sys)
    if args.cmd == "probe":
        qs = load_queries(args.queries)
        probe = probe_caches(members, schema, qs, args.runs)
        extra: dict[str, Any] = {
            "default_caches": probe_default(members, schema, next((x for x in qs if x.agg), qs[0])),
            "ranges": probe_ranges(members, schema, next((x for x in qs if x.range), qs[0])),
            "counts": probe_counts(members, schema, next((x for x in qs if x.terms), qs[0])),
            "effective": {m.label: m.system.effective(m.client, m.index) for m in members},
        }
        if args.out:
            with open(args.out, "w") as fh:
                json.dump({"result_caches": probe, **extra}, fh, indent=1)
        print(json.dumps(extra["counts"]))
        for label, res in probe.items():
            print(
                f"{label}: result-cache hits +{res['delta'].get('result_hits')}, filter-cache hits +{res['delta'].get('filter_hits')}, ok={res['ok']}"
            )
        return 0 if all(r["ok"] for r in probe.values()) else 1
    report = check_systems(members, schema, load_queries(args.queries), args.tolerance)
    text = json.dumps(report, indent=1)
    if args.out:
        with open(args.out, "w") as fh:
            fh.write(text + "\n")
    print(
        f"kept {len(report['kept'])}, dropped {len(report['dropped'])}, order differences {len(report['order_diffs'])}, "
        f"aggregation differences {len(report['agg_diffs'])}, stop={report['stop']}"
    )
    return 3 if report["stop"] else 0


if __name__ == "__main__":
    sys.exit(main())
