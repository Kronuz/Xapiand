#!/usr/bin/env python3
"""Nested queries answer 400 past the depth cap and a result below it, and never stop the node.

Usage: query_depth_check.py PORT   (a running node; the cap is read from src/engine/query.h)
"""
import http.client, json, os, re, sys, urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAUSES = int(re.search(r"MAX_QUERY_CLAUSES\s*=\s*(\d+)", open(os.path.join(ROOT, "src/engine/query.h")).read()).group(1))
CAP = int(re.search(r"MAX_QUERY_DEPTH\s*=\s*(\d+)", open(os.path.join(ROOT, "src/engine/query.h")).read()).group(1))
port = int(sys.argv[1])
failures = 0


def call(method, path, body=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=300)
    conn.request(method, path, body=body, headers={"Content-Type": "application/json"})
    resp = conn.getresponse()
    data = resp.read()
    conn.close()
    return resp.status, data


def check(name, got, want, data=b""):
    global failures
    ok = got == want
    failures += not ok
    print("%-4s %-34s -> %s (want %s) %s" % ("ok" if ok else "FAIL", name, got, want, data[:100].decode("latin1") if not ok or got == 400 else ""))


def search(query=None, q=None):
    path = "/t/?sort=_id" + ("&q=" + urllib.parse.quote(q) if q is not None else "")
    return call("SEARCH", path, json.dumps({"_query": query}) if query is not None else None)


def boolean(text):
    return {"body": {"_or": text}}  # a modifier turns the boolean syntax on


def dsl(n):
    tree = {"_value": "xx"}
    for _ in range(n):
        tree = {"_and": [tree, {"_value": "xx"}]}
    return {"body": tree}


def alt(n):
    return "xx AND (yy OR (" * (n // 2) + "zz" + "))" * (n // 2)


call("PUT", "/t/1?commit", json.dumps({"body": {"_type": "text", "_value": "xx yy zz"}}))

# The text path: nesting is exact at the cap, and a result below it.
check("text alt below cap n=%d" % (CAP - 8), search(boolean(alt(CAP - 8)))[0], 200)
for n in (CAP + 8, 4 * CAP, 20000, 200000):
    status, data = search(boolean(alt(n)))
    check("text alt n=%d" % n, status, 400, data)
status, data = search(boolean("xx AND (" * 2 * CAP + "zz" + ")" * 2 * CAP))
check("text and-chain n=%d" % (2 * CAP), status, 400, data)
check("text parens only n=200000", search(boolean("(" * 200000 + "xx" + ")" * 200000))[0], 200)
check("text q= alt below cap", search(q="body:(%s)" % alt(CAP // 4))[0], 200)
for n in (2000, 3000):
    status, data = search(q="body:(%s)" % alt(n))
    check("text q= alt n=%d" % n, status, 400, data)
check("text q= NOT chain n=2000", search(q="NOT " * 2000 + "xx")[0], 400)

# Width: a flat text of many words is as much recursion for the matcher as a deep one.
words = lambda n: " ".join("w%d" % i for i in range(n))
check("text %d words" % (CLAUSES - 1), search({"body": words(CLAUSES - 1)})[0], 200)
for n in (CLAUSES + 1, 20000, 100000):
    status, data = search({"body": words(n)})
    check("text %d words" % n, status, 400, data)
status, data = search({"body": "yy " * 100000})
check("text 100000 repeats of one word", status, 400, data)

# The query DSL: nested compound clauses. The boundary is where a level no longer fits.
lo, hi = 0, 4 * CAP
while hi - lo > 1:
    mid = (lo + hi) // 2
    if search(dsl(mid))[0] == 200:
        lo = mid
    else:
        hi = mid
print("dsl _and nesting: %d levels answer 200, %d answer 400" % (lo, hi))
check("dsl at the limit n=%d" % lo, search(dsl(lo))[0], 200)
status, data = search(dsl(hi))
check("dsl one above n=%d" % hi, status, 400, data)
check("dsl at least ES default (30)", lo >= 30, True)
for n in (1000, 12000):
    check("dsl n=%d" % n, search(dsl(n))[0], 400)
check("node alive", call("GET", "/")[0], 200)
sys.exit(1 if failures else 0)
