#!/usr/bin/env python3
"""Boolean query strings (q=): an implicit OR obeys precedence, and malformed ones answer 400.

Usage: boolean_query_check.py PORT   (a running node on a fresh data directory)
"""
import http.client, json, sys, urllib.parse

port = int(sys.argv[1])
failures = 0


def call(method, path, body=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
    conn.request(method, path, body=body, headers={"Content-Type": "application/json"})
    resp = conn.getresponse()
    data = resp.read()
    conn.close()
    return resp.status, data


# d1..d3 have a, d2 d4 d6 have b, d5 d6 have c.
for i in range(1, 7):
    doc = {"a": "yes" if i <= 3 else "no", "b": "yes" if i % 2 == 0 else "no", "c": "yes" if i >= 5 else "no"}
    call("PUT", "/t/d%d?commit" % i, json.dumps(doc))


def ids(q):
    status, data = call("SEARCH", "/t/?q=" + urllib.parse.quote(q))
    return status, sorted(h["_id"] for h in json.loads(data).get("hits", [])) if status == 200 else data[:80].decode("latin1")


def check(q, want_status, want_ids=None):
    global failures
    status, got = ids(q)
    ok = status == want_status and (want_ids is None or got == want_ids)
    failures += not ok
    print("%-4s %-28s -> %s %s" % ("ok" if ok else "FAIL", q, status, got))


check("a:yes", 200, ["d1", "d2", "d3"])
check("a:yes AND b:yes c:yes", 200, ["d2", "d5", "d6"])  # (a AND b) OR c; it was a OR (b AND c)
check("NOT a:yes b:yes", 200, ["d2", "d4", "d5", "d6"])  # (NOT a) OR b
check("a:yes NOT b:yes", 200, ["d1", "d2", "d3", "d5"])  # a OR (NOT b); it was refused
for q in ("a:yes NOT", "a:yes ()", "(a:yes", "AND", "a:yes OR OR b:yes", "a:yes AND", "a:yes b:yes)"):
    check(q, 400)
print("failures: %d" % failures)
sys.exit(1 if failures else 0)
