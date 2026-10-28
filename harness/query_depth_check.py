#!/usr/bin/env python3
"""Nested queries answer 400 past the depth cap and a result below it, and never stop the node.

Usage: query_depth_check.py PORT   (a running node; the cap is read from src/engine/query.h)
"""
import http.client, json, os, re, sys, urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAUSES = int(re.search(r"MAX_QUERY_CLAUSES\s*=\s*(\d+)", open(os.path.join(ROOT, "src/engine/query.h")).read()).group(1))
CAP = int(re.search(r"MAX_QUERY_DEPTH\s*=\s*(\d+)", open(os.path.join(ROOT, "src/engine/query.h")).read()).group(1))
CHAIN = int(re.search(r"MAX_QUERY_CHAIN\s*=\s*(\d+)", open(os.path.join(ROOT, "src/engine/query.h")).read()).group(1))
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


call("PUT", "/t/1?commit", json.dumps({"body": {"_type": "text", "_value": "xx yy zz a b"}}))

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
# The matcher recurses once per phrase or near group that an AND joins, whatever the clause count: the
# limit is on those. Phrases under an OR (or any operator but AND and FILTER) run apart and are not limited.
def joined(op, n, kind="_phrase"):
    return {"body": {op: [{kind: "a b"} for _ in range(n)]}}


for kind in ("_phrase", "_near"):
    for op in ("_and", "_filter"):
        check("dsl %s of %d %ss" % (op, CHAIN, kind), search(joined(op, CHAIN, kind))[0], 200)
        status, data = search(joined(op, CHAIN + 1, kind))
        check("dsl %s of %d %ss" % (op, CHAIN + 1, kind), status, 400, data)
    for op in ("_or", "_xor", "_and_maybe", "_and_not"):
        check("dsl %s of %d %ss" % (op, CLAUSES // 3, kind), search(joined(op, CLAUSES // 3, kind))[0], 200)
check("dsl _and of the worst shape, %d phrases" % (CLAUSES // 3), search(joined("_and", CLAUSES // 3))[0], 400)
check("text _and of %d phrases" % CHAIN, search({"body": {"_and": '"a b" ' * CHAIN}})[0], 200)
status, data = search({"body": {"_and": '"a b" ' * (CHAIN + 1)}})
check("text _and of %d phrases" % (CHAIN + 1), status, 400, data)
check("text _or of %d phrases" % (CLAUSES // 3), search({"body": {"_or": '"a b" ' * (CLAUSES // 3)}})[0], 200)
check("text AND of %d phrases" % CHAIN, search(boolean(" AND ".join(['"a b"'] * CHAIN)))[0], 200)
status, data = search(boolean(" AND ".join(['"a b"'] * (CLAUSES // 3))))
check("text AND of %d phrases" % (CLAUSES // 3), status, 400, data)
for word in ("_max", "_synonym", "_elite_set"):  # these nest one level per phrase, so the depth cap bounds them
    check("text %s of %d phrases" % (word, CAP - 1), search({"body": {word: '"a b" ' * (CAP - 1)}})[0], 200)
    check("text %s of %d phrases" % (word, CAP + 1), search({"body": {word: '"a b" ' * (CAP + 1)}})[0], 400)
# Phrases wrapped in other operators still chain; halves under an OR and an AND add up.
wrapped = lambda n: {"body": {"_and": [{"_and_not": [{"_phrase": "a b"}, {"_value": "zz"}]} for _ in range(n)]}}
check("dsl _and of %d wrapped phrases" % CHAIN, search(wrapped(CHAIN))[0], 200)
status, data = search(wrapped(CHAIN + 1))
check("dsl _and of %d wrapped phrases" % (CHAIN + 1), status, 400, data)
half = lambda n: {"_and": [{"_phrase": "a b"} for _ in range(n)]}
nested = lambda n: {"body": {"_and": [{"_or": [half(CHAIN // 2), {"_value": "c"}]}, half(n)]}}
check("dsl _and over an _or, %d phrases" % CHAIN, search(nested(CHAIN // 2))[0], 200)
status, data = search(nested(CHAIN // 2 + 1))
check("dsl _and over an _or, %d phrases" % (CHAIN + 1), status, 400, data)
check("node alive", call("GET", "/")[0], 200)
sys.exit(1 if failures else 0)
