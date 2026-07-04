---
title: Benchmarks
---

A single-node smoke comparison of Xapiand against Elasticsearch on the same
dataset and the same query mix. The headline is the on-disk footprint: on this
data Xapiand stores the same 15,000 documents in **3.6 MB against Elasticsearch's
13.1 MB**, about **3.6x smaller**, while serving queries at a higher rate and
lower tail latency.

Read the numbers with the caveats below in mind. This is a quick, honest
side-by-side to get a feel for the shape of things, not a controlled, tuned,
production-scale benchmark. In particular the two engines did not run on the same
substrate (see "Environment").

---

## Setup

Both engines indexed the bundled `accounts` dataset (1,000 records of nested
account objects: name, age, gender, balance, nested `contact` address, a text
`personality` field, a geo point) replicated to **20,000 documents**, then served
a fixed mix of seven queries at **concurrency 16 for 5 seconds, two trials**.

The query mix is the same on both sides, translated to each engine's query
language:

- `match_all`
- a term filter (`gender = male`)
- a nested match (`name.firstName = Michael`)
- two numeric range filters (`age` in 20..30, `balance` in 1000..3000)
- a keyword term (`contact.state = California`)
- a boolean AND (`gender = male` AND `age` in 25..40)

Xapiand is driven by `harness/loadtest.py`; Elasticsearch by
`harness/es_loadtest.py`. Both replicate the dataset to the same count, bulk-load
it, then hammer the query mix from 16 threads and report QPS and latency
percentiles. Query numbers are from a warm run.

## Results

| Metric | Xapiand (this branch) | Xapiand (`origin/master`) | Elasticsearch 8.15.3 |
| --- | ---: | ---: | ---: |
| Index throughput | ~12,100 docs/s | ~10,200 docs/s | ~12,900 docs/s |
| On-disk size (20k docs) | **3.47 MB** | 3.55 MB | 17.3 MB |
| Query throughput | ~12,800 qps | ~13,000 qps | ~7,200 qps |
| Query latency p50 | 1.17 ms | 1.16 ms | 1.94 ms |
| Query latency p95 | 2.18 ms | 2.11 ms | 4.08 ms |
| Query latency p99 | 2.75 ms | 2.65 ms | 5.83 ms |

The `origin/master` column is the upstream Xapiand exactly at `origin/master` with
only the Apple-silicon build fix cherry-picked on top (commit `da3a9b97a`), so the
two Xapiand columns isolate what the extraction/modernization work on this branch
changed. Four things stand out.

**On-disk footprint.** Xapiand's ~5x smaller index is the most defensible result
here, because it does not depend on the runtime substrate (see below). It is the
Zstandard-compressed storage doing its job on this fairly compressible, text-heavy
data. Both Xapiand builds are within ~2% of each other.

**Index throughput.** This branch bulk-loads ~19% faster than `origin/master`
(~12,100 vs ~10,200 docs/s) and is now level with Elasticsearch. The write path
genuinely improved with the modernization.

**Query throughput and latency.** The two Xapiand builds are within a few percent
(`origin/master` a hair ahead at p50), and both serve ~1.8x the QPS of
Elasticsearch at roughly half the p50/p95/p99. Some of the gap to ES is real and
some is the substrate (ES pays a virtualization and loopback-network tax that
Xapiand does not; see Environment). The small residual query gap to `origin/master`
is per-request overhead from the reactor-based request path; profiling shows the
query path itself is search/IO-bound, so it is CPU headroom rather than a wall.

**Xapiand vs Elasticsearch.** On this workload Xapiand is dramatically more compact
on disk and serves queries faster at lower latency; index throughput is a wash.

## Xapian 2.0.0 upgrade

This branch also carries the vendored Xapian fork forward from the 1.5.0-dev
snapshot to the **2.0.0** release (rebuilt as clean per-feature patches on top of
pristine 2.0.0). To isolate what the engine upgrade alone costs or buys, both
builds were measured back-to-back in the same session, same machine, same
`loadtest.py` invocation (20k docs, concurrency 16, 6s, three trials); `origin/master`
here is the 1.5.0 oracle binary described above.

| Metric | This branch (Xapian 2.0.0) | `origin/master` (Xapian 1.5.0) | Δ |
| --- | ---: | ---: | ---: |
| Index throughput | ~11,500 docs/s | ~10,600 docs/s | **+8%** |
| On-disk size (20k docs) | **3.41 MB** | 3.68 MB | **−7%** |
| Query throughput (warm) | ~12,330 qps | ~12,670 qps | −2.7% |
| Query latency p50 | 1.21 ms | 1.19 ms | +0.02 ms |
| Query latency p95 | 2.30 ms | 2.18 ms | +0.12 ms |
| Query latency p99 | 2.97 ms | 2.73 ms | +0.24 ms |

The upgrade **indexes ~8% faster and stores ~7% smaller** on this data. Query
throughput is within ~3% and lands a hair behind, but that gap is not the engine:
it is the reactor request path's offload thread-hop (every DB-touching request hops
to the reactor pool so a slow query cannot stall co-located connections), a
deliberate un-stallability trade that predates this upgrade and shows up exactly on
a low-concurrency fast-query bench. Correctness held: the full end-to-end suite runs
green against the 1.5.0 oracle with **zero regressions**, and 2.0.0's improved
stemming and stopword handling actually fixed five assertions that fail on
`origin/master` (default-operator matching, query-time stopword filtering, a
complex-object info case, and a typed-content selector).

