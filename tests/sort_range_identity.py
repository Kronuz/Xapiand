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

"""sort_range_identity.py OLD NEW ASSETS FILES SHA256: the switch keeps every sort and range answer (design T5).

Restores accounts and cities (ASSETS, site/public/assets) and the frozen file-metadata corpus FILES (a gzipped
ndjson whose content must have sha256 SHA256) on each binary, then runs, with a fixed seed, ascending and
descending sorts of every numeric and datetime field, 1,000 ranges over them (bounds drawn from the values and
between them, integer and float bounds on every numeric type) and a geo set over cities (circles, and sorts
by distance). An answer is the hit ids in order (a sort) or as a set (a range).

Each NEW answer must equal the OLD one, or differ only where brute force says OLD was wrong and NEW is right;
geo answers have no brute force here and must be equal. Prints the counts and exits 1 on any other answer.

  python3 tests/sort_range_identity.py old/bin/xapiand build/bin/xapiand site/public/assets files.ndjson.gz SHA
"""

from __future__ import annotations

import calendar
import gzip
import hashlib
import http.client
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any

PORT = int(os.environ.get("PORT", "18976"))
SCRATCH = os.environ.get("IDENTITY_TMP") or tempfile.mkdtemp(prefix="identity_", dir=os.path.expanduser("~/.scratch"))
SEED = 20261116
RANGES = 1000
TIMEOUT = 600  # seconds per request: the slowest search takes about 2 s on a loaded host

Number = int | float
Query = tuple[str, str, dict[str, Any]]  # (kind, index, body); kind: sort, range or geo


def call(method: str, path: str, body: object = None, ctype: str = "application/json") -> tuple[int, Any]:
    raw = body if isinstance(body, (bytes, str)) or body is None else json.dumps(body)
    conn = http.client.HTTPConnection("localhost", PORT, timeout=TIMEOUT)
    conn.request(method, path, body=raw, headers={"Content-Type": ctype, "Accept": "application/json"})
    response = conn.getresponse()
    data = response.read()
    try:
        return response.status, json.loads(data)
    except ValueError:
        return response.status, data


def stamp_us(text: str) -> int:
    """A corpus datetime (YYYY-MM-DDTHH:MM:SS.ffffff, UTC) as integer microseconds."""
    whole, _, frac = text.partition(".")
    seconds = calendar.timegm(time.strptime(whole, "%Y-%m-%dT%H:%M:%S"))
    return seconds * 1_000_000 + int((frac or "0").ljust(6, "0"))


def stamp(us: int) -> str:
    seconds, micros = divmod(us, 1_000_000)
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(seconds)) + f".{micros:06d}"


def load(assets: str, files: str, digest: str) -> dict[str, tuple[str, list[dict[str, Any]]]]:
    with gzip.open(files, "rb") as f:
        raw = f.read()
    got = hashlib.sha256(raw).hexdigest()
    if got != digest:
        sys.exit(f"{files}: sha256 {got}, expected {digest}")
    corpora: dict[str, tuple[str, list[dict[str, Any]]]] = {}
    for index in ("accounts", "cities"):
        with open(os.path.join(assets, f"{index}.ndjson")) as f:
            text = f.read()
        corpora[index] = (text, [json.loads(line) for line in text.splitlines() if line.strip()])
    text = raw.decode()
    corpora["files"] = (text, [json.loads(line) for line in text.splitlines() if line.strip()])
    return corpora


# The fields compared, by index: (name, kind), kind being how brute force reads a value.
FIELDS = {
    "accounts": [("accountNumber", "int"), ("balance", "float"), ("age", "int")],
    "cities": [("population", "int"), ("_id", "int")],
    "files": [("size", "int"), ("mtime", "datetime")],
}


def value(doc: dict[str, Any], name: str, kind: str) -> Number:
    v = doc[name]
    return stamp_us(v) if kind == "datetime" else v


def bound(v: Number, kind: str) -> object:
    return stamp(int(v)) if kind == "datetime" else v


def everything(name: str, count: int) -> dict[str, Any]:
    return {"_query": {name: {"_in": {"_range": {}}}}, "_limit": count, "_check_at_least": count}


def queries(corpora: dict[str, tuple[str, list[dict[str, Any]]]]) -> list[Query]:
    rnd = random.Random(SEED)
    out: list[Query] = []
    for index, fields in FIELDS.items():
        count = len(corpora[index][1])
        for name, _ in fields:
            for order in ("", "-"):
                out.append(("sort", index, {**everything(name, count), "_sort": order + name}))
    pool = [(index, name, kind) for index, fields in FIELDS.items() for name, kind in fields]
    for _ in range(RANGES):
        index, name, kind = rnd.choice(pool)
        values = sorted({value(d, name, kind) for d in corpora[index][1] if name in d})
        lo, hi = sorted((rnd.choice(values), rnd.choice(values)))
        shape = rnd.randrange(6)
        if shape == 1:  # bounds between values: a fraction for numbers, a microsecond for datetimes
            lo, hi = (lo - 1, hi + 1) if kind == "datetime" else (lo - 0.5, hi + 0.5)
        elif shape == 2:
            lo = None
        elif shape == 3:
            hi = None
        elif shape == 4 and kind != "datetime":
            lo, hi = float(lo), float(hi)  # float bounds equal to integer values
        rng: dict[str, object] = {}
        if lo is not None:
            rng["_from"] = bound(lo, kind)
        if hi is not None:
            rng["_to"] = bound(hi, kind)
        count = len(corpora[index][1])
        body = {"_query": {name: {"_in": {"_range": rng}}}, "_limit": count, "_check_at_least": count}
        out.append(("range", index, body))
    cities = corpora["cities"][1]
    for _ in range(50):
        center = rnd.choice(cities)["location"]["_point"]
        point = {"_latitude": center["_latitude"], "_longitude": center["_longitude"]}
        radius = rnd.choice((5000, 20000, 100000, 500000))
        body = {
            "_query": {"location": {"_in": {"_circle": {**point, "_radius": radius}}}},
            "_sort": {"location": {"_order": rnd.choice(("asc", "desc")), "_value": {"_point": point}}},
            "_limit": len(cities),
            "_check_at_least": len(cities),
        }
        out.append(("geo", "cities", body))
    return out


