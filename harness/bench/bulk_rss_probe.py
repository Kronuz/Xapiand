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

"""Peak RSS of one `RESTORE /idx/?commit` (docs/perf/bulk-load-rss.md).

bulk_rss_probe.py --bin BIN --msgpack FILE --out DIR [--body array|stream] [--trials N] [--pause-at MB] [--msl]
bulk_rss_probe.py --bin BIN --msgpack FILE --out DIR --identity

Starts BIN fresh per trial (`--shards 1`, as harness bench bulk_load.sh's xapiand arm), sends the dump in 1 MiB
pieces and samples the server's RSS every 20 ms against the bytes sent. `--body stream` strips the dump's top-level
array header, so the same objects arrive as a stream of objects. `--pause-at MB` stops the server once (SIGSTOP)
when its RSS first reaches MB and captures `vmmap --summary` and `heap -s` (and, with `--msl`, which runs the
server under MallocStackLogging=lite, `malloc_history -callTree`, inverted and not); such a trial's time is not a timing.
After each response it captures `vmmap --summary` again. Writes DIR/<name>.json and DIR/<name>-*.txt.

`--identity` restores the dump into one server twice, as the array and as its objects streamed without it, and
checks that DUMP answers the same documents (ids and full contents) for both. Writes DIR/<name>-identity.json.
"""

from __future__ import annotations

import argparse
import hashlib
import http.client
import json
import os
import shutil
import signal
import socket
import statistics
import subprocess
import threading
import time
from collections.abc import Callable, Iterator
from typing import override


def free_port() -> int:
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port: int = s.getsockname()[1]
    s.close()
    return port


def rss_kb(pid: int) -> int:
    out = subprocess.run(
        ["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True, check=False
    ).stdout.strip()
    return int(out) if out else 0


def strip_array(body: bytes) -> tuple[bytes, int]:
    """The objects of a top-level MessagePack array as a stream, and their count."""
    b = body[0]
    if 0x90 <= b <= 0x9F:
        return body[1:], b & 0x0F
    if b == 0xDC:
        return body[3:], int.from_bytes(body[1:3], "big")
    if b == 0xDD:
        return body[5:], int.from_bytes(body[1:5], "big")
    raise SystemExit("the dump is not a top-level array")


def capture(path: str, cmd: list[str]) -> None:
    with open(path, "w") as f:
        subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=900, check=False)


class Sampler(threading.Thread):
    def __init__(self, pid: int, pause_at_kb: int, on_pause: Callable[[], None]) -> None:
        super().__init__(daemon=True)
        self.pid, self.pause_at_kb, self.on_pause = pid, pause_at_kb, on_pause
        self.stop = False
        self.sent = 0
        self.t0 = time.perf_counter()
        self.rows: list[tuple[float, int, int]] = []
        self.paused = 0.0

    @override
    def run(self) -> None:
        while not self.stop:
            r = rss_kb(self.pid)
            self.rows.append((time.perf_counter() - self.t0, r, self.sent))
            if self.pause_at_kb and r >= self.pause_at_kb:
                self.pause_at_kb = 0
                t = time.perf_counter()
                os.kill(self.pid, signal.SIGSTOP)
                try:
                    self.on_pause()
                finally:
                    os.kill(self.pid, signal.SIGCONT)
                self.paused += time.perf_counter() - t
            time.sleep(0.02)


