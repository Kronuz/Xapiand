#!/usr/bin/env python3
"""restore_large_check.py BIN [--mib N] -- a RESTORE past the 16 MiB buffered-body cap, chunked and with Content-Length.

On fresh nodes: restore N MiB (default 72) of NDJSON into a source index, DUMP it, then restore that dump into a
fresh node once with `Transfer-Encoding: chunked` and once with a `Content-Length` (and a MsgPack stream chunked).
Each restore must answer 200, count every document, and hold the same documents as the dump (all of them compared,
and a sample read back by id). The node's peak RSS is sampled during each restore and must stay under the body's size
less a margin: a buffering node holds the whole body (and a copy being parsed), a streaming one holds a few chunks.

  python3 harness/restore_large_check.py build/bin/xapiand [--mib 72]
"""
import http.client
import json
import os
import random
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import threading
import time
from collections.abc import Iterator

BIN = os.path.abspath(sys.argv[1])
MIB = int(sys.argv[sys.argv.index("--mib") + 1]) if "--mib" in sys.argv else 72
PORT = int(os.environ.get("PORT", "18841"))
SCRATCH = tempfile.mkdtemp(prefix="restore_large_", dir=os.environ.get("HARNESS_TMP") or os.path.expanduser("~/.scratch"))
CHUNK = 64 * 1024
FAILED: list[str] = []


def check(ok: bool, what: str) -> None:
    print(("ok   " if ok else "FAIL ") + what, flush=True)
    if not ok:
        FAILED.append(what)


def document(i: int) -> dict[str, object]:
    rng = random.Random(i)
    words = " ".join(f"w{rng.randrange(1 << 20):x}" for _ in range(60))
    return {"_id": i, "user": f"u{i % 997}", "n": i * 3, "message": f"message {i}: {words}"}


def ndjson_file(path: str, mib: int) -> int:
    n = 0
    with open(path, "wb") as f:
        while f.tell() < mib << 20:
            n += 1
            f.write(json.dumps(document(n), separators=(",", ":")).encode() + b"\n")
    return n


def msgpack_file(path: str, n: int) -> None:
    def pack(o: object) -> bytes:
        if isinstance(o, dict):
            return b"\xde" + struct.pack(">H", len(o)) + b"".join(pack(k) + pack(v) for k, v in o.items())
        if isinstance(o, str):
            e = o.encode()
            return b"\xdb" + struct.pack(">I", len(e)) + e
        assert isinstance(o, int)
        return b"\xd3" + struct.pack(">q", o)

    with open(path, "wb") as f:
        for i in range(1, n + 1):
            f.write(pack(document(i)))


class Node:
    def __init__(self, name: str) -> None:
        self.dir = os.path.join(SCRATCH, name)
        self.log = open(os.path.join(SCRATCH, name + ".log"), "ab")
        self.proc = subprocess.Popen([BIN, "--port", str(PORT), "--name", name, "-D", self.dir], stdout=self.log, stderr=self.log)
        for _ in range(120):
            try:
                self.call("GET", "/")
                return
            except OSError:
                time.sleep(0.5)
        raise SystemExit("the node did not start")

    def call(self, method: str, path: str, **kw: object) -> tuple[int, bytes]:
        c = http.client.HTTPConnection("localhost", PORT, timeout=900)
        c.request(method, path, **kw)  # type: ignore[arg-type]
        r = c.getresponse()
        return r.status, r.read()

    def rss_kib(self) -> int:
        out = subprocess.run(["ps", "-o", "rss=", "-p", str(self.proc.pid)], capture_output=True, text=True).stdout.strip()
        return int(out) if out else 0

    def stop(self) -> None:
        self.proc.send_signal(signal.SIGKILL)
        self.proc.wait()
        self.log.close()


def chunks(path: str) -> Iterator[bytes]:
    with open(path, "rb") as f:
        while block := f.read(CHUNK):
            yield block


def restore(node: Node, index: str, path: str, ctype: str, chunked: bool) -> tuple[int, int, dict[str, object]]:
    """Returns (HTTP status, peak RSS growth in KiB, the response)."""
    peak = [node.rss_kib()]
    base = peak[0]
    done = threading.Event()

    def sample() -> None:
        while not done.wait(0.05):
            peak[0] = max(peak[0], node.rss_kib())

    t = threading.Thread(target=sample)
    t.start()
    try:
        headers = {"Content-Type": ctype}
        if chunked:
            headers["Transfer-Encoding"] = "chunked"
            status, body = node.call("RESTORE", f"/{index}/", body=chunks(path), headers=headers, encode_chunked=True)
        else:
            headers["Content-Length"] = str(os.path.getsize(path))
            with open(path, "rb") as f:
                status, body = node.call("RESTORE", f"/{index}/", body=f, headers=headers)
    finally:
        done.set()
        t.join()
    try:
        response = json.loads(body)
    except ValueError:
        response = {"raw": body[:200].decode(errors="replace")}
    return status, peak[0] - base, response


