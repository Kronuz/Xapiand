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

"""test_serialize_validation_date_http.py BIN -- a `_date` cast value is served over HTTP (D130).

Starts BIN on a fresh data directory, indexes `{"f": {"_date": ...}}` (text and number forms) and reads
each document back; the field's schema type is date. Exits 1 and prints each failed check.

  python3 -I tests/test_serialize_validation_date_http.py build/bin/xapiand
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
PORT = int(os.environ.get("PORT", str(21000 + random.randrange(5000))))
SCRATCH = tempfile.mkdtemp(prefix="date_http_", dir=os.path.expanduser("~/.scratch"))
failures = []


def call(method, path, body=None):
    c = http.client.HTTPConnection("127.0.0.1", PORT, timeout=60)
    c.request(method, path, body=None if body is None else json.dumps(body),
              headers={"Content-Type": "application/json", "Accept": "application/json"})
    r = c.getresponse()
    raw = r.read()
    c.close()
    try:
        return r.status, (json.loads(raw) if raw else None)
    except ValueError:
        return r.status, raw[:200].decode(errors="replace")


def check(ok, what, got=None):
    if not ok:
        failures.append(what)
        print("  FAIL: %s%s" % (what, "" if got is None else ": " + json.dumps(got)[:300]), flush=True)


def main():
    data_dir = os.path.join(SCRATCH, "data")
    node = subprocess.Popen(["timeout", "240", BIN, "--port", str(PORT), "--name", "datehttp", "-D", data_dir],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(240):
            try:
                call("GET", "/")
                break
            except OSError:
                time.sleep(0.5)
        for n, value in enumerate(["2020-01-02", 86400, "1970-01-02"], start=1):
            s, js = call("PUT", "/dates/%d?commit" % n, {"f": {"_date": value}})
            check(s in (200, 201, 204), "PUT {_date: %r} is a success" % (value,), [s, js])
            s, js = call("GET", "/dates/%d" % n)
            check(s == 200 and isinstance(js, dict) and js.get("f") is not None, "GET reads %r back" % (value,), [s, js])
        s, js = call("GET", "/dates/1")
        check(s == 200 and "2020-01-02" in json.dumps(js.get("f")), "the stored value is the date", js)
        s, js = call("GET", "/dates/2")
        check(s == 200 and "1970-01-02" in json.dumps(js.get("f")), "the number reads back as the date", js)
        s, js = call("POST", "/dates/:search", {"_query": "*"})
        check(s == 200 and len(js.get("hits", [])) == 3, "the three documents are searchable", [s, js])
        s, js = call("GET", "/dates/._schema")
        check(s == 200 and "date" in json.dumps(js), "the schema types the field", js)
        s, js = call("PUT", "/dates/9?commit", {"f": {"_date": "not a date"}})
        check(s == 400, "a bad _date is a 400", [s, js])
    finally:
        node.terminate()
        node.wait()
        shutil.rmtree(SCRATCH, ignore_errors=True)
    print("OK: date over HTTP" if not failures else "FAILED: %d checks" % len(failures))
    return 1 if failures else 0


sys.exit(main())