def run(binary: str, corpora: dict[str, tuple[str, list[dict[str, Any]]]], plan: list[Query]) -> list[Any]:
    data_dir = os.path.join(SCRATCH, "data")
    shutil.rmtree(data_dir, ignore_errors=True)
    node = subprocess.Popen(
        ["timeout", "1800", binary, "--port", str(PORT), "--name", "identity", "-D", data_dir],
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
        schema = {"_type": "object", "size": {"_type": "integer"}, "mtime": {"_type": "datetime"}}
        status, data = call("PUT", "/files/", {"_schema": schema})
        assert status in (200, 201, 204), (binary, status, data)
        for index, (text, _) in corpora.items():
            status, data = call("RESTORE", f"/{index}/?commit", text, "application/x-ndjson")
            assert status in (200, 201, 204), (binary, index, status, data)
        answers: list[Any] = []
        slowest = (0.0, "")
        for kind, index, body in plan:
            begin = time.monotonic()
            try:
                status, data = call("SEARCH", f"/{index}/?volatile", body)
            except TimeoutError:
                # A search that never answers is a wrong answer named in the report, not a crash.
                print(f"TIMEOUT {binary} /{index}/ {json.dumps(body)[:300]}", file=sys.stderr, flush=True)
                answers.append(("timeout", TIMEOUT))
                continue
            slowest = max(slowest, (time.monotonic() - begin, f"/{index}/ {json.dumps(body)[:200]}"))
            if status != 200 or not isinstance(data, dict):
                answers.append(("error", status))
                continue
            ids = [h["_id"] for h in data["hits"]]
            answers.append(ids if kind != "range" else sorted(ids))
        print(f"{binary}: slowest search {slowest[0]:.2f} s: {slowest[1]}", flush=True)
        return answers
    finally:
        node.terminate()
        node.wait()


def truth(corpora: dict[str, tuple[str, list[dict[str, Any]]]], query: Query) -> Any:
    """Brute force: a range's id set, or a sort's value sequence."""
    kind, index, body = query
    docs = corpora[index][1]
    if kind == "sort":
        name = body["_sort"].lstrip("-")
        field_kind = dict(FIELDS[index])[name]
        return sorted((value(d, name, field_kind) for d in docs), reverse=body["_sort"].startswith("-"))
    name, spec = next(iter(body["_query"].items()))
    field_kind = dict(FIELDS[index])[name]
    rng = spec["_in"]["_range"]
    lo, hi = rng.get("_from"), rng.get("_to")
    if field_kind == "datetime":
        lo = None if lo is None else stamp_us(lo)
        hi = None if hi is None else stamp_us(hi)
    return sorted(
        d["_id"]
        for d in docs
        if (lo is None or value(d, name, field_kind) >= lo) and (hi is None or value(d, name, field_kind) <= hi)
    )


def as_truth(corpora: dict[str, tuple[str, list[dict[str, Any]]]], query: Query, answer: Any) -> Any:
    """An answer in truth()'s form: a sort's ids become their values (ties may come in any order)."""
    kind, index, body = query
    if kind != "sort" or not isinstance(answer, list):
        return answer
    name = body["_sort"].lstrip("-")
    field_kind = dict(FIELDS[index])[name]
    by_id = {d["_id"]: d for d in corpora[index][1]}
    return [value(by_id[i], name, field_kind) for i in answer]


def main() -> None:
    old, new, assets, files, digest = sys.argv[1:6]
    corpora = load(assets, files, digest)
    for _, docs in corpora.values():
        for i, d in enumerate(docs, 1):
            d.setdefault("_id", i)  # RESTORE numbers documents without an _id from 1, in order
    plan = queries(corpora)
    try:
        before = run(os.path.abspath(old), corpora, plan)
        after = run(os.path.abspath(new), corpora, plan)
    finally:
        shutil.rmtree(SCRATCH, ignore_errors=True)
    counts = {"equal": 0, "old wrong, new right": 0, "wrong": 0}
    for query, a, b in zip(plan, before, after):
        if a == b or (query[0] == "sort" and as_truth(corpora, query, a) == as_truth(corpora, query, b)):
            counts["equal"] += 1
            continue
        right = query[0] != "geo" and as_truth(corpora, query, b) == truth(corpora, query)
        old_right = query[0] != "geo" and as_truth(corpora, query, a) == truth(corpora, query)
        if right and not old_right:
            counts["old wrong, new right"] += 1
            continue
        counts["wrong"] += 1
        if counts["wrong"] <= 5:
            print(f"WRONG {query[0]} /{query[1]}/ {json.dumps(query[2])[:300]}: old {str(a)[:200]} new {str(b)[:200]}")
    print(f"{len(plan)} queries: " + "; ".join(f"{k} {v}" for k, v in counts.items()))
    sys.exit(1 if counts["wrong"] else 0)


if __name__ == "__main__":
    main()
