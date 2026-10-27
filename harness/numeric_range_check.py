#!/usr/bin/env python3
"""numeric_range_check.py BIN -- range queries over integer, positive, floating and datetime fields against brute force.

Starts BIN (--solo, a fresh data directory, hard timeout), restores documents whose values sit around powers of two
and ten, accuracy edges, zero, negatives and (for datetime) 1970 and 2038, then runs closed and half-open
`_range` queries and compares each answer with the brute-force one. Exit 1 on any wrong range.

  python3 harness/numeric_range_check.py build/bin/xapiand
"""
import calendar
import http.client
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time

BIN = os.path.abspath(sys.argv[1])
PORT = int(os.environ.get("PORT", "18973"))
SCRATCH = os.environ.get("NUMRANGE_TMP") or tempfile.mkdtemp(prefix="numrange_", dir=os.path.expanduser("~/.scratch"))


def call(method, path, body=None, ctype="application/json"):
    c = http.client.HTTPConnection("localhost", PORT, timeout=60)
    c.request(method, path, body=body, headers={"Content-Type": ctype})
    r = c.getresponse()
    return r.status, r.read()


def stamp(ts):
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(ts))


def main():
    data_dir = os.path.join(SCRATCH, "data")
    shutil.rmtree(data_dir, ignore_errors=True)
    node = subprocess.Popen(
        ["timeout", "400", BIN, "--port", str(PORT), "--name", "numrange", "--solo", "-D", data_dir],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    bad = 0
    total = 0
    per = {}
    try:
        for _ in range(120):
            try:
                if call("GET", "/")[0] == 200:
                    break
            except OSError:
                time.sleep(0.5)

        ints = sorted(set([0, 1, 99, 100, 101, 999, 1000, 1001, 1999, 2000, 2001, 2500, 5000, 99999, 100000, 999999,
                           1000000, 1000001, 5000000, 99999999, 100000000, 150000000, 199999999, 200000000, 200000001,
                           250000000, 268435456, 299999999, 300000000, 536870912, 3000000000, -1, -99, -100, -101,
                           -1000, -100000, -99999999, -100000000, -200000000, -268435456, -300000000, -3000000000,
                           2 ** 62, -2 ** 62, 2 ** 63 - 1, -2 ** 63] +
                          [2 ** k + d for k in range(16, 41) for d in (-1, 0, 1)]))
        pos = sorted(v for v in ints if v >= 0) + [2 ** 63, 2 ** 64 - 1]
        flo = sorted(set([x / 4.0 for x in (-40000, -401, -400, -399, -1, 0, 1, 399, 400, 401, 40000, 4000000001)]
                         + [-3e15, 3e15, -2.5e9, 2.5e9, 0.5, -0.5, 99.9, 100.1]))
        dts = sorted(set(
            [calendar.timegm((y, m, d, h, mi, s)) for y in (1969, 1970, 1971, 1999, 2000, 2019, 2037, 2038, 2039, 2100, 2300)
             for (m, d, h, mi, s) in ((1, 1, 0, 0, 0), (1, 1, 0, 0, 1), (2, 28, 23, 59, 59), (3, 1, 0, 0, 0), (6, 15, 12, 34, 56), (12, 31, 23, 59, 59))]))

        schema = {"_schema": {"_type": "object", "n": {"_type": "integer"}, "p": {"_type": "positive"},
                              "f": {"_type": "floating"}, "d": {"_type": "datetime"}, "i": {"_type": "integer"}}}
        assert call("PUT", "/nr/", json.dumps(schema))[0] in (200, 201, 204)
        docs = []
        for k, v in enumerate(ints):
            docs.append({"_id": "n%d" % k, "i": k, "n": v})
        for k, v in enumerate(pos):
            docs.append({"_id": "p%d" % k, "i": 1000 + k, "p": v})
        for k, v in enumerate(flo):
            docs.append({"_id": "f%d" % k, "i": 2000 + k, "f": v})
        for k, v in enumerate(dts):
            docs.append({"_id": "d%d" % k, "i": 3000 + k, "d": stamp(v)})
        body = "".join(json.dumps(x) + "\n" for x in docs)
        assert call("RESTORE", "/nr/?commit", body, "application/x-ndjson")[0] in (200, 201, 204)

        def check(field, values, lo, hi, base, fmt=lambda x: x):
            nonlocal bad, total
            rng = {}
            if lo is not None:
                rng["_from"] = fmt(lo)
            if hi is not None:
                rng["_to"] = fmt(hi)
            q = {"_query": {field: {"_in": {"_range": rng}}}, "_limit": 2000, "_check_at_least": 2000}
            status, data = call("SEARCH", "/nr/?volatile", json.dumps(q))
            want = sorted(base + k for k, v in enumerate(values)
                          if (lo is None or v >= lo) and (hi is None or v <= hi))
            total += 1
            per.setdefault(field, [0, 0])[0] += 1
            got = sorted(h["i"] for h in json.loads(data)["hits"]) if status == 200 else None
            if got != want:
                bad += 1
                per[field][1] += 1
                if per[field][1] <= 4:
                    print("FAIL %s [%s,%s] status=%s missing=%d extra=%d" % (
                        field, lo, hi, status, len(set(want) - set(got or [])), len(set(got or []) - set(want))))

        r = random.Random(5)
        for field, values, base, fmt in (("n", ints, 0, lambda x: x), ("p", pos, 1000, lambda x: x),
                                         ("f", flo, 2000, lambda x: x), ("d", dts, 3000, stamp)):
            fixed = [(values[0], values[-1]), (values[0], values[0]), (values[-1], values[-1]), (None, values[len(values) // 2]),
                     (values[len(values) // 2], None), (None, values[0]), (values[-1], None)]
            for lo, hi in fixed:
                check(field, values, lo, hi, base, fmt)
            for _ in range(150):
                a, b = sorted((r.choice(values), r.choice(values)))
                check(field, values, a, b, base, fmt)
            for _ in range(20):
                v = r.choice(values)
                check(field, values, v, None, base, fmt)
                check(field, values, None, v, base, fmt)
        if ints:
            for lo, hi in ((0, 0), (100, 100), (1, 3000000000), (0, 3000000000), (1999, 4294967295), (100000, 134217729),
                           (2, 150416086), (268435455, 268435457), (-300000000, 300000000)):
                check("n", ints, lo, hi, 0)
    finally:
        node.terminate()
        node.wait()
        shutil.rmtree(data_dir, ignore_errors=True)
    print("ranges %d, wrong %d (per field: %s)" % (total, bad, ", ".join("%s %d/%d" % (k, v[1], v[0]) for k, v in sorted(per.items()))))
    sys.exit(1 if bad else 0)


main()
