#!/usr/bin/env python3
"""id_zero_check.py BIN -- a numeric document id 0 is rejected, and it takes no other document with it.

Starts BIN (--solo, a fresh data directory, hard timeout), restores 1000 documents with ids 0..999 and puts id 0
on its own. Document 0 must be refused (400) and documents 1..999 must all be there. Exit 1 otherwise.

  python3 harness/id_zero_check.py build/bin/xapiand
"""
import http.client
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

BIN = os.path.abspath(sys.argv[1])
PORT = int(os.environ.get("PORT", "18974"))
DATA = tempfile.mkdtemp(prefix="idzero_", dir=os.path.expanduser("~/.scratch"))
N = 1000


def call(method, path, body=None, ctype="application/json"):
    c = http.client.HTTPConnection("localhost", PORT, timeout=60)
    c.request(method, path, body=body, headers={"Content-Type": ctype})
    r = c.getresponse()
    return r.status, r.read()


node = subprocess.Popen(["timeout", "120", BIN, "--port", str(PORT), "--name", "idzero", "--solo", "-D", DATA],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
bad = []
try:
    for _ in range(100):
        try:
            if call("GET", "/")[0] == 200:
                break
        except OSError:
            time.sleep(0.5)
    body = "".join(json.dumps({"_id": k, "n": k}) + "\n" for k in range(N))
    call("RESTORE", "/r/?commit", body, "application/x-ndjson")
    status, _ = call("PUT", "/p/0?commit", json.dumps({"n": 0}))
    if status != 400:
        bad.append("PUT id 0 answered %d, expected 400" % status)
    status, data = call("SEARCH", "/r/?volatile", json.dumps({"_query": {"n": {"_in": {"_range": {"_from": 0}}}}, "_limit": 1, "_check_at_least": N + 1}))
    total = json.loads(data).get("total")
    if total != N - 1:
        bad.append("RESTORE of ids 0..%d holds %s documents, expected %d" % (N - 1, total, N - 1))
    missing = [k for k in range(1, N) if call("GET", "/r/%d" % k)[0] != 200]
    if missing:
        bad.append("ids missing after the restore: %s" % missing[:5])
finally:
    node.terminate()
    node.wait()
    shutil.rmtree(DATA, ignore_errors=True)
print("\n".join(bad) if bad else "ok: id 0 refused, ids 1..%d kept" % (N - 1))
sys.exit(1 if bad else 0)
