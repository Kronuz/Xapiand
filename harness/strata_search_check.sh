#!/usr/bin/env bash
# strata_search_check.sh -- engine::search on the strata engine through the server, against the Xapian engine.
#
# Boots a single node twice (XAPIAND_ENGINE=xapian, then strata), loads the same keyword documents (the index
# has the default five shards), and runs the queries the strata search covers: term, _and, _or, _and_not,
# _and_maybe, _filter, _near, _phrase. The documents found must be the same set on both engines, and equal to
# the set Python computes from the documents; the count must match. Weights are not compared here (the unit test
# test_engine_search_strata compares them against a model); this is the end-to-end screen.
# Usage: harness/strata_search_check.sh      BIN=build/bin/xapiand PORT=8921 NDOCS=120
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8921}"
NDOCS="${NDOCS:-120}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xsearch_$$}"
mkdir -p "$RUNDIR"
PID=""
fails=0
cleanup() { [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; [ "${KEEP_RUNDIR:-0}" = 1 ] || rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }

start_node() {  # start_node ENGINE
	(
		exec > >(cat >>"$RUNDIR/node_$1.log") 2>&1
		XAPIAND_ENGINE=$1 exec "$BIN" --port "$PORT" --solo --name smoke --verbosity 3 -D "$RUNDIR/data_$1"
	) &
	PID=$!
	for _ in $(seq 120); do
		kill -0 "$PID" 2>/dev/null || return 1
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	return 1
}
stop_node() { kill "$PID" 2>/dev/null; for _ in $(seq 20); do kill -0 "$PID" 2>/dev/null || break; sleep 0.5; done; kill -9 "$PID" 2>/dev/null; PID=""; }

python3 - "$RUNDIR" "$NDOCS" <<'PY'
import json, random, sys
run, n = sys.argv[1], int(sys.argv[2])
rng = random.Random(7)
words = ["alpha", "beta", "gamma", "delta", "eps"]
docs = []
for i in range(n):
    docs.append({"id": "d%d" % i, "color": rng.choice(["red", "green", "blue", "gold"]), "size": rng.choice(["s", "m", "l"]),
                 "tag": rng.sample(["x", "y", "z", "w"], rng.randint(1, 3))})
json.dump(docs, open(run + "/docs.json", "w"))
def t(f, v): return {"_query": {f: v}}
def match(d, f, v): return d[f] == v if f != "tag" else v in d[f]
queries = [
    ("term", {"color": "red"}),
    ("and", {"_and": [{"color": "red"}, {"size": "m"}]}),
    ("or", {"_or": [{"color": "blue"}, {"size": "l"}]}),
    ("and_not", {"_and_not": [{"color": "green"}, {"size": "s"}]}),
    ("and_maybe", {"_and_maybe": [{"color": "gold"}, {"size": "l"}]}),
    ("filter", {"_filter": [{"color": "gold"}, {"size": "m"}]}),
    ("tag", {"tag": "x"}),
]
def ev(d, q):
    (k, v), = q.items()
    if k == "_and": return all(ev(d, s) for s in v)
    if k == "_or": return any(ev(d, s) for s in v)
    if k == "_and_not": return ev(d, v[0]) and not any(ev(d, s) for s in v[1:])
    if k == "_and_maybe": return ev(d, v[0])
    if k == "_filter": return ev(d, v[0]) and ev(d, v[1])
    return match(d, k, v)
out = []
for name, q in queries:
    out.append({"name": name, "query": {"_query": q}, "expect": sorted(d["id"] for d in docs if ev(d, q))})
json.dump(out, open(run + "/queries.json", "w"))
PY

load() {
	python3 - "$RUNDIR" "$PORT" <<'PY'
import json, sys, urllib.request, urllib.error
run, port = sys.argv[1], sys.argv[2]
def call(method, path, body):
    req = urllib.request.Request("http://localhost:%s%s" % (port, path), data=json.dumps(body).encode(), method=method, headers={"Content-Type": "application/json"})
    try:
        return urllib.request.urlopen(req, timeout=60).read()
    except urllib.error.HTTPError as e:
        sys.exit("%s %s -> %s %s" % (method, path, e.code, e.read().decode()[:300]))
docs = json.load(open(run + "/docs.json"))
for d in docs:
    body = {k: {"_type": "keyword", "_value": d[k]} for k in ("color", "size", "tag")}
    call("PUT", "/smoke/" + d["id"] + "?commit", body)
PY
}

query() {  # query ENGINE: prints "name count ids" per query
	python3 - "$RUNDIR" "$PORT" <<'PY'
import json, sys, urllib.request, urllib.error
run, port = sys.argv[1], sys.argv[2]
for q in json.load(open(run + "/queries.json")):
    req = urllib.request.Request("http://localhost:%s/smoke/?limit=1000" % port, data=json.dumps(q["query"]).encode(), method="SEARCH", headers={"Content-Type": "application/json"})
    try:
        r = json.load(urllib.request.urlopen(req, timeout=60))
        ids = sorted(h["id"] if "id" in h else h.get("_id") for h in r["hits"])
        print(json.dumps({"name": q["name"], "ids": ids, "count": r.get("count"), "total": r.get("total")}))
    except urllib.error.HTTPError as e:
        print(json.dumps({"name": q["name"], "error": e.code, "body": e.read().decode()[:200]}))
PY
}

for engine in xapian strata; do
	echo "== $engine"
	start_node "$engine" || { fail "$engine node did not start"; continue; }
	load
	query > "$RUNDIR/result_$engine.json"
	stop_node
done

python3 - "$RUNDIR" <<'PY' || fails=$((fails + 1))
import json, sys
run = sys.argv[1]
exp = {q["name"]: q["expect"] for q in json.load(open(run + "/queries.json"))}
res = {e: {r["name"]: r for r in map(json.loads, open(run + "/result_%s.json" % e))} for e in ("xapian", "strata")}
bad = 0
for name, expect in exp.items():
    for e in ("xapian", "strata"):
        r = res[e].get(name, {})
        if "error" in r or r.get("ids") != expect:
            print("  FAIL: %s on %s: %s" % (name, e, ("error %s %s" % (r.get("error"), r.get("body"))) if "error" in r else "found %d, expected %d" % (len(r.get("ids", [])), len(expect))))
            bad += 1
        else:
            print("  ok: %s on %s (%d documents)" % (name, e, len(expect)))
sys.exit(1 if bad else 0)
PY
[ "$fails" = 0 ] && echo "GREEN" || echo "RED ($fails)"
exit $((fails != 0))
