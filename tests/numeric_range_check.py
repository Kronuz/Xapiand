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

"""numeric_range_check.py BIN: ranges, sorts and min/max over numbers against brute force (design T6).

Starts BIN on a fresh data directory and checks, per group, that every answer equals brute force:

  range     closed, half-open and random ranges over integer, positive, floating and datetime fields whose
            values sit at 2^k + d (k 31..63), -(2^a) - d (a 30..62), 1 + 2^-k, powers of ten, negative fractions
            and pre-1970 datetimes with sub-second parts
  intbound  floating bounds on integer and positive fields
  inf       +inf and -inf values under open and closed bounds
  sort      ascending and descending sorts of each field
  minmax    min and max aggregations (a double: the nearest double of the exact extreme)
  wide      one field typed integer in one index and floating in another, under one search
  ids       integer ids above 2^53: ranges and sorts on _id
  midpoint  9007199254740993.0000001 as a JSON number, a _float cast of its text and a range bound is its
            nearest double
  refuse    a NaN bound, a NaN value and an integer of 2^63 or more answer 400; -0.0 indexes as 0

Prints each group's wrong count and exits 1 when any answer is wrong.

  python3 tests/numeric_range_check.py build/bin/xapiand
"""

from __future__ import annotations

import calendar
import http.client
import json
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from collections.abc import Callable, Sequence
from typing import Any

BIN = os.path.abspath(sys.argv[1])
PORT = int(os.environ.get("PORT", "18974"))
SCRATCH = os.environ.get("NUMRANGE_TMP") or tempfile.mkdtemp(prefix="numrange_", dir=os.path.expanduser("~/.scratch"))
INF = float("inf")
LIMIT = 5000

Number = int | float
wrong: dict[str, list[int]] = {}


def call(method: str, path: str, body: object = None, ctype: str = "application/json") -> tuple[int, Any]:
    raw = body if isinstance(body, (bytes, str)) or body is None else json.dumps(body)
    conn = http.client.HTTPConnection("localhost", PORT, timeout=60)
    conn.request(method, path, body=raw, headers={"Content-Type": ctype, "Accept": "application/json"})
    response = conn.getresponse()
    data = response.read()
    try:
        return response.status, json.loads(data)
    except ValueError:
        return response.status, data


def msgpack(value: object) -> bytes:
    """The few MsgPack forms these documents need: maps, strings, ints and binary64 floats."""
    if isinstance(value, dict):
        out = bytes([0xDE]) + struct.pack(">H", len(value))
        return out + b"".join(msgpack(k) + msgpack(v) for k, v in value.items())
    if isinstance(value, str):
        data = value.encode()
        return bytes([0xD9, len(data)]) + data
    if isinstance(value, float):
        return b"\xcb" + struct.pack(">d", value)
    if isinstance(value, int) and value >= 0:
        return b"\xcf" + struct.pack(">Q", value)
    if isinstance(value, int):
        return b"\xd3" + struct.pack(">q", value)
    raise TypeError(value)


def record(group: str, ok: bool, label: str) -> None:
    counts = wrong.setdefault(group, [0, 0])
    counts[0] += 1
    if not ok:
        counts[1] += 1
        if counts[1] <= 5:
            print(f"WRONG {group}: {label}")


def hits(status: int, data: Any) -> list[dict[str, Any]] | None:
    return data["hits"] if status == 200 and isinstance(data, dict) else None


def search(path: str, query: object, sort: str | None = None) -> list[dict[str, Any]] | None:
    body: dict[str, object] = {"_query": query, "_limit": LIMIT, "_check_at_least": LIMIT}
    if sort:
        body["_sort"] = sort
    return hits(*call("SEARCH", f"{path}?volatile", body))


def in_range(value: Number, lo: Number | None, hi: Number | None) -> bool:
    return (lo is None or value >= lo) and (hi is None or value <= hi)


def range_query(field: str, lo: object, hi: object) -> dict[str, object]:
    bounds: dict[str, object] = {}
    if lo is not None:
        bounds["_from"] = lo
    if hi is not None:
        bounds["_to"] = hi
    return {field: {"_in": {"_range": bounds}}}


