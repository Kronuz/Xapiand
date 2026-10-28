#!/usr/bin/env python3
"""strata_callbacks_check.py BIN -- the same searches on a Xapian and a strata index, answers compared.

Starts BIN twice (--solo, fresh data directories, one engine each by XAPIAND_ENGINE, hard timeout), loads the
same documents, and runs aggregations (metrics and buckets, with and without a query), ranges, sorted searches,
wildcard and fuzzy terms. Every answer must be the same JSON on both engines once `took` is removed (hits
compared as a set when the query has no sort, as a list when it has one). Exit 1 on any difference.

  python3 harness/strata_callbacks_check.py .scratch/xapiand
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
BASE = int(os.environ.get("PORT", "18975"))
SCRATCH = os.environ.get("CHECK_TMP") or tempfile.mkdtemp(prefix="cbcheck_", dir=os.path.expanduser("~/.scratch"))
DOCS = int(os.environ.get("DOCS", "400"))


def call(port, method, path, body=None, ctype="application/json"):
    c = http.client.HTTPConnection("localhost", port, timeout=120)
    c.request(method, path, body=body, headers={"Content-Type": ctype})
    r = c.getresponse()
    return r.status, r.read()


def documents():
    r = random.Random(11)
    states = ["IN", "TX", "CA", "NY", "WA", "OR", "NV"]
    words = ["alpha", "alpine", "beta", "betamax", "gamma", "gammon", "delta", "deltoid"]
    out = []
    for i in range(DOCS):
        out.append({"_id": "doc%d" % i, "n": i, "balance": round(r.uniform(-500, 6000), 2), "age": r.randint(18, 90),
                    "state": r.choice(states), "word": r.choice(words), "tags": (lambda t: t + [t[0]])(r.sample(words, 2)),
                    "note": "%s and %s" % (r.choice(words), r.choice(words)), "mixed": r.choice(["Red", "red", "RED", "Blue"]),
                    "flag": r.random() < 0.3})
    return out


def queries():
    q = []
    aggs = {"_count": {"_count": {"_field": "age"}}, "_sum": {"_sum": {"_field": "balance"}}, "_avg": {"_avg": {"_field": "balance"}},
            "_min": {"_min": {"_field": "age"}}, "_max": {"_max": {"_field": "balance"}}, "_median": {"_median": {"_field": "age"}},
            "_mode": {"_mode": {"_field": "age"}}, "_stats": {"_stats": {"_field": "balance"}},
            "_extended_stats": {"_extended_stats": {"_field": "age"}}, "_variance": {"_variance": {"_field": "age"}},
            "_std": {"_std_deviation": {"_field": "age"}},
            "by_state": {"_terms": {"_field": "state"}, "_aggs": {"avg": {"_avg": {"_field": "balance"}}}},
            "by_word": {"_values": {"_field": "word"}},
            "terms_age": {"_terms": {"_field": "age"}}, "terms_balance": {"_terms": {"_field": "balance"}},
            "terms_word": {"_terms": {"_field": "word"}}, "terms_tags": {"_terms": {"_field": "tags"}},
            "terms_mixed": {"_terms": {"_field": "mixed"}}, "terms_flag": {"_terms": {"_field": "flag"}},
            "terms_missing": {"_terms": {"_field": "nothing"}},
            "terms_nested": {"_terms": {"_field": "state"}, "_aggs": {"words": {"_terms": {"_field": "word"}}}},
            "terms_sorted": {"_terms": {"_field": "age", "_sort": {"_key": "asc"}, "_limit": 7}},
            "hist": {"_histogram": {"_field": "age", "_interval": 10}},
            "ranges": {"_range": {"_field": "balance", "_ranges": [{"_to": 0}, {"_from": 0, "_to": 3000}, {"_from": 3000}]}},
            "filtered": {"_filter": {"state": "TX"}, "_aggs": {"s": {"_sum": {"_field": "balance"}}}}}
    queries_ = [("all", "*"), ("state", {"state": "TX"}), ("age range", {"age": {"_in": {"_range": {"_from": 30, "_to": 60}}}}),
                ("and", {"_and": [{"state": "CA"}, {"age": {"_in": {"_range": {"_from": 18, "_to": 50}}}}]}),
                ("or", {"_or": [{"state": "CA"}, {"word": "beta"}]}),
                ("balance range", {"balance": {"_in": {"_range": {"_from": -100.5, "_to": 2500.25}}}})]
    for qn, qq in queries_:
        for an, ag in aggs.items():
            q.append(("agg %s / %s" % (an, qn), {"_query": qq, "_limit": 0, "_check_at_least": 1000, "_aggs": {an: ag}}, False))
        q.append(("all aggs / %s" % qn, {"_query": qq, "_limit": 5, "_aggs": aggs}, True))
    for qn, qq in queries_:
        q.append(("hits / %s" % qn, {"_query": qq, "_limit": 1000, "_check_at_least": 1000}, False))
        q.append(("sorted / %s" % qn, {"_query": qq, "_sort": [{"age": "asc"}, {"balance": "desc"}], "_limit": 1000}, True))
        q.append(("sorted desc / %s" % qn, {"_query": qq, "_sort": {"balance": "desc"}, "_limit": 25, "_offset": 3}, True))
    return q


def main():
    docs = documents()
    body = "".join(json.dumps(d) + "\n" for d in docs)
    nodes, answers = {}, {}
    try:
        for k, engine in enumerate(("xapian", "strata")):
            port = BASE + k
            data = os.path.join(SCRATCH, "data_" + engine)
            shutil.rmtree(data, ignore_errors=True)
            env = dict(os.environ, XAPIAND_ENGINE=engine)
            nodes[engine] = subprocess.Popen(["timeout", "900", BIN, "--port", str(port), "--name", "cb" + engine, "--solo", "-D", data],
                                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
            for _ in range(120):
                try:
                    if call(port, "GET", "/")[0] == 200:
                        break
                except OSError:
                    time.sleep(0.5)
            schema = {"_schema": {"_type": "object", "balance": {"_type": "floating"}, "age": {"_type": "integer"}, "n": {"_type": "integer"},
                                  "state": {"_type": "keyword"}, "word": {"_type": "keyword"}, "mixed": {"_type": "keyword"}, "tags": {"_type": "array/keyword"},
                                  "note": {"_type": "text"}}}
            assert call(port, "PUT", "/cb/", json.dumps(schema))[0] in (200, 201, 204)
            restored = call(port, "RESTORE", "/cb/?commit", body, "application/x-ndjson")
            assert restored[0] in (200, 201, 204), restored
            counted = json.loads(call(port, "SEARCH", "/cb/?volatile", json.dumps({"_query": "*", "_limit": 0, "_check_at_least": 1000}))[1])["total"]
            assert counted == DOCS, "%s holds %s of %d documents" % (engine, counted, DOCS)
        bad = 0
        total = 0
        for name, query, ordered in queries():
            res = {}
            for k, engine in enumerate(("xapian", "strata")):
                status, data = call(BASE + k, "SEARCH", "/cb/?volatile", json.dumps(query))
                try:
                    doc = json.loads(data)
                except ValueError:
                    doc = {"unparsed": data[:200].decode("latin-1")}
                doc.pop("took", None)
                if isinstance(doc.get("hits"), list):
                    for h in doc["hits"]:
                        h.pop("_version", None)
                    if not ordered:
                        doc["hits"] = sorted(doc["hits"], key=lambda h: h.get("_id", ""))
                res[engine] = (status, doc)
            total += 1
            if name.startswith("agg terms_") and "missing" not in name and name.endswith("/ all"):
                buckets = res["strata"][1].get("aggregations", {}).get(name.split()[1], [])
                # A document counts once per distinct value: two distinct tags (one repeated) per document, one age.
                expected = {"terms_tags": 2 * DOCS, "terms_age": DOCS, "terms_word": DOCS, "terms_mixed": DOCS}.get(name.split()[1])
                if not buckets or (expected is not None and sum(b["_doc_count"] for b in buckets) != expected):
                    bad += 1
                    print("WRONG BUCKETS %s: %s" % (name, json.dumps(res["strata"][1])[:160]))
            if res["xapian"] != res["strata"]:
                bad += 1
                if bad <= 8:
                    print("DIFF %s: xapian %s %s | strata %s %s" % (name, res["xapian"][0], json.dumps(res["xapian"][1])[:160], res["strata"][0], json.dumps(res["strata"][1])[:160]))
        # The strata engine keeps no term lists: `_terms` over analysed text and a document's `:info` are refused.
        refused = call(BASE + 1, "SEARCH", "/cb/?volatile", json.dumps({"_query": "*", "_limit": 0, "_check_at_least": 1000, "_aggs": {"a": {"_terms": {"_field": "note"}}}}))
        listed = call(BASE + 1, "SEARCH", "/cb/?volatile", json.dumps({"_query": "*", "_limit": 1}))[1]
        first = json.loads(listed)["hits"][0]["_id"] if json.loads(listed).get("hits") else print("NO HITS", listed[:200]) or "doc1"
        info = call(BASE + 1, "INFO", "/cb/" + first)
        for what, (status, data) in (("_terms over text", refused), ("document :info", info)):
            total += 1
            if status != 400 or b"term lists" not in data:
                bad += 1
                print("NOT REFUSED %s: %s %s" % (what, status, data[:160]))
        print("searches %d, different %d" % (total, bad))
        sys.exit(1 if bad else 0)
    finally:
        for node in nodes.values():
            node.terminate()
            node.wait()
        for engine in nodes:
            shutil.rmtree(os.path.join(SCRATCH, "data_" + engine), ignore_errors=True)


main()
