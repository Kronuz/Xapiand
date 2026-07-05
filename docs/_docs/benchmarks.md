---
title: Benchmarks
---

A single-host, native comparison of Xapiand against Elasticsearch on the same
dataset and query mix, plus the internal before/after numbers for the work on this
branch. Both engines run **natively on the same Apple-silicon machine** (no Docker,
no VM), and shard count is held **equal** on both sides, so the cross-engine
comparison is apples-to-apples.

The honest headline: on this workload native Xapiand and Elasticsearch are close.
Query serving is a tie at one shard (~12k qps, ~1.2 ms p50 on both) and swings
Xapiand's way as shards multiply (its local fan-out is cheaper than ES's);
Elasticsearch indexes faster; on-disk footprint is within ~1.25x at one shard and
too sensitive to background segment-merge state to call at higher shard counts.
Xapiand's material wins are architectural and show up in the cluster section below,
not in the single-node smoke numbers.

An earlier version of this page claimed Xapiand was "~5x smaller on disk" and
"~1.8x faster on queries." Both were artifacts: the ES side ran in Docker (paying a
VM and loopback-network tax) and the two sides were compared at different shard
counts (Xapiand's default 5 vs ES's default 1). Re-run natively and at equal shard
count, those headlines do not hold. This page documents what does.

---

## Setup

Both engines indexed the bundled `accounts` dataset (1,000 records of nested
account objects: name, age, gender, balance, nested `contact` address, a text
`personality` field, a geo point) replicated to **20,000 documents**, then served
a fixed mix of seven queries at **concurrency 16 for 5 seconds, three trials**.

The query mix is the same on both sides, translated to each engine's query
language:

- `match_all`
- a term filter (`gender = male`)
- a nested match (`name.firstName = Michael`)
- two numeric range filters (`age` in 20..30, `balance` in 1000..3000)
- a keyword term (`contact.state = California`)
- a boolean AND (`gender = male` AND `age` in 25..40)

Xapiand is driven by `harness/loadtest.py`; Elasticsearch by
`harness/es_loadtest.py` (`NUM_SHARDS=N` to match shard count). Both replicate the
dataset to the same count, bulk-load it, then hammer the query mix from 16 threads
and report QPS and latency percentiles. Query numbers are from a warm run.

Both engines run **natively** on the same Apple-silicon machine: Elasticsearch
8.15.3 from the official aarch64 tarball (`discovery.type: single-node`, security
off, 1 GB heap), no Docker. Every Xapiand build on this page is compiled **Release
+ LTO**, matching how Elasticsearch and the `origin/master` baseline are built (the
`LTO` CMake option defaults on; leaving it off, as a fast dev build does, costs
Xapiand ~10% and silently handicapped an earlier draft). The index is created with
an explicit shard count on both sides so the two are compared at **equal shards**.

## Xapiand vs Elasticsearch (native, equal shards)

Both engines native on the same machine, same 20k documents, measured at one shard
and at five shards so the effect of sharding is visible on each side. Query numbers
are the warm trials; on-disk is the settled footprint (actual bytes).

| Metric | Xapiand 1-shard | ES 1-shard | Xapiand 5-shard | ES 5-shard |
| --- | ---: | ---: | ---: | ---: |
| Query throughput (warm) | ~12,270 qps | ~11,950 qps | ~11,700 qps | ~7,470 qps |
| Query latency p50 | 1.20 ms | 1.27 ms | 1.26 ms | 1.98 ms |
| Query latency p95 | 2.28 ms | 2.25 ms | 2.40 ms | 3.78 ms |
| Index throughput | ~6,300 docs/s | ~17,000 docs/s | ~11,800 docs/s | ~24,000 docs/s |
| On-disk (settled) | 13.7 MB | ~17 MB | 9.5 MB | 3.7–19 MB (see below) |

**Query serving: a tie at one shard, Xapiand's at five.** At one shard the two are
indistinguishable (~12k qps, ~1.2 ms p50). At five shards Xapiand holds ~11,700 qps
while ES drops to ~7,470 at ~2 ms p50: a single-node ES still coordinates a
per-shard query and merges the results, and that fan-out is more expensive than
Xapiand's native multi-database merge (the same machinery the cluster section leans
on). So Xapiand's query edge here is an *at-scale-of-shards* edge, not a raw
single-shard-engine edge.