class Field:
    """One numeric field of one index: its values by document number, and how a bound is written."""

    def __init__(self, index: str, name: str, values: Sequence[Number], fmt: Callable[[Number], object] = lambda v: v):
        self.index, self.name, self.values, self.fmt = index, name, list(values), fmt

    def check_range(self, group: str, lo: Number | None, hi: Number | None) -> None:
        want = sorted(i for i, v in enumerate(self.values) if in_range(v, lo, hi))
        found = search(
            f"/{self.index}/",
            range_query(self.name, None if lo is None else self.fmt(lo), None if hi is None else self.fmt(hi)),
        )
        got = None if found is None else sorted(h["i"] for h in found)
        ok = got == want
        detail = "" if ok else f" missing {len(set(want) - set(got or []))} extra {len(set(got or []) - set(want))}"
        record(group, ok, f"{self.index}.{self.name} [{lo}, {hi}]{detail}")

    def check_sort(self) -> None:
        everything = range_query(self.name, None, None)
        for descending in (False, True):
            want = sorted(range(len(self.values)), key=lambda i: self.values[i], reverse=descending)
            found = search(f"/{self.index}/", everything, ("-" if descending else "") + self.name)
            got = None if found is None else [h["i"] for h in found]
            # Equal values may come in any order among themselves.
            ok = got is not None and [self.values[i] for i in got] == [self.values[i] for i in want]
            record("sort", ok, f"{self.index}.{self.name} {'desc' if descending else 'asc'}")

    def check_minmax(self) -> None:
        aggs = {"lo": {"_min": {"_field": self.name}}, "hi": {"_max": {"_field": self.name}}}
        body = {"_query": range_query(self.name, None, None), "_limit": 0, "_aggs": aggs}
        status, data = call("SEARCH", f"/{self.index}/?volatile", body)
        finite = [v for v in self.values if v not in (INF, -INF)]
        results = data.get("aggregations", {}) if status == 200 and isinstance(data, dict) else {}
        for key, metric, want in (("lo", "_min", min(finite)), ("hi", "_max", max(finite))):
            got = results.get(key, {metric: None}).get(metric)
            ok = got is not None and float(got) == float(want)
            record("minmax", ok, f"{self.index}.{self.name} {metric} {got}, want {float(want)!r}")

    def check_ranges(self, rnd: random.Random) -> None:
        values = sorted({v for v in self.values if v not in (INF, -INF)})
        middle = values[len(values) // 2]
        for lo, hi in (
            (values[0], values[-1]),
            (values[0], values[0]),
            (values[-1], values[-1]),
            (None, middle),
            (middle, None),
            (None, values[0]),
            (values[-1], None),
        ):
            self.check_range("range", lo, hi)
        for _ in range(150):
            lo, hi = sorted((rnd.choice(values), rnd.choice(values)))
            self.check_range("range", lo, hi)
        for _ in range(20):
            v = rnd.choice(values)
            self.check_range("range", v, None)
            self.check_range("range", None, v)


def stamp(us: Number) -> str:
    seconds, micros = divmod(int(us), 1_000_000)
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(seconds)) + f".{micros:06d}"


def restore(index: str, schema: dict[str, object], docs: list[dict[str, object]]) -> None:
    status, data = call("PUT", f"/{index}/", {"_schema": {"_type": "object", **schema}})
    assert status in (200, 201, 204), (index, status, data)
    body = "".join(json.dumps(d) + "\n" for d in docs)
    status, data = call("RESTORE", f"/{index}/?commit", body, "application/x-ndjson")
    assert status in (200, 201, 204), (index, status, data)


def value_sets() -> tuple[list[int], list[int], list[float], list[int]]:
    ints = {0, 1, -1, 99, 100, 101, -100, 2**62, -(2**62), 2**63 - 1, -(2**63), -(2**63) + 1}
    ints |= {2**k + d for k in range(31, 64) for d in (-2, -1, 0, 1, 2) if 2**k + d < 2**63}
    ints |= {-(2**a) - d for a in range(30, 63) for d in (0, 1, 2)}
    ints |= {10**k + d for k in range(1, 19) for d in (-1, 0, 1)}
    pos = {v for v in ints if v >= 0} | {
        2**k + d for k in range(53, 65) for d in (-2, -1, 0, 1, 2) if 0 <= 2**k + d < 2**64
    }
    flo = {1 + 2.0**-k for k in range(1, 53)} | {-(1 + 2.0**-k) for k in range(1, 53)}
    flo |= {x / 4.0 for x in (-40000, -401, -400, -399, -1, 0, 1, 399, 400, 401, 40000, 4000000001)}
    flo |= {
        -3e15,
        3e15,
        -2.5e9,
        2.5e9,
        0.5,
        -0.5,
        99.9,
        100.1,
        -0.001,
        -1e-300,
        5e-324,
        -5e-324,
        2.2250738585072014e-308,
    }
    flo |= {1.7976931348623157e308, -1.7976931348623157e308, 2.0**53, 2.0**53 + 2, 2.0**63, 2.0**64, -(2.0**63)}
    years = (1900, 1950, 1969, 1970, 1971, 2000, 2038, 2039, 2100)
    dts = {
        calendar.timegm((y, m, d, h, mi, s)) * 1_000_000 + us
        for y in years
        for (m, d, h, mi, s) in ((1, 1, 0, 0, 0), (6, 15, 12, 34, 56), (12, 31, 23, 59, 59))
        for us in (0, 1, 500_000, 999_999)
    }
    return sorted(ints), sorted(pos), sorted(flo), sorted(dts)