def start(bin_path: str, root: str, env: dict[str, str]) -> tuple[subprocess.Popen[bytes], int]:
    """A fresh server in `root`, ready to answer."""
    shutil.rmtree(root, ignore_errors=True)
    data = os.path.join(root, "data")
    os.makedirs(data)
    port = free_port()
    p = subprocess.Popen(
        [bin_path, "--port", str(port), "--name", "bulk", "-D", data, "--shards", "1"],
        env=env,
        stdout=open(os.path.join(root, "out.log"), "wb"),
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    for _ in range(480):
        try:
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            c.request("GET", "/")
            if c.getresponse().status == 200:
                return p, port
        except OSError:
            pass
        if p.poll() is not None:
            raise SystemExit(f"server exited {p.returncode}")
        time.sleep(0.25)
    stop(p)
    raise SystemExit("server not ready")


def stop(p: subprocess.Popen[bytes]) -> None:
    try:
        os.killpg(p.pid, signal.SIGTERM)
        p.wait(timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        os.killpg(p.pid, signal.SIGKILL)


def restore(port: int, index: str, body: bytes) -> dict[str, object]:
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=1800)
    c.request(
        "RESTORE",
        f"/{index}/?commit",
        body=(body[i : i + (1 << 20)] for i in range(0, len(body), 1 << 20)),
        headers={
            "Content-Type": "application/x-msgpack",
            "Content-Length": str(len(body)),
            "Accept": "application/json",
        },
    )
    r = c.getresponse()
    resp = r.read()
    assert r.status in (200, 201, 204), (r.status, resp[:300])
    answer: dict[str, object] = json.loads(resp)
    return answer


def dump(port: int, index: str) -> bytes:
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=1800)
    c.request("DUMP", f"/{index}/", headers={"Accept": "application/json"})
    r = c.getresponse()
    resp = r.read()
    assert r.status == 200, (r.status, resp[:300])
    return resp