**Indexing: Elasticsearch is faster.** Lucene's bulk path out-indexes Xapiand's
`RESTORE` ingest at both shard counts (~17k vs ~6k docs/s at one shard, ~24k vs ~12k
at five). The old page called index throughput "a wash"; that was the Docker tax on
ES flattering Xapiand. Natively, ES indexes faster. (Both engines index *faster*
with more shards, from write parallelism across shards.)

**On-disk: within ~1.25x at one shard, unstable at five.** At one shard the
footprint is steady and Xapiand is ~1.25x smaller (13.7 MB vs ES's ~17 MB, still
~16.8 MB after a force-merge to one segment). At five shards ES's reported store
swings between ~3.7 MB and ~19 MB across otherwise-identical runs, depending on how
far background segment merging has progressed at the instant it is measured, so
there is no honest five-shard number to quote; Xapiand's own five-shard footprint is
a steady ~9.5 MB. The takeaway is not a footprint winner but that footprint on this
duplicative dataset is dominated by shard count and merge state. The old "~5x
smaller" was an artifact: an undercounted Xapiand 5-shard measurement (the harness
read the index mid-flush; since fixed) compared against an ES 1-shard.

## What this branch changed (vs origin/master)

Beyond the cross-engine comparison, the more controlled number is Xapiand against
its own past: this branch versus `origin/master` (upstream `0.40.0`, Xapian
1.5.0-dev) with only the Apple-silicon build fix cherry-picked on top (commit
`da3a9b97a`). Everything this branch did lands in the gap: the Xapian 1.5.0-dev →
2.0.0 engine upgrade, the de-vendoring into a library tree, and the Asio reactor
runtime. Both binaries are **Release + LTO**, default shard count, measured
back-to-back on the same machine (20k docs, concurrency 16, 5s, three trials).

| Metric | This branch (2.0.0, LTO) | `origin/master` (1.5.0, LTO) | Δ |
| --- | ---: | ---: | ---: |
| Index throughput | ~11,870 docs/s | ~10,090 docs/s | **+17.7%** |
| Query throughput (warm) | ~11,820 qps | ~10,590 qps | **+11.6%** |
| Query latency p50 | 1.25 ms | 1.42 ms | **−12%** |
| Query latency p95 | 2.36 ms | 2.45 ms | −3.5% |
| Query latency p99 | 3.22 ms | 3.09 ms | +4.3% |
| On-disk size (20k docs) | ~9.5 MB | ~9.5 MB | ~0% |

The modernization **indexes ~18% faster and serves ~12% more queries at ~12% lower
p50**, with tail latency within noise and footprint unchanged. An earlier draft of
this page reported query throughput slightly *behind* `origin/master` (−2.7%); that
was not the engine but a build artifact. The dev build being measured had `LTO`
off while the baseline (built with defaults) had it on, so the new code was racing
with a hand tied behind its back. Rebuilt with LTO on both sides, the query gap
inverts into a ~12% lead. Correctness held: the full end-to-end suite runs green
against the 1.5.0 oracle with **zero regressions**, and 2.0.0's improved stemming
and stopword handling actually fixed five assertions that fail on `origin/master`
(default-operator matching, query-time stopword filtering, a complex-object info
case, and a typed-content selector).

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

(Solo, for reference: ~11,700 qps, p50 1.23 ms on the LTO build.)

> **On these cluster numbers and LTO.** The forced-remote and fan-out figures in
> this section were captured during development on a non-LTO build (fast to
> rebuild). They are *A/B* results (before vs after a change, or serial vs
> overlapped), and LTO shifts both sides together, so the ratios and the
> conclusions hold; only the absolute qps would rise a few percent with LTO. The
> headline solo and fully-replicated throughput above is confirmed on the LTO build
> (~11,700 qps, p50 1.23 ms).

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

This is a smoke test. It is now apples-to-apples on the two things that most
distorted the earlier version (substrate and shard count), but several caveats
remain:

- **Same substrate, at last.** Both engines run natively on the same Apple-silicon
  machine; Elasticsearch is the official aarch64 build, not a Docker image under a
  Linux VM. The earlier Docker setup made ES pay a VM and loopback-network tax on
  every request and flattered Xapiand's latency and QPS. That tax is gone here, and
  it is why the native ES numbers are so much more competitive than the old page's.
