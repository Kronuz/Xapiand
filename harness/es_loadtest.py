#!/usr/bin/env python3
# Fair ES counterpart to Xapiand's harness/loadtest.py: same accounts dataset
# replicated to the same doc count, same concurrency/duration/trials, the same
# 7-query mix translated to ES DSL. Measures index docs/s, on-disk bytes, and
# query QPS + p50/p95/p99.
import json, time, http.client, threading, statistics, sys, os

ES = "localhost:9200"
IDX = "accounts"
DATASET = "docs/assets/accounts.ndjson"
REPLICATE = int(os.environ.get("REPLICATE", 20))
CONCURRENCY = int(os.environ.get("CONCURRENCY", 16))
DURATION = int(os.environ.get("DURATION", 5))
TRIALS = int(os.environ.get("TRIALS", 3))
NUM_SHARDS = int(os.environ.get("NUM_SHARDS", 1))

def es(method, path, body=None, conn=None):
    c = conn or http.client.HTTPConnection(ES, timeout=60)
    hdr = {"Content-Type": "application/json"}
    c.request(method, path, body, hdr)
    r = c.getresponse(); data = r.read()
    if conn is None: c.close()
    return r.status, data

# ES DSL translation of loadtest.py's QUERY_MIX
QUERIES = [
    ("match_all",   {"query": {"match_all": {}}}),
    ("term_gender", {"query": {"term": {"gender.keyword": "male"}}}),
    ("nested_name", {"query": {"match": {"name.firstName": "Michael"}}}),
    ("range_age",   {"query": {"range": {"age": {"gte": 20, "lte": 30}}}}),
    ("range_bal",   {"query": {"range": {"balance": {"gte": 1000, "lte": 3000}}}}),
    ("term_state",  {"query": {"term": {"contact.state.keyword": "California"}}}),
    ("bool_and",    {"query": {"bool": {"must": [{"term": {"gender.keyword": "male"}},
                                                 {"range": {"age": {"gte": 25, "lte": 40}}}]}}}),
]

# ---- load base docs ----
base = [json.loads(l) for l in open(DATASET) if l.strip()]
print(f"base docs: {len(base)}  replicate: {REPLICATE}  total: {len(base)*REPLICATE}")

# ---- fresh index ----
es("DELETE", f"/{IDX}")
es("PUT", f"/{IDX}", json.dumps({"settings": {"number_of_shards": NUM_SHARDS, "number_of_replicas": 0}}))

# ---- INDEX phase: bulk, replicated ----
def build_bulk():
    lines = []
    for r in range(REPLICATE):
        for d in base:
            src = {k: v for k, v in d.items() if k != "_id"}
            did = d.get("_id", 0) + r * 100000
            lines.append(json.dumps({"index": {"_index": IDX, "_id": did}}))
            lines.append(json.dumps(src))
    return lines
lines = build_bulk()
total_docs = len(lines) // 2
t0 = time.time()
BATCH = 4000  # action+source lines per bulk
i = 0
while i < len(lines):
    chunk = lines[i:i+BATCH]
    body = "\n".join(chunk) + "\n"
    st, data = es("POST", "/_bulk", body)
    if st >= 300: print("bulk err", st, data[:200]); sys.exit(1)
    i += BATCH
es("POST", f"/{IDX}/_refresh")
idx_dt = time.time() - t0
print(f"[index] {total_docs} docs in {idx_dt:.3f}s = {int(total_docs/idx_dt)} docs/s")

# on-disk size
st, data = es("GET", f"/{IDX}/_stats/store")
size = json.loads(data)["indices"][IDX]["primaries"]["store"]["size_in_bytes"]
print(f"[index] disk={size} bytes")

# ---- QUERY phase ----
def worker(deadline, counts, lats, tid):
    conn = http.client.HTTPConnection(ES, timeout=30)
    n = 0
    while time.time() < deadline:
        name, q = QUERIES[n % len(QUERIES)]
        b = time.time()
        st, data = es("POST", f"/{IDX}/_search?size=10", json.dumps(q), conn)
        lats.append((time.time() - b) * 1000)
        if st < 300: counts[tid] += 1
        n += 1
    conn.close()

for trial in range(1, TRIALS+1):
    counts = [0]*CONCURRENCY
    lats = []
    deadline = time.time() + DURATION
    ths = [threading.Thread(target=worker, args=(deadline, counts, lats, t)) for t in range(CONCURRENCY)]
    for t in ths: t.start()
    for t in ths: t.join()
    total = sum(counts)
    qps = total / DURATION
    lats.sort()
    def pct(p): return lats[int(len(lats)*p)] if lats else 0
    print(f"[query] trial {trial}/{TRIALS}: {int(qps)} qps, p50={pct(0.50):.3f}ms p95={pct(0.95):.3f}ms p99={pct(0.99):.3f}ms (n={total})")