def check_values(rnd: random.Random) -> None:
    ints, pos, flo, dts = value_sets()
    fields = [Field("t6n", "n", ints), Field("t6p", "p", pos), Field("t6f", "f", flo), Field("t6d", "d", dts, stamp)]
    types = {"n": "integer", "p": "positive", "f": "floating", "d": "datetime"}
    for field in fields:
        docs = [{"_id": f"{field.name}{i}", "i": i, field.name: field.fmt(v)} for i, v in enumerate(field.values)]
        restore(field.index, {field.name: {"_type": types[field.name]}, "i": {"_type": "integer"}}, docs)
    for field in fields:
        field.check_ranges(rnd)
        field.check_sort()
        if field.name != "d":
            field.check_minmax()
    n, p = fields[0], fields[1]
    for field in (n, p):
        for lo, hi in ((2.5, 1000.5), (-0.5, 0.5), (99.5, None), (None, 100.5), (2.0**62 + 0.5, None), (0.1, 0.9)):
            if field is p and (lo is not None and lo < 0):
                continue
            field.check_range("intbound", lo, hi)


def check_infinities() -> None:
    values = [-INF, -1.7976931348623157e308, -1.0, 0.0, 1.0, 1.7976931348623157e308, INF]
    schema = {"_schema": {"_type": "object", "f": {"_type": "floating"}, "i": {"_type": "integer"}}}
    status, _ = call("PUT", "/t6inf/", schema)
    assert status in (200, 201, 204)
    for i, v in enumerate(values):
        status, data = call("PUT", f"/t6inf/x{i}?commit", msgpack({"i": i, "f": v}), "application/x-msgpack")
        record("inf", status in (200, 201, 204), f"PUT f={v} answered {status} {data!r:.200}")
    field = Field("t6inf", "f", values)
    for lo, hi in (
        (5.0, None),
        (None, -5.0),
        (None, None),
        (-1.7976931348623157e308, 1.7976931348623157e308),
        (0.0, None),
    ):
        field.check_range("inf", lo, hi)


def check_wide() -> None:
    ints = [2**53 + 1, 2**53 + 3, 2**60 + 1, -(2**60) - 1, 7, -7, 2**63 - 1]
    flos = [2.0**53, 2.0**53 + 2, 2.0**60, 6.5, -6.5, 1e300]
    restore(
        "t6wa",
        {"w": {"_type": "integer"}, "i": {"_type": "integer"}},
        [{"_id": f"a{i}", "i": i, "w": v} for i, v in enumerate(ints)],
    )
    restore(
        "t6wb",
        {"w": {"_type": "floating"}, "i": {"_type": "integer"}},
        [{"_id": f"b{i}", "i": 100 + i, "w": v} for i, v in enumerate(flos)],
    )
    union = {i: v for i, v in enumerate(ints)} | {100 + i: v for i, v in enumerate(flos)}
    for lo, hi in ((2**53, 2**53 + 2), (2**53 + 1, None), (None, 7), (-7, 6.5), (2**60, 2**60 + 1), (None, None)):
        want = sorted(i for i, v in union.items() if in_range(v, lo, hi))
        found = search("/t6w*/", range_query("w", lo, hi))
        got = None if found is None else sorted(h["i"] for h in found)
        record("wide", got == want, f"/t6w*/ w [{lo}, {hi}] got {got} want {want}")
    found = search("/t6w*/", range_query("w", None, None), "w")
    got = None if found is None else [union[h["i"]] for h in found]
    record("wide", got == sorted(union.values()), f"/t6w*/ sort w got {got}")


def check_ids() -> None:
    ids = sorted({2**k + d for k in range(53, 63) for d in (-1, 0, 1)} | {1, 2, 2**63 - 1})
    restore("t6id", {"_id": {"_type": "integer"}, "i": {"_type": "integer"}}, [{"_id": v, "i": v} for v in ids])
    for lo, hi in ((2**53, 2**53 + 1), (2**60 - 1, 2**60), (2**62, None), (None, 2**53 - 1), (None, None)):
        want = [v for v in ids if in_range(v, lo, hi)]
        found = search("/t6id/", range_query("_id", lo, hi))
        got = None if found is None else sorted(h["i"] for h in found)
        record("ids", got == want, f"_id [{lo}, {hi}] got {got} want {want}")
    found = search("/t6id/", range_query("_id", None, None), "_id")
    record("ids", found is not None and [h["i"] for h in found] == ids, "sort _id")


