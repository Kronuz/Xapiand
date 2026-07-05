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
so the cost of the distributed data plane is explicit.

> **A benchmark bug once made this look ~60x worse than it is.** `loadtest.py`'s index
> phase wrote to per-trial index names (`{index}_t{tr}`) while its query phase queried
> the bare `{index}` name, which **never existed**. A search against a missing index
> still answers HTTP 200 with 0 hits, so the error count stayed at zero and hid it. On a
> cluster that empty search still fans out to the (missing) shards, and each miss cost a
> full remote handshake that ended in `DatabaseNotFoundError` ("Couldn't stat …"),
> retried `DB_RETRIES` times. That is what produced the old "~215 qps / ~79 ms p50"
> figure. `loadtest.py` now queries the populated index and aborts loudly if the probe
> query returns no hits, so this can't recur.

With that fixed, querying a *populated* index, the deciding factor turns out to be
**replica placement** (`num_replicas`), because it dictates whether a node reads the
shard locally or over the Remote protocol:

| `num_replicas` | node1 | node2 | reads |
| --- | ---: | ---: | --- |
| **0** (default) | ~1,900 qps, p50 4.7 ms | ~3,400 qps, p50 3.7 ms | one home node; other nodes go remote |
| **1** | ~11,600 qps, p50 1.1 ms | ~11,500 qps, p50 1.1 ms | every node holds a copy, reads locally |

(Solo, for reference: ~10,500 qps, p50 1.4 ms.)

**The cluster query path was never slow.** After the v47 remote protocol + two-phase
reconciliation, a cluster forms, replicates, and returns correct distributed results
(`cluster_check.sh` is green). With a replica on every node (`num_replicas = 1`) both
nodes serve at **solo speed** (~11,500 qps, p50 1.1 ms). A single uncontended cross-node
search is ~1.6 ms, not the ~12 ms the broken benchmark implied, and the remote path is
healthy: on a populated index it does **not** reconnect per query (profiling shows a
handful of `Xapian::Remote::open` frames, not the per-access churn the broken benchmark
produced).

**The 3-4x "penalty" is just an unreplicated shard.** With the default
`num_shards = 1, num_replicas = 0` a shard has a single home node, so any query that
lands elsewhere pays the remote two-phase match (serialize the query, one round-trip to
prepare, one to fetch the mset, deserialize) instead of a local read: p50 rises from
~1.1 ms to ~4 ms. It is `endpoint.is_local()` in `Shard::reopen_readable()` that decides
this: the owner reads locally, a non-holder reads remotely. A node's *incidental*
on-disk copy (left over even at 0 replicas) is deliberately **not** used, because at 0
replicas that node isn't a designated holder and wouldn't receive updates, so serving it
would be stale. Give the shard a real replica on that node (`num_replicas >= 1`, which
you want for availability anyway) and the local-fallback serves it locally at solo speed.

Replication is the first lever (a node that holds the shard reads it locally at solo
speed), but it is not the *only* one: forcing every read through the Remote protocol
(`num_replicas = 0`, query a non-holder) exposes real per-query overhead worth cutting,
and that is a portable win (it helps any deployment with more shards than replicas, where
some node must always read some shard remotely).

### Optimizing the forced-remote path

Instrumenting the client's `send_message` on the forced-remote path (accounts, c=16)
showed **~8.7 protocol round-trips per query**:

