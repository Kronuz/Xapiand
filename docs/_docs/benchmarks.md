---
title: Benchmarks
---

Results of running Xapiand against Elasticsearch, and against its own previous
release, on the same dataset and query mix. Xapiand serves this workload in about
**a third of the memory** Elasticsearch needs, keeps a **smaller index on disk**,
and is at **full speed from the first query** (no warm-up), while matching
Elasticsearch on warm query throughput and latency.


## Results

Single node, one shard, **20,000 documents**, a fixed mix of seven queries driven
by 16 concurrent clients. Query figures are the warm, steady-state numbers.

| Metric | **Xapiand** | Xapiand 0.40.0 | Elasticsearch 8.15.3 |
| --- | ---: | ---: | ---: |
| Query throughput (warm) | ~11,500 qps | ~11,300 qps | ~11,400 qps |
| Query latency p50 | 1.29 ms | 1.32 ms | 1.32 ms |
| Query latency p95 | 2.40 ms | 2.41 ms | 2.35 ms |
| Query latency p99 | 3.21 ms | 3.30 ms | 2.98 ms |
| First-pass query (cold) | ~11,500 qps, p99 2.9 ms | ~11,500 qps, p99 2.9 ms | ~9,100 qps, p99 6.2 ms |
| Index throughput | ~7,800 docs/s | ~6,300 docs/s | ~13,200 docs/s |
| **Memory (resident)** | **~470 MB** | ~1.5 GB | ~1.4 GB |
| On-disk size | **9.2 MB** | 13.7 MB | 17.3 MB |

**Memory.** This is the clearest difference. Xapiand holds the whole running index
in **~470 MB** of resident memory; Elasticsearch needs **~1.4 GB** for the same
data, roughly **3x more**. Xapiand memory-maps its storage and keeps a small
private working set, so most of its memory is reclaimable page cache the operating
system can reuse; the Java heap Elasticsearch commits is not.

**Warm-up and stability.** Xapiand runs at full speed from the very first query.
Elasticsearch's first pass is about **20% slower with roughly double the tail
latency** (p99 6.2 ms vs 2.9 ms) while the JVM warms up, and its warm throughput
varies more from run to run. Xapiand's throughput is steady from the first trial.

**On-disk size.** Xapiand stores the same 20,000 documents in **9.2 MB** against
Elasticsearch's **17.3 MB**, about **1.9x smaller**, thanks to its
Zstandard-compressed storage.

**Query throughput and latency.** Once Elasticsearch is warm the three are close:
all serve ~11,400 qps at ~1.3 ms median. Xapiand matches Elasticsearch here while
using a fraction of the memory.

**Indexing.** Elasticsearch's bulk path is faster on this workload (~13,200 vs
~7,800 docs/s). This is the one axis where Elasticsearch leads.


## What changed since 0.40.0

The current release against the previous one (0.40.0), same measurement:

| Metric | **Xapiand** | Xapiand 0.40.0 | Δ |
| --- | ---: | ---: | ---: |
| Index throughput | ~7,800 docs/s | ~6,300 docs/s | **+24%** |
| Memory (resident) | **~470 MB** | ~1.5 GB | **~3x less** |
| On-disk size | **9.2 MB** | 13.7 MB | **−33%** |
| Query throughput (warm) | ~11,500 qps | ~11,300 qps | +2% |
| Query latency p50 | 1.29 ms | 1.32 ms | −2% |

The modernization indexes faster, stores smaller, and above all runs far leaner:
the current release uses about **a third of the memory** the previous one did.


## Cluster

The numbers above are single-node. Across a cluster the two levers that matter are
replica placement and how a distributed search fans out to the shards.

**Replicas serve reads locally.** With a replica of every shard on every node, each
node answers searches from its own copy and serves at single-node speed
(**~11,600 qps, p50 1.25 ms** on a two-node cluster). A node that does not hold a
copy of a shard reads it from a holder over the internal protocol instead, so the
more replicas a shard has, the more nodes can serve it locally.

**Overlapped shard fan-out.** A distributed search feeds every shard into one
native match that sends all the shard queries up front and collects the replies in
readiness order, rather than querying the shards one after another. Single-query
latency therefore stays nearly flat as the shard count grows, instead of climbing
with it:

| shards | serial, one-by-one | overlapped fan-out | speedup |
| ---: | ---: | ---: | ---: |
| 2 | 0.80 ms | 0.79 ms | ~1x |
| 4 | 0.93 ms | 0.83 ms | 1.12x |
| 8 | 1.13 ms | 0.87 ms | 1.30x |
| 16 | 1.67 ms | 1.00 ms | **1.67x** |

The gap widens with both shard count and network latency, so on a real cluster with
millisecond links the overlapped curve stays flat while the serial one climbs `K`
times steeper.


## Methodology

- **Dataset.** The bundled `accounts` set (1,000 nested account records) replicated
  to 20,000 documents.
- **Query mix.** Seven queries covering the common ground: `match_all`, a term
  filter, a nested match, two numeric range filters, a keyword term, and a boolean
  AND. The same mix is translated to each engine's query language.
- **Load.** 16 concurrent clients; warm query numbers are reported (the cold
  first-pass figure is shown separately in the table).
- **Equal footing.** Both engines run natively, at the same shard count, on the
  same machine. Xapiand is built Release with Link-Time Optimization; Elasticsearch
  runs with a 1 GB heap and otherwise stock settings.
- **Memory.** Reported as each process's resident private memory (what it actually
  occupies, excluding reclaimable file-backed cache), so an engine that memory-maps
  its storage and one that commits a heap are compared on equal terms.

This is a single-node smoke test on a small dataset, not a tuned, production-scale
benchmark; it is meant to show the shape of each engine's behavior on the same
work, not to quote a production multiplier.


## Reproducing

Build Xapiand Release with LTO, start a node, and run the driver:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLTO=ON && cmake --build build --target xapiand -j
build/bin/xapiand --port 8880 --name benchnode --solo --shards 1 -D /tmp/xap_bench &
python3 harness/loadtest.py \
    --target localhost:8880 --dataset docs/assets/accounts.ndjson \
    --replicate 20 --concurrency 16 --duration 5 --trials 3 \
    --datadir /tmp/xap_bench --out harness/results/xapiand_bench.json
```

Run Elasticsearch natively from the official distribution and point the matching
driver at it (`NUM_SHARDS` matches Xapiand's `--shards`):

```sh
ES_JAVA_OPTS="-Xms1g -Xmx1g" elasticsearch-8.15.3/bin/elasticsearch &   # wait for a green /_cluster/health
NUM_SHARDS=1 REPLICATE=20 CONCURRENCY=16 DURATION=5 TRIALS=3 python3 harness/es_loadtest.py
```

The two-node cluster figures come from `harness/cluster_bench.sh`.

<div style="min-height: 100px"></div>