MIDPOINT = "9007199254740993.0000001"  # just above the midpoint of 2^53 and 2^53 + 2: its nearest double is 2^53 + 2


def check_midpoint() -> None:
    """A decimal just past a midpoint is its nearest double as a JSON number, a cast text and a range bound (F3).

    A numeric field refuses a bare string value; the text reaches it through the `_float` cast.
    """
    nearest = float(MIDPOINT)
    assert nearest == 2.0**53 + 2
    schema = {"_schema": {"_type": "object", "f": {"_type": "floating"}, "i": {"_type": "integer"}}}
    status, _ = call("PUT", "/t6m/", schema)
    assert status in (200, 201, 204)
    bodies = {
        "json": f'{{"i": 0, "f": {MIDPOINT}}}',
        "cast": f'{{"i": 1, "f": {{"_float": "{MIDPOINT}"}}}}',
        "below": '{"i": 2, "f": 9007199254740992.0}',
        "integer": '{"i": 3, "f": 9007199254740994}',
    }
    for name, body in bodies.items():
        status, data = call("PUT", f"/t6m/{name}?commit", body)
        record("midpoint", status in (200, 201, 204), f"PUT {name} answered {status} {data!r:.200}")
    values = [nearest, nearest, 2.0**53, 2**53 + 2]
    for lo, hi, lo_text, hi_text in (
        (nearest, nearest, f'"{MIDPOINT}"', f'"{MIDPOINT}"'),
        (nearest, None, MIDPOINT, None),
        (None, 2**53 + 1, None, "9007199254740993"),
        (2**53 + 1, None, "9007199254740993", None),
    ):
        want = sorted(i for i, v in enumerate(values) if in_range(v, lo, hi))
        bounds = ", ".join(f'"{k}": {t}' for k, t in (("_from", lo_text), ("_to", hi_text)) if t is not None)
        body = f'{{"_query": {{"f": {{"_in": {{"_range": {{{bounds}}}}}}}}}, "_limit": 10}}'
        found = hits(*call("SEARCH", "/t6m/?volatile", body))
        got = None if found is None else sorted(h["i"] for h in found)
        record("midpoint", got == want, f"f [{lo_text}, {hi_text}] got {got} want {want}")


def check_refusals() -> None:
    schema = {"_schema": {"_type": "object", "n": {"_type": "integer"}, "f": {"_type": "floating"}}}
    status, _ = call("PUT", "/t6r/", schema)
    assert status in (200, 201, 204)
    status, data = call("PUT", "/t6r/nan?commit", msgpack({"f": float("nan")}), "application/x-msgpack")
    record("refuse", status == 400, f"NaN value answered {status} {data!r:.200}")
    status, data = call("PUT", "/t6r/big?commit", {"n": 2**63})
    record("refuse", status == 400, f"integer 2^63 answered {status} {data!r:.200}")
    status, data = call("PUT", "/t6r/negzero?commit", {"f": -0.0})
    record("refuse", status in (200, 201, 204), f"-0.0 answered {status}")
    found = search("/t6r/", range_query("f", 0, 0))
    record("refuse", found is not None and [h.get("_id") for h in found] == ["negzero"], f"-0.0 in [0, 0]: {found}")
    status, data = call("SEARCH", "/t6r/?volatile", {"_query": range_query("f", "nan", 1.0)})
    record("refuse", status == 400, f"NaN bound answered {status} {data!r:.200}")


def main() -> None:
    data_dir = os.path.join(SCRATCH, "data")
    shutil.rmtree(data_dir, ignore_errors=True)
    node = subprocess.Popen(
        ["timeout", "900", BIN, "--port", str(PORT), "--name", "numrange", "-D", data_dir],
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
        rnd = random.Random(5)
        for check in (
            lambda: check_values(rnd),
            check_infinities,
            check_wide,
            check_ids,
            check_midpoint,
            check_refusals,
        ):
            check()
    finally:
        node.terminate()
        node.wait()
        shutil.rmtree(SCRATCH, ignore_errors=True)
    print("; ".join(f"{group} {bad} wrong of {total}" for group, (total, bad) in wrong.items()))
    sys.exit(1 if any(bad for _, bad in wrong.values()) else 0)


if __name__ == "__main__":
    main()
