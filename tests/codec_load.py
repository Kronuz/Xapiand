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

"""codec_load.py OLD NEW ASSETS FILES SHA256 [ROUNDS]: index size, restore rate and numeric query throughput (design T7).

Runs each build on a fresh data directory, interleaved old, new, new, old, ... for ROUNDS rounds (default 3), and per run:

  restore  RESTORE of the accounts corpus repeated 10 times and the frozen 20,000-file corpus, documents per second
  size     bytes of the data directory after a clean stop (every index closed and committed)
  sort     searches sorted on a number or a datetime, per second, 8 clients for 15 seconds
  range    numeric and datetime range searches, per second, 8 clients for 15 seconds

Prints one JSON line per run, then each measure's median per build, new/old, and the A/A band (the old runs'
spread over their median). Exits 1 when a measure is slower than old by more than the band.

  python3 tests/codec_load.py build-old/bin/xapiand build/bin/xapiand site/public/assets \
      docs/design/sortable-encoding/files.ndjson.gz 27dfeab6...
"""

from __future__ import annotations

import gzip
import hashlib
import http.client
import json
import os
import random
import shutil
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from typing import Any

PORT = int(os.environ.get("PORT", "18977"))
SCRATCH = os.environ.get("LOAD_TMP") or tempfile.mkdtemp(prefix="codec_load_", dir=os.path.expanduser("~/.scratch"))
CLIENTS = 8
SECONDS = 15
REPEAT = 10  # the accounts corpus is 1,000 documents


def call(method: str, path: str, body: bytes | str | None = None, ctype: str = "application/json") -> tuple[int, Any]:
    conn = http.client.HTTPConnection("localhost", PORT, timeout=300)
    conn.request(method, path, body=body, headers={"Content-Type": ctype, "Accept": "application/json"})
    response = conn.getresponse()
    data = response.read()
    conn.close()
    try:
        return response.status, json.loads(data)
    except ValueError:
        return response.status, data


def corpora(assets: str, files: str, digest: str) -> dict[str, str]:
    with gzip.open(files, "rb") as f:
        raw = f.read()
    if hashlib.sha256(raw).hexdigest() != digest:
        sys.exit(f"{files}: sha256 differs from {digest}")
    with open(os.path.join(assets, "accounts.ndjson")) as f:
        lines = [line for line in f.read().splitlines() if line.strip()]
    accounts = "".join(f"{line}\n" for _ in range(REPEAT) for line in lines)
    return {"accounts": accounts, "files": raw.decode()}


def queries() -> dict[str, list[tuple[str, str]]]:
    """The sort and range mixes, as (index, body): the same seeded list for every run."""
    rnd = random.Random(20261116)
    sort = [
        ("files", json.dumps({"_sort": order + name, "_limit": 10}))
        for name in ("size", "mtime")
        for order in ("", "-")
    ] + [("accounts", json.dumps({"_sort": order + "balance", "_limit": 10})) for order in ("", "-")]
    ranges: list[tuple[str, str]] = []
    for _ in range(64):
        lo = rnd.randrange(0, 1 << 20)
        ranges.append(("files", json.dumps({"_query": {"size": {"_in": {"_range": {"_from": lo, "_to": lo * 4}}}}})))
        day = rnd.randrange(1, 28)
        mtime = {"_from": f"2026-10-{day:02d}T00:00:00", "_to": f"2026-10-{day:02d}T23:59:59.999999"}
        ranges.append(("files", json.dumps({"_query": {"mtime": {"_in": {"_range": mtime}}}})))
        b = rnd.uniform(1000, 3500)
        ranges.append(
            ("accounts", json.dumps({"_query": {"balance": {"_in": {"_range": {"_from": b, "_to": b + 200}}}}}))
        )
    return {"sort": sort, "range": ranges}