def dump_docs(node: Node, index: str, path: str | None = None) -> dict[int, dict[str, object]]:
    status, body = node.call("DUMP", f"/{index}/", headers={"Accept": "application/x-ndjson"})
    assert status == 200, status
    if path:
        with open(path, "wb") as f:
            f.write(body)
    docs: dict[int, dict[str, object]] = {}
    for line in body.splitlines():
        d = json.loads(line)
        docs[int(d["_id"])] = {k: v for k, v in d.items() if k not in ("_version",)}
    return docs


def verify(node: Node, index: str, want: dict[int, dict[str, object]], what: str) -> None:
    status, body = node.call("GET", f"/{index}/:count")
    total = json.loads(body).get("total", -1) if status == 200 else -1
    check(total == len(want), f"{what}: counted {total} of {len(want)} documents")
    got = dump_docs(node, index)
    check(got == want, f"{what}: the dump of the restored index equals the source dump ({len(got)} documents)")
    rng = random.Random(7)
    bad = 0
    for i in rng.sample(sorted(want), 200):
        status, body = node.call("GET", f"/{index}/{i}", headers={"Accept": "application/json"})
        d = json.loads(body) if status == 200 else {}
        bad += not (status == 200 and d.get("message") == want[i]["message"] and d.get("n") == want[i]["n"])
    check(bad == 0, f"{what}: 200 sampled documents read back by id ({bad} differ)")


def main() -> None:
    nd = os.path.join(SCRATCH, "source.ndjson")
    n = ndjson_file(nd, MIB)
    size = os.path.getsize(nd)
    print(f"source: {n} documents, {size / (1 << 20):.1f} MiB of NDJSON", flush=True)
    check(size > 16 << 20, "the body is past the 16 MiB buffered-body cap")

    # The source index, then its dump (the file the restores below replay).
    node = Node("source")
    status, _, resp = restore(node, "src", nd, "application/x-ndjson", chunked=False)
    check(status == 200 and resp.get("indexed") == n, f"source restore: HTTP {status}, indexed {resp.get('indexed')} of {n}")
    dump = os.path.join(SCRATCH, "dump.ndjson")
    want = dump_docs(node, "src", dump)
    check(len(want) == n, f"the dump holds {len(want)} of {n} documents")
    node.stop()
    dump_size = os.path.getsize(dump)

    mp = os.path.join(SCRATCH, "source.msgpack")
    mp_n = min(n, (MIB << 20) // 400)
    msgpack_file(mp, mp_n)
    mp_size = os.path.getsize(mp)

    rows = [
        ("chunked NDJSON", dump, "application/x-ndjson", True, want, dump_size),
        ("Content-Length NDJSON", dump, "application/x-ndjson", False, want, dump_size),
        ("chunked MsgPack", mp, "application/x-msgpack", True, {i: want[i] for i in range(1, mp_n + 1)}, mp_size),
    ]
    for name, path, ctype, chunked, expect, body_size in rows:
        node = Node("restore")
        status, growth, resp = restore(node, "dst", path, ctype, chunked)
        mib = body_size / (1 << 20)
        check(status == 200 and resp.get("indexed") == len(expect), f"{name} ({mib:.1f} MiB): HTTP {status}, indexed {resp.get('indexed')} of {len(expect)}")
        # Peak growth of the node's resident set: under 3/4 of the body (a buffering node holds all of it, twice over).
        check(growth * 1024 < body_size * 3 // 4, f"{name}: peak RSS grew {growth / 1024:.1f} MiB for a {mib:.1f} MiB body")
        print(f"     {name}: peak RSS growth {growth / 1024:.1f} MiB", flush=True)
        if status == 200:
            verify(node, "dst", expect, name)
        node.stop()
        shutil.rmtree(node.dir, ignore_errors=True)

    if not FAILED:
        shutil.rmtree(SCRATCH, ignore_errors=True)
    sys.exit(1 if FAILED else 0)


main()
