#!/usr/bin/env python3
"""id_terms_check.py BIN [--census] -- the accuracy terms of a numeric _id and range queries over it.

Starts BIN (--solo, a fresh data directory, hard timeout), gives an index an integer `_id` through its schema,
restores documents with ids at several magnitudes, and checks every range query over `_id` against a brute-force
answer. With --census (BIN built with -DENGINE_CENSUS=ON) it also checks that no written document has more than
one term leading with Q (gate row T5: the id term is the only one).

  python3 harness/id_terms_check.py build/bin/xapiand [--census]
"""
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
CENSUS = "--census" in sys.argv
PORT = int(os.environ.get("PORT", "18991"))
SCRATCH = os.environ.get("ID_TERMS_TMP") or tempfile.mkdtemp(prefix="id_terms_", dir=os.path.expanduser("~/.scratch"))

IDS = sorted(set(list(range(1, 60)) + list(range(99990, 100012)) + [65535, 65536, 123456, 999999, 5000000, 3000000000]
                 + [int(x) for x in os.environ.get("ID_TERMS_EXTRA", "").split(",") if x]))


def call(method, path, body=None, ctype="application/json"):
    c = http.client.HTTPConnection("localhost", PORT, timeout=60)
    c.request(method, path, body=body, headers={"Content-Type": ctype})
    r = c.getresponse()
    data = r.read()
    return r.status, data


def main():
    data_dir = os.path.join(SCRATCH, "data")
    census = os.path.join(SCRATCH, "census.jsonl")
    shutil.rmtree(data_dir, ignore_errors=True)
    env = dict(os.environ)
    if CENSUS:
        env["XAPIAND_ENGINE_CENSUS"] = census
    node = subprocess.Popen(
        ["timeout", "240", BIN, "--port", str(PORT), "--name", "benchnode", "--solo", "-D", data_dir],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    failures = []
    try:
        for _ in range(120):
            try:
                if call("GET", "/")[0] == 200:
                    break
            except OSError:
                time.sleep(0.5)
        schema = {"_schema": {"_type": "object", "_id": {"_type": "integer"}, "user": {"_type": "keyword"}}}
        status, _ = call("PUT", "/idt/", json.dumps(schema))
        assert status in (200, 201, 204), status
        body = "".join(json.dumps({"_id": i, "user": "u%d" % i}) + "\n" for i in IDS)
        status, _ = call("RESTORE", "/idt/?commit", body, "application/x-ndjson")
        assert status in (200, 201, 204), status

        rng = random.Random(7)
        ranges = [(1, 59), (30, 30), (99995, 100005), (65535, 65536), (100, 5000000), (0, 3000000000), (123457, 999998), (1000, 2000)]
        ranges += [tuple(sorted((rng.choice(IDS), rng.choice(IDS)))) for _ in range(12)]
        for lo, hi in ranges:
            q = {"_query": {"_id": {"_in": {"_range": {"_from": lo, "_to": hi}}}}, "_limit": 1000, "_check_at_least": 1000}
            status, data = call("SEARCH", "/idt/?volatile", json.dumps(q))
            want = [i for i in IDS if lo <= i <= hi]
            if status != 200:
                failures.append("range %d..%d: status %d" % (lo, hi, status))
                continue
            got = sorted(h["_id"] for h in json.loads(data)["hits"])
            if got != want:
                failures.append("range %d..%d: got %d ids, want %d, missing %s, extra %s" % (lo, hi, len(got), len(want), sorted(set(want) - set(got)), sorted(set(got) - set(want))))
        print("range queries: %d, failures: %d" % (len(ranges), len(failures)))

        if CENSUS:
            time.sleep(1)
            multiple = docs = 0
            for line in open(census, errors="replace"):
                counts = json.loads(line)["counts"]
                multiple += counts.get("t5[Q].violation.multiple", 0)
                docs += counts.get("t5[Q].documents", 0)
            print("census: %d documents, %d with more than one Q term" % (docs, multiple))
            if docs == 0:
                failures.append("census recorded no documents")
            if multiple:
                failures.append("%d documents have more than one Q term" % multiple)
    finally:
        node.terminate()
        node.wait()
        if not os.environ.get("ID_TERMS_TMP"):
            shutil.rmtree(SCRATCH, ignore_errors=True)
    for f in failures:
        print("FAIL", f)
    print("id_terms_check:", "FAIL" if failures else "OK")
    return 1 if failures else 0


sys.exit(main())