def throughput(mix: list[tuple[str, str]]) -> float:
    """Searches per second of CLIENTS clients cycling through mix for SECONDS; a failed search stops the run."""
    done = [0] * CLIENTS
    failed: list[str] = []
    stop = time.monotonic() + SECONDS

    def client(n: int) -> None:
        conn = http.client.HTTPConnection("localhost", PORT, timeout=60)
        i = n
        while time.monotonic() < stop and not failed:
            index, body = mix[i % len(mix)]
            conn.request("SEARCH", f"/{index}/", body=body, headers={"Content-Type": "application/json"})
            response = conn.getresponse()
            response.read()
            if response.status != 200:
                failed.append(f"{response.status} {body}")
            done[n] += 1
            i += CLIENTS
        conn.close()

    threads = [threading.Thread(target=client, args=(n,)) for n in range(CLIENTS)]
    begin = time.monotonic()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    if failed:
        sys.exit(f"search failed: {failed[0][:300]}")
    return sum(done) / (time.monotonic() - begin)


def dir_bytes(path: str) -> int:
    return sum(os.path.getsize(os.path.join(d, f)) for d, _, names in os.walk(path) for f in names)


def run(binary: str, data: dict[str, str], mixes: dict[str, list[tuple[str, str]]]) -> dict[str, float]:
    data_dir = os.path.join(SCRATCH, "data")
    shutil.rmtree(data_dir, ignore_errors=True)
    node = subprocess.Popen(
        [binary, "--port", str(PORT), "--name", "codecload", "-D", data_dir],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        for _ in range(120):
            try:
                if call("GET", "/")[0] == 200:
                    break
            except OSError:
                time.sleep(0.5)
        schema = {"_schema": {"_type": "object", "size": {"_type": "integer"}, "mtime": {"_type": "datetime"}}}
        status, body = call("PUT", "/files/", json.dumps(schema))
        assert status in (200, 201, 204), (binary, status, body)
        docs = sum(text.count("\n") for text in data.values())
        begin = time.monotonic()
        for index, text in data.items():
            status, body = call("RESTORE", f"/{index}/?commit", text, "application/x-ndjson")
            assert status in (200, 201, 204), (binary, index, status, body)
        restore = docs / (time.monotonic() - begin)
        rates = {name: throughput(mix) for name, mix in mixes.items()}
    finally:
        node.terminate()
        node.wait()
    # A clean stop closes and commits every index, so the size does not depend on when it is read.
    return {"restore": restore, "size": dir_bytes(data_dir), **rates}


def main() -> None:
    old, new, assets, files, digest = sys.argv[1:6]
    rounds = int(sys.argv[6]) if len(sys.argv) > 6 else 3
    data = corpora(assets, files, digest)
    mixes = queries()
    results: dict[str, list[dict[str, float]]] = {"old": [], "new": []}
    try:
        for r in range(rounds):
            order = ("old", "new") if r % 2 == 0 else ("new", "old")
            for side in order:
                result = run(os.path.abspath(old if side == "old" else new), data, mixes)
                results[side].append(result)
                print(
                    json.dumps({"round": r + 1, "build": side, **{k: round(v, 1) for k, v in result.items()}}),
                    flush=True,
                )
    finally:
        shutil.rmtree(SCRATCH, ignore_errors=True)
    slower = []
    for measure in ("restore", "size", "sort", "range"):
        o = [r[measure] for r in results["old"]]
        n = [r[measure] for r in results["new"]]
        mo, mn = statistics.median(o), statistics.median(n)
        band = (max(o) - min(o)) / mo
        # Size is better smaller; the rates are better larger.
        worse = (mn - mo) / mo if measure == "size" else (mo - mn) / mo
        print(f"{measure}: old {mo:.1f} new {mn:.1f} new/old {mn / mo:.3f} A/A band {band:.3f}")
        if worse > band:
            slower.append(measure)
    print("worse than old beyond the A/A band: " + (", ".join(slower) or "none"))
    sys.exit(1 if slower else 0)


if __name__ == "__main__":
    main()