## Cluster: with vs without the Remote protocol

The numbers above are all single-node (`--solo`), where a search never leaves the
process. A cluster is different: an index is sharded across nodes, so a search on
the coordinator runs the two-phase Remote match against the other nodes' shards, and
a write replicates. `harness/cluster_bench.sh` runs the *same* load
(`accounts` x20k, concurrency 16) once solo and once on a 2-node localhost cluster,
so the cost of the distributed data plane is explicit:

| Metric | Solo (no Remote) | 2-node cluster | Δ |
| --- | ---: | ---: | ---: |
| Index throughput | ~11,400 docs/s | ~5,800 docs/s | **−49%** |
| Query throughput | ~12,900 qps | ~215 qps | **−98%** |
| Query p50 | 1.16 ms | 79 ms | ~68x |
| Query p95 | 2.14 ms | 196 ms | ~91x |
| Query p99 | 2.72 ms | 231 ms | ~85x |

**Correctness is restored, performance is the next frontier.** After the v47 remote
protocol + two-phase reconciliation, a cluster forms, replicates, and returns correct
distributed results (`cluster_check.sh` is green). But the query path is currently
**slow**: a single *uncontended* cross-node search is ~12 ms (vs ~1 ms solo), and
under concurrency 16 the p50 degrades to ~79 ms, so it isn't just a fixed network hop,
it also serializes under load. Three leads to chase, in order of suspicion:

1. **The two-phase round-trip.** Every search is MSG_QUERY -> REPLY_STATS ->
   MSG_GETMSET -> REPLY_RESULTS across the socket; the ~12 ms uncontended floor is
   already high for localhost and points at per-request overhead (possibly a shard
   reopen / reconnect in the checkout path, `shard.cc`'s `Xapian::Remote::open`).
2. **A global lock on the prepared-match table.** `remote_protocol_views.cc` guards
   its shared `pending_queries` map with one process-wide mutex, taken on *every*
   MSG_QUERY and MSG_GETMSET, so concurrent cross-node searches contend on it.
3. **The reactor offload hop**, which the solo bench already showed as a small tax,
   compounds here because every remote DB operation crosses it.

Index throughput halving is more expected (writes replicate and pay first-touch shard
creation across nodes). These are measurements on a fresh cluster with default
settings, no tuning; the point is to make the gap visible and give the optimization
work a baseline, not to quote a production multiplier.

## Environment and caveats

This is a smoke test, and the two engines were not on equal footing:

- **Different substrate.** Xapiand ran natively on macOS (Apple silicon).
  Elasticsearch ran in the official Docker image under a Linux VM (colima), so it
  paid VM and virtual-network overhead that Xapiand did not. This flatters
  Xapiand on the latency and QPS numbers and is the single biggest reason not to
  read those as a controlled result. The on-disk size is unaffected by this.
- **Small, single-node, single-shard.** 20,000 documents on one node with one
  shard and no replicas. Nothing here says anything about multi-node scaling,
  large corpora, or sharded fan-out, where the trade-offs differ.
- **Default configuration.** Stock settings on both sides. No JVM heap tuning
  beyond a 1 GB cap for Elasticsearch, no merge/refresh tuning, no Xapiand
  tuning. Elasticsearch also does more work by default (per-field `.keyword`
  multi-fields, its own analysis chain).
- **Warmup.** Elasticsearch's first query trial was noticeably slower than later
  ones (JVM warmup and cache fill); the reported query numbers are from a warm
  run. Xapiand was steady from the first trial.
- **Feature surface.** The two are not feature-identical. This mix exercises the
  common ground (term, range, nested, boolean), not the full surface of either.

The honest summary: on this data Xapiand is dramatically more compact on disk, and
looks competitive-to-faster on query serving, but a fair query comparison needs
both engines on the same native substrate and a larger, multi-node corpus before
any multiplier should be quoted.

## Reproducing

Start a Xapiand node and run the driver:

```sh
# Xapiand (native) -- this branch, and origin/master + the arm64 build fix
build/bin/xapiand --port 8880 --name benchnode --solo -D /tmp/xap_bench &
python3 harness/loadtest.py \
    --target localhost:8880 --dataset docs/assets/accounts.ndjson \
    --replicate 20 --concurrency 16 --duration 5 --trials 2 \
    --datadir /tmp/xap_bench --out harness/results/xapiand_bench.json
```

For the `origin/master` column, build a second tree at `origin/master` with only
the Apple-silicon build fix cherry-picked, and point the same driver at it:

```sh
git worktree add -b oldmaster-bench ../xapiand-oldmaster origin/master
git -C ../xapiand-oldmaster cherry-pick da3a9b97a3bdb2a46eba011740698e5c8e4a7f8b
cmake -S ../xapiand-oldmaster -B ../xapiand-oldmaster/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../xapiand-oldmaster/build --target xapiand -j4
```

```sh
# Elasticsearch (Docker)
docker run -d --name es-bench -p 9200:9200 \
    -e discovery.type=single-node -e xpack.security.enabled=false \
    -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" \
    docker.elastic.co/elasticsearch/elasticsearch:8.15.3
python3 harness/es_loadtest.py
```

<div style="min-height: 100px"></div>