def identity(a: argparse.Namespace, raw: bytes, stream: bytes, n: int, ds: str) -> None:
    """The dump restored as its array and as its objects streamed: the same documents."""
    root = os.path.join(a.out, "run", f"{ds}-identity")
    p, port = start(a.bin, root, dict(os.environ))
    try:
        res: dict[str, object] = {"docs": n}
        docs: dict[str, list[dict[str, object]]] = {}
        for body_name, body in (("array", raw), ("stream", stream)):
            index = f"{ds}-{body_name}"
            answer = restore(port, index, body)
            dumped = dump(port, index)
            listed = json.loads(dumped)
            assert isinstance(listed, list), type(listed)
            docs[body_name] = listed
            res[body_name] = {
                "restore_total": answer.get("total"),
                "restore_indexed": answer.get("indexed"),
                "dumped": len(listed),
                "dump_bytes": len(dumped),
                "dump_sha256": hashlib.sha256(dumped).hexdigest(),
                "contents_sha256": hashlib.sha256(json.dumps(listed, sort_keys=True).encode()).hexdigest(),
            }
        by_id = {k: {json.dumps(d.get("_id"), sort_keys=True): d for d in v} for k, v in docs.items()}
        res["ids"] = len(by_id["array"])
        res["same_ids"] = by_id["array"].keys() == by_id["stream"].keys()
        res["same_contents"] = docs["array"] == docs["stream"]
        res["differing"] = sum(1 for k, d in by_id["array"].items() if by_id["stream"].get(k) != d)
        print(f"{ds} identity: " + json.dumps(res), flush=True)
        with open(os.path.join(a.out, f"{ds}-identity.json"), "w") as f:
            json.dump(res, f, indent=1)
        assert len(docs["array"]) == n, f"dumped {len(docs['array'])}, wanted {n}"
        assert res["ids"] == n, f"{res['ids']} distinct ids, wanted {n}"
        assert res["same_ids"] and res["same_contents"] and res["differing"] == 0, "the two restores differ"
    finally:
        stop(p)
        shutil.rmtree(root, ignore_errors=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--msgpack", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--body", choices=["array", "stream"], default="array")
    ap.add_argument("--trials", type=int, default=1)
    ap.add_argument("--pause-at", type=int, default=0, help="MB")
    ap.add_argument("--msl", action="store_true")
    ap.add_argument("--name", default=None)
    ap.add_argument("--identity", action="store_true")
    a = ap.parse_args()

    raw = open(a.msgpack, "rb").read()
    body, n = strip_array(raw)
    ds = os.path.basename(a.msgpack).removeprefix("es-").removesuffix(".msgpack")
    if a.identity:
        os.makedirs(a.out, exist_ok=True)
        identity(a, raw, body, n, ds)
        return
    if a.body == "array":
        body = raw
    name = a.name or f"{ds}-{a.body}" + ("-msl" if a.msl else "") + (f"-pause{a.pause_at}" if a.pause_at else "")
    os.makedirs(a.out, exist_ok=True)
    rows_out = []
    for trial in range(a.trials):
        root = os.path.join(a.out, "run", name)
        env = dict(os.environ)
        if a.msl:
            env["MallocStackLogging"] = "lite"
        p, port = start(a.bin, root, env)
        try:
            if a.msl:
                time.sleep(3.0)  # lite stack logging starts some time after launch
            idle = rss_kb(p.pid)
            prefix = os.path.join(a.out, f"{name}-t{trial}")

            def on_pause(pid: int = p.pid, prefix: str = prefix) -> None:
                mh = [
                    "malloc_history",
                    str(pid),
                    "-callTree",
                    "-ignoreThreads",
                    "-collapseRecursion",
                    "-consolidateAllBySymbol",
                ]
                if a.msl:  # first: vmmap and heap may reset lite stack logging
                    capture(f"{prefix}-pause-mh.txt", [*mh, "-invert"])
                    capture(f"{prefix}-pause-mh-top.txt", mh)
                capture(f"{prefix}-pause-vmmap.txt", ["vmmap", "--summary", str(pid)])
                capture(f"{prefix}-pause-heap.txt", ["heap", "-s", "-H", str(pid)])

            s = Sampler(p.pid, a.pause_at * 1024, on_pause)

            def pieces(s: Sampler = s) -> Iterator[bytes]:
                for i in range(0, len(body), 1 << 20):
                    piece = body[i : i + (1 << 20)]
                    yield piece
                    s.sent += len(piece)

            s.start()
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=1800)
            t = time.perf_counter()
            c.request(
                "RESTORE",
                f"/{ds}/?commit",
                body=pieces(),
                headers={"Content-Type": "application/x-msgpack", "Content-Length": str(len(body))},
            )
            sent_at = time.perf_counter() - s.t0
            r = c.getresponse()
            resp = r.read()
            e = time.perf_counter() - t
            done_at = time.perf_counter() - s.t0
            time.sleep(1.0)
            s.stop = True
            s.join()
            assert r.status in (200, 201, 204), (r.status, resp[:300])
            capture(f"{prefix}-after-vmmap.txt", ["vmmap", "--summary", str(p.pid)])
            after = rss_kb(p.pid)
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
            c.request("GET", f"/{ds}/:count")
            got = json.loads(c.getresponse().read()).get("total", -1)
            assert got == n, f"counted {got}, wanted {n}"
            peak = max(s.rows, key=lambda row: row[1])
            res = {
                "trial": trial,
                "seconds": e - s.paused,
                "paused": s.paused > 0,
                "idle_mb": idle / 1024,
                "peak_mb": peak[1] / 1024,
                "peak_at_s": peak[0],
                "peak_sent_mb": peak[2] / 2**20,
                "body_sent_at_s": sent_at,
                "response_at_s": done_at,
                "after_1s_mb": after / 1024,
                "body_mb": len(body) / 2**20,
                "docs": n,
            }
            with open(f"{prefix}-timeline.csv", "w") as f:
                f.write("t_s,rss_mb,sent_mb\n")
                f.writelines(f"{row[0]:.3f},{row[1] / 1024:.1f},{row[2] / 2**20:.1f}\n" for row in s.rows)
            print(
                f"{name} t{trial}: "
                + json.dumps({k: (round(v, 3) if isinstance(v, float) else v) for k, v in res.items()}),
                flush=True,
            )
            rows_out.append(res)
        finally:
            stop(p)
            shutil.rmtree(root, ignore_errors=True)
    summary = {
        "name": name,
        "trials": rows_out,
        "median_seconds": statistics.median(r["seconds"] for r in rows_out),
        "median_peak_mb": statistics.median(r["peak_mb"] for r in rows_out),
    }
    with open(os.path.join(a.out, f"{name}.json"), "w") as f:
        json.dump(summary, f, indent=1)
    print(f"{name}: median {summary['median_seconds']:.3f} s, peak {summary['median_peak_mb']:.0f} MB", flush=True)


if __name__ == "__main__":
    main()