| message | round-trips / query | what it is |
| --- | ---: | --- |
| `MSG_UPDATE` | ~4.3 | fetch global match stats (doccount, total_length, doclen bounds, ...) |
| `MSG_DOCUMENT` | ~2.3 | fetch each result document's data |
| `MSG_QUERY` | 1 | two-phase match, prepare |
| `MSG_GETMSET` | 1 | two-phase match, finalize |
| `MSG_REOPEN` | ~0 | (only on the pool's staleness window) |

The `MSG_UPDATE` storm was pure waste. A read-only remote database is pinned to a
revision until `reopen()`, so its stats can't change under it, yet every
`get_doccount()` / `get_total_length()` the coordinator called to build global match
stats did a fresh round-trip. Caching them (`stats_valid`, invalidated only by
`reopen()`; writers never cache) collapsed **`MSG_UPDATE` from ~267,000 to 3 over a run**,
cut round-trips to ~4.4/query, and lifted the forced-remote node from ~1,900 to ~2,460
qps (p50 4.7 → 3.66 ms) and the other node from ~3,400 to ~5,200 qps (p50 3.7 → 2.37 ms),
with distributed correctness unchanged (`cluster_check.sh` green, hit counts identical).
This is a general Xapian-remote inefficiency, not Xapiand-specific, so it is a candidate
to upstream.

What is left is architectural: the two-phase `MSG_QUERY` + `MSG_GETMSET` (inherent to the
non-blocking distributed match) and ~2.3 `MSG_DOCUMENT` per query (a separate round-trip
per result document, fetched lazily during result rendering). The next lever is to stop
paying a round-trip per document: pipeline or batch the per-hit fetches so the render
loop's `N` documents cost ~1 round-trip instead of `N`. That is the bigger, still-portable
win, but it threads a batch API across the handler -> shard -> remote layers and touches
the connection's request/reply lockstep, so it wants a deliberate design pass.

Two things this investigation ruled out. Reusing the remote handle across the pool's
staleness window (instead of reconnecting) only trades a `Xapian::Remote::open` for an
`MSG_REOPEN` round-trip, so it is qps-neutral. And the remote path does **not** deadlock:
hammered at 48-way concurrency (single- and multi-shard) it sustains thousands of
queries/second with no stall. Earlier apparent "hangs" were the measurement scaffolding
(a shell `wait` on a backgrounded load plus `sample` suspending the process), not the
server.

Index throughput on a cluster is lower than solo (writes replicate and pay first-touch
shard creation across nodes), which is expected. These are measurements on a fresh cluster,
no tuning; the point is to make the real cost visible, not to quote a production multiplier.

### Parallel fan-out: one native Enquire instead of shard-by-shard

The coordinator used to run a distributed search **shard by shard**: one
single-database `Xapian::Enquire` per shard, driven by a serial loop that blocked on
each shard's two-phase conversation before starting the next. So a `K`-shard search
paid `K` prepare round-trips one after another, then `K` finalise round-trips one after
another -- the fan-out latency was the *sum* over shards, not the max.

That was a workaround. Xapian's own matcher already fans a multi-database search out
efficiently: it sends every shard's `MSG_QUERY` up front, does local work, then collects
replies with `poll()` in readiness order (and merges the per-shard MSets and unshards the
docids itself). Xapiand had abandoned that native path years ago "because it will not work
in cluster mode" -- the old remote server did the whole match in one blocking call the
reactor can't hold, and the matcher's single `query_id` collided for shards co-located on
one node. Both blockers are now gone (the server is a non-blocking two-phase, and the
`query_id` is made per-shard-unique), so the search feeds every shard into **one** Enquire
again and lets Xapian overlap them.

Single-query latency, 2-node localhost cluster, `replicas=0` (forcing the Remote path):

| shards | serial (old) | one Enquire (new) | speedup |
| ---: | ---: | ---: | ---: |
| 2 | 0.80 ms | 0.79 ms | ~1x |
| 4 | 0.93 ms | 0.83 ms | 1.12x |
| 8 | 1.13 ms | 0.87 ms | 1.30x |
| 16 | 1.67 ms | 1.00 ms | **1.67x** |

Serial latency climbs roughly linearly with shard count; the native match stays nearly
flat. The gap widens with shard count, and with network RTT: on loopback each round trip
is tens of microseconds, so this understates the win: on a real cluster with millisecond
links the serial curve is `K` times steeper while the overlapped one barely moves, so it
approaches a per-shard-count speedup. Correctness is unchanged (identical hits, count,
sort, pagination against a solo node; identical aggregations once `check_at_least` is high
enough to check every document; `cluster_check` green at 2 and 3 nodes; no stall under
concurrency), and it deletes the hand-rolled fan-out and manual merge in favour of
Xapian's.

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
