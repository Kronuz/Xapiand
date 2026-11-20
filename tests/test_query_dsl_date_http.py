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

"""The query DSL serves `_date`, `_time` and `_timedelta` as typed-value operators, as it serves `_datetime`.

Usage: test_query_dsl_date_http.py BINARY   (boots a node on a free port and a fresh data directory)
"""
import http.client, json, os, shutil, socket, subprocess, sys, tempfile, time

binary = os.path.abspath(sys.argv[1])
work = tempfile.mkdtemp(prefix="dsl-date-")
with socket.socket() as s:
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
node = subprocess.Popen([binary, "--port", str(port), "--name", "dsldate", "-D", os.path.join(work, "data")],
                        stdout=open(os.path.join(work, "node.log"), "w"), stderr=subprocess.STDOUT)
failures = 0


def call(method, path, body=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
    conn.request(method, path, body=body, headers={"Content-Type": "application/json"})
    resp = conn.getresponse()
    data = resp.read()
    conn.close()
    return resp.status, data


def search(query):
    status, data = call("SEARCH", "/dsl/", json.dumps({"_query": query}))
    if status == 200:
        return status, sorted(h["_id"] for h in json.loads(data).get("hits", []))
    return status, data[:160].decode("latin1")


def check(label, query, want_status, want_ids=None, want_text=None):
    global failures
    status, got = search(query)
    ok = status == want_status and (want_ids is None or got == want_ids) and (want_text is None or want_text in got)
    failures += not ok
    print("%-4s %-44s -> %s %s" % ("ok" if ok else "FAIL", label, status, got))


try:
    for _ in range(240):
        try:
            if call("GET", "/")[0] == 200:
                break
        except OSError:
            pass
        if node.poll() is not None:
            sys.exit("node exited before ready: " + os.path.join(work, "node.log"))
        time.sleep(0.5)
    docs = {
        "1": {"d": "2020-01-02", "dt": "2020-01-02T10:30:00", "t": "10:30:00", "td": "+10:12:12"},
        "2": {"d": "2021-05-06", "dt": "2021-05-06T11:45:00", "t": "11:45:00", "td": "+01:02:03"},
    }
    for i, doc in docs.items():
        status, _ = call("PUT", "/dsl/%s?commit" % i, json.dumps(doc))
        assert status < 300, status

    # A1: each operator finds its document, by a field.
    check("d  _date", {"d": {"_date": "2020-01-02"}}, 200, ["1"])
    check("dt _datetime", {"dt": {"_datetime": "2021-05-06T11:45:00"}}, 200, ["2"])
    check("t  _time", {"t": {"_time": "10:30:00"}}, 200, ["1"])
    check("td _timedelta", {"td": {"_timedelta": "+01:02:03"}}, 200, ["2"])
    check("d  _date, in a list", {"_or": [{"d": {"_date": "2020-01-02"}}, {"d": {"_date": "2021-05-06"}}]}, 200, ["1", "2"])
    # A1: and by the global query (no field).
    check("global _date", {"_date": "2020-01-02"}, 200)
    check("global _time", {"_time": "10:30:00"}, 200)
    check("global _timedelta", {"_timedelta": "+10:12:12"}, 200)

    # A2: a datetime of a new field, and of a date and a time.
    check("new field _datetime", {"nf": {"_datetime": "2020-01-02"}}, 200, [])
    check("_datetime of _date and _time (strings)", {"dt": {"_datetime": {"_date": "2020-01-02", "_time": "10:30:00"}}}, 200, ["1"])
    check("_datetime of _date and _time (maps)",
          {"dt": {"_datetime": {"_date": {"_year": 2020, "_month": 1, "_day": 2}, "_time": "10:30:00"}}}, 200, ["1"])
    check("_float into a datetime field", {"dt": {"_float": 1.5}}, 400)
    check("_date that is no date", {"d": {"_date": "nonsense"}}, 400)
finally:
    node.kill()
    node.wait()
    shutil.rmtree(work, ignore_errors=True)
print("failures: %d" % failures)
sys.exit(1 if failures else 0)