- **Highly duplicative dataset.** The corpus is 1,000 distinct records replicated
  20x. That compresses and caches far better than real data would, and it is the
  main reason the on-disk numbers are both small and shard-count-sensitive. A
  non-duplicative corpus would move all of these numbers.
- **On-disk footprint is not robust here.** As measured, ES's five-shard store
  swings several-fold between identical runs depending on background segment merging,
  and Xapiand's harness once undercounted a sharded index by reading it mid-flush
  (now fixed). Treat the one-shard footprint (steady, Xapiand ~1.25x smaller) as the
  only defensible footprint result on this page.
- **Small, single-host.** 20,000 documents on one machine. Nothing here speaks to
  multi-node scaling or large corpora; the cluster section measures the distributed
  data plane's *cost*, not a production throughput multiplier.
- **Default configuration.** Stock settings on both sides (1 GB heap for ES, no
  merge/refresh tuning, no Xapiand tuning). Elasticsearch also does more work by
  default (per-field `.keyword` multi-fields, its own analysis chain).
- **Build.** Every Xapiand binary here is Release + LTO, matching ES and the
  baseline. Warm query numbers only (ES's first trial pays JVM warmup; Xapiand is
  steady from the first).
- **Feature surface.** The two are not feature-identical. This mix exercises the
  common ground (term, range, nested, boolean), not the full surface of either.

The honest summary: natively and at equal shard count, Xapiand and Elasticsearch
are close on this workload. Elasticsearch indexes faster; query serving is a tie at
one shard and tilts Xapiand's way as shards multiply; on-disk footprint is
inconclusive beyond a modest one-shard edge. Xapiand's material advantages are
architectural (the compact single-shard index, the cheap native multi-database
merge, and the cluster/Remote-protocol work below), not a single-node smoke-test
multiplier. A larger, non-duplicative, multi-node corpus is needed before quoting
any headline number.

## Reproducing

Build Xapiand **Release + LTO** (the `LTO` option defaults on, so a plain Release
build already has it; a fast dev build that turned it off must turn it back on for
benchmarking), then start a node at a chosen shard count and run the driver:

```sh
# Xapiand (native), Release + LTO
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLTO=ON && cmake --build build --target xapiand -j
build/bin/xapiand --port 8880 --name benchnode --solo --shards 1 -D /tmp/xap_bench &
python3 harness/loadtest.py \
    --target localhost:8880 --dataset docs/assets/accounts.ndjson \
    --replicate 20 --concurrency 16 --duration 5 --trials 3 \
    --datadir /tmp/xap_bench --out harness/results/xapiand_bench.json
```

For the `origin/master` comparison, build a second tree at `origin/master` with only
the Apple-silicon build fix cherry-picked, and point the same driver at it (default
`Release` already implies LTO):

```sh
git worktree add -b oldmaster-bench ../xapiand-oldmaster origin/master
git -C ../xapiand-oldmaster cherry-pick da3a9b97a3bdb2a46eba011740698e5c8e4a7f8b
cmake -S ../xapiand-oldmaster -B ../xapiand-oldmaster/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../xapiand-oldmaster/build --target xapiand -j
```

Run Elasticsearch **natively** (not Docker), from the official aarch64 tarball, and
point the ES driver at it (`NUM_SHARDS` matches Xapiand's `--shards`):

```sh
# Elasticsearch (native)
curl -sLO https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-8.15.3-darwin-aarch64.tar.gz
tar xzf elasticsearch-8.15.3-darwin-aarch64.tar.gz
cd elasticsearch-8.15.3
printf 'discovery.type: single-node\nxpack.security.enabled: false\n' >> config/elasticsearch.yml
ES_JAVA_OPTS="-Xms1g -Xmx1g" bin/elasticsearch &   # wait for a green /_cluster/health
# back in the repo:
NUM_SHARDS=1 REPLICATE=20 CONCURRENCY=16 DURATION=5 TRIALS=3 python3 harness/es_loadtest.py
```

The two-node cluster numbers come from `harness/cluster_bench.sh` (see also
`BIN=build/bin/xapiand harness/cluster_bench.sh` to point it at a specific build).

<div style="min-height: 100px"></div>
