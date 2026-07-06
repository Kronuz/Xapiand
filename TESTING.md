# Testing & Stressing Xapiand

How to test **and** stress every layer of Xapiand, what each layer proves, and how
to run it. The tools live in `harness/`; one command runs them all:

```sh
harness/run_all.sh                 # smoke + e2e + cluster + recovery + load + stress + bench
harness/run_all.sh smoke cluster   # just a subset
```

Everything here drives the built binary (`build/bin/xapiand`) over its real HTTP /
binary ports. There is no mocking: a "green" run means the actual server did the
work. Build first:

```sh
(cd build && cmake .. && make xapiand -j4)
```

---

## The layers at a glance

| Layer | Tool | What it proves | Needs |
| --- | --- | --- | --- |
| **smoke** | `run_all.sh smoke` | a single node indexes and searches at all | nothing |
| **unit** | `ctest` | C++ units (parsers, serialise, wal, query, ...) in isolation | `-DBUILD_TESTS=ON` + GTest |
| **functional E2E** | `e2e_check.sh` | the whole documented HTTP API still behaves (vs a baseline) | a baseline report + `newman` |
| **remote / cluster** | `cluster_check.sh` | discovery, the Remote protocol, replication, distributed search | a multicast-capable interface |
| **recovery** | `wal_recovery_check.sh` | WAL crash recovery: uncommitted writes survive a `kill -9` via open-time replay | nothing |
| **protocol sniff** | `proxy --xapian` | *see* the MSG_/REPLY_ wire between two nodes | two nodes |
| **load** | `index_fortune` | bulk indexing of many docs | nothing (falls back if no `fortune(6)`) |
| **stress / soak** | `stress_fortune` | correctness under sustained concurrency | nothing |
| **benchmark** | `loadtest.py` / `es_loadtest.py` | index throughput + query QPS/latency (vs Elasticsearch) | a dataset (bundled) |

The single-node HTTP E2E is the workhorse for correctness; the **cluster** layer is
the only one that exercises Discovery / Remote / Replication, so it's the net for
anything touching the distributed data plane. See `docs/_docs/benchmarks.md` for
recorded benchmark results.

---

## smoke

The cheapest sanity check: boot one `--solo` node, `PUT` a document with
`?commit=true`, search for a term in it, expect one hit. No baseline, no
dependencies. `run_all.sh` runs it first so a broken binary fails fast.

```sh
harness/run_all.sh smoke
```

---

## Unit tests (ctest)

C++ unit tests live in `tests/` (active) and `oldtests/` (a large legacy GTest
suite: `boolparser`, `compressor`, `serialise`, `wal`, `query`, `geospatial`,
`msgpack`, `url_parser`, ...). They're only built when you configure with
`-DBUILD_TESTS=ON`, which needs GoogleTest:

```sh
cmake -S . -B build -DBUILD_TESTS=ON      # (GTest must be findable)
cmake --build build --target check         # builds + runs via ctest
# or, after building:
(cd build && ctest --output-on-failure)
```

`run_all.sh unit` runs `ctest` and **skips** (doesn't fail) if no tests are
configured, telling you to reconfigure. The `oldtests/` suite is wired but marked
legacy in `CMakeLists.txt`; re-enable modules there as they're brought current.

---

## Functional E2E (the docs are the test suite)

Xapiand's documentation (`docs/_docs/*.md`, `docs/tests/*.md`) is a runnable HTTP
test suite: `docs_to_postman.py` turns the fenced request/response blocks into a
Postman collection, and `newman` runs it against a live node. This is the primary
correctness net for the API surface (schemas, query DSL, ranges, restore, ...).

```sh
# capture our node's report and diff it against the saved baseline
harness/e2e_check.sh
```

"Green" is **parity with the baseline**, not 100% pass: the doc suite carries a set
of pre-existing aspirational failures that fail on both our build and the baseline.
`e2e_check.sh` compares two ways — assertion outcomes (`e2e_diff.py`) and response
bodies (`bodydiff.py`) — and expects only a few volatile fields (auto-ids, node
runtime info, Prometheus counters) to differ.

The baseline report (`harness/results/e2e_base_7bd295b.json`, gitignored, ~61 MB)
is captured once from a reference binary:

```sh
harness/e2e_capture.sh ../xapiand-master-bench/build/bin/xapiand \
    harness/results/e2e_base_7bd295b.json
```

`run_all.sh e2e` **skips** if the baseline is missing.

---

## Remote / cluster E2E

`cluster_check.sh` spins an N-node cluster (default 2) on localhost and asserts the
three things the `--solo` E2E can never reach:

1. **Discovery** — every node converges on the full membership and one leader.
2. **Remote / replication** — a write on node1 is readable from the other nodes
   (shards distributed across nodes, reached over the Remote protocol / replicated).
3. **Distributed search** — a query on one node gathers hits across every node's
   shards (the two-phase remote match).

```sh
harness/cluster_check.sh          # 2 nodes
harness/cluster_check.sh 3        # 3 nodes
```

Gotchas (documented in the script): the nodes share one multicast discovery port
but need their own http / xapian(remote) / replica TCP ports; a fresh cluster is
slow to settle (~8 s/node, and the first write is slower while shards are created);
and multicast needs a multicast-capable interface up. Offline with no interface,
discovery can't rendezvous — that's an environment limit, not a bug.

### The Remote protocol 1:1 invariant

The client (`src/xapian/backends/remote/`) and the Xapiand server
(`src/server/remote_protocol_views.{h,cc}`) speak the same binary protocol, so their
message/reply tables **must** stay 1:1 with the vendored definition. Three files
must agree:

- `src/xapian/net/remoteprotocol.h` — the authoritative `message_type` / `reply_type`
  enums (protocol v47).
- `src/server/remote_protocol_views.h` — Xapiand's `RemoteMessageType` /
  `RemoteReplyType`; ordinals must match the above exactly or messages misroute.
- `harness/proxy` — the sniffer; it **auto-parses** `remoteprotocol.h` at runtime, so
  it can't drift.

The protocol **version** is deliberately separate. `remote_protocol_views.h` hand-
maintains `XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION` as an honest assertion of the layout
this server actually implements — it is **not** re-derived from the vendored header.
A mismatch then fails loud at the handshake (`Server supports protocol version N -
client is using M`) instead of silently misparsing a field that moved. Bump it only
after the layout has actually been reconciled to the new version.

### Sniffing the wire (`harness/proxy`)

`harness/proxy` is a small, dependency-free (stdlib-only) colorizing man-in-the-middle:
it listens on one TCP port, forwards every byte to a target `host:port`, and prints each
frame in both directions (`-->` sent, `<--` received). With `--xapian` it *decodes* the
binary Remote protocol as it passes, so you can watch the actual `MSG_*` / `REPLY_*`
conversation between two nodes: the two-phase match (`MSG_QUERY` → `REPLY_STATS`,
`MSG_GETMSET` → `REPLY_RESULTS`), per-document fetches (`MSG_DOCUMENT` → `REPLY_DOCDATA`),
stats refreshes (`MSG_UPDATE`), reopens (`MSG_REOPEN`), and the handshake. It **auto-parses
`src/xapian/net/remoteprotocol.h` at startup**, so its decode tables stay 1:1 with the C++
wire protocol (v47) — there is no second copy to drift.

```
usage: proxy [--xapian | --http] <listen_port> [<host>:]<target_port>
             (no flag = raw byte forward, no decode)
```

Modes: `--xapian` decodes the Remote protocol, `--http` decodes HTTP, and with no flag it
is a plain forwarder (raw bytes, useful for anything else).

**Worked example — watch one node query another's shards.** A node reaches a remote
shard over the *other* node's Xapian/Remote port (`--xapian-port`, default `9880`). Put
the proxy in front of that port and route the client node through it:

```sh
# 1. Server node holds the shards, Remote port on :9880 (queried by peers):
build/bin/xapiand --name A --xapian-port 9880 --port 8880 \
    --discovery-interface 127.0.0.1 -D /tmp/A &

# 2. Sniff its Remote port: listen on :8861, forward to the real :9880
harness/proxy --xapian 8861 localhost:9880

# 3. Start the client node so the shard it must fetch from A resolves to :8861
#    instead of :9880 (e.g. run A's advertised xapian-port as 8861, with the real
#    server behind the proxy), then search the client node and watch the proxy:
curl -s -XSEARCH localhost:8881/index/ -H 'Content-Type: application/json' \
    -d '{"_query":{"_match_all":{}}}'
```

Each line the proxy prints is one framed message, e.g. `A --> client REPLY_STATS <bytes>`
/ `client --> A MSG_GETMSET <query_id ...>`, so a broken or mismatched protocol shows up
immediately as an unexpected type, a version mismatch in the opening handshake, or a
conversation that stalls waiting for a reply that never comes. `Ctrl-C` stops it.

Related low-level probes: `discovery_sniff.py` (decode the multicast Discovery gossip),
`graceful_shutdown_check.sh` and `signal_check.sh` (signal handling / clean shutdown).

---

## Recovery — `wal_recovery_check.sh`

The durability net for the write-ahead log. Every write is appended to the per-database
WAL as it happens, *before* the debounced glass commit (the timed committer only forces
at ~8-10s, the count threshold at 100000). So a crash after some uncommitted writes must
be recoverable: reopening the data dir replays the WAL and restores every one of them.

The check makes that concrete: start a solo node, `PUT` N docs **without** `?commit=true`
(they live only in the WAL), `kill -9` the node within ~1s (a hard crash, well before any
timed commit), restart on the same data dir, and assert all N docs come back. It also
greps the restart log for the replay line (`Read and execute operations WAL`) as positive
proof the recovery went through the WAL rather than a lucky early commit.

```sh
harness/wal_recovery_check.sh            # 50 docs (default)
NDOCS=500 harness/wal_recovery_check.sh  # more writes
```

This is the modern, real-architecture equivalent of the retired `oldtests/test_wal.cc`,
which drove the long-gone 2019 `DatabaseQueue`/`Database` API by hand; here we exercise the
actual HTTP → WAL → crash → open-time-replay path end to end. The storage engine underneath
(`Kronuz/storage`, the append-only checksummed volume format the WAL is built on) has its
own fault-injection suite (torn writes, truncation, bit rot, a corrupt size field, and the
guarantee that records committed before any damage stay readable) — see that repo's
`test/test.cc`.

---

## Load — `index_fortune`

A quick functional bulk-load: `PUT` a range of documents built from random
"fortunes" (uses `fortune(6)` if installed, otherwise a small built-in corpus, so it
runs anywhere).

```sh
harness/index_fortune 'http://localhost:8880/fortune?commit=true' 1 1000
```

Good for populating an index fast and eyeballing write behaviour; not a measured
benchmark.

---

## Stress / soak — `stress_fortune`

Sustained concurrency with read-back verification: N workers loop `PUT` a document
into one of M databases, then `GET` it back and check the field round-trips. It
prints one status char per op so you can watch it live, and a summary (ops, errors
by kind, rate) at the end. Unlike the benchmark, this is the "does it stay up and
stay correct under load" net.

```sh
harness/stress_fortune --target localhost:8880 --workers 50 --databases 20 \
    --duration 30 --commit
harness/stress_fortune --ops 100000 --workers 200          # op-count instead of time
```

Methodology note: each worker owns a disjoint id band (`worker*docs_per_db + i`), so
no two workers ever touch the same document — otherwise a concurrent write would race
the read-back and report a bogus mismatch. A non-zero exit means real failures
(hard errors or a verified value that didn't round-trip); eventually-consistent 404
read misses are not counted as errors. This is the dependency-free rewrite of the
original `contrib/bin/stress_fortune`.

---

## Benchmarks

`loadtest.py` bulk-loads a dataset (the bundled `accounts` set, replicated) and then
hammers a fixed query mix from many threads, reporting index docs/s and query
QPS/latency percentiles:

```sh
harness/loadtest.py --target localhost:8880 --dataset docs/assets/accounts.ndjson \
    --replicate 20 --concurrency 16 --duration 5 --trials 2 --datadir /tmp/xap_bench
```

`es_loadtest.py` runs the same query mix against Elasticsearch for a side-by-side
(different substrate — read the caveats in `docs/_docs/benchmarks.md`). `perfdiff.py`
diffs two `loadtest.py` result JSONs.

**Cluster benchmark.** `cluster_bench.sh` runs the *same* load twice — once solo (no
Remote protocol) and once on a 2-node cluster (index sharded across nodes, so every
search runs the two-phase Remote match and every write replicates) — and prints both
sets of numbers with the delta, making the cost of the distributed data plane explicit:

```sh
harness/cluster_bench.sh
REPLICATE=20 CONCURRENCY=16 DURATION=5 TRIALS=2 harness/cluster_bench.sh
```

It needs a multicast-capable interface for discovery (like `cluster_check.sh`); if the
cluster can't form it still reports the solo numbers. See `docs/_docs/benchmarks.md`
for recorded solo-vs-cluster results.

---

## The unified runner

`harness/run_all.sh` orchestrates the layers above (it doesn't reimplement them),
starting each on its own fresh data dir, and prints one pass / fail / skip summary.
Layers whose prerequisites are missing (a baseline, the oracle binary, GTest) are
**skipped**, not failed, with a note on how to enable them.

```sh
harness/run_all.sh                 # default: smoke e2e cluster recovery load stress bench
harness/run_all.sh smoke stress    # a subset
BIN=/path/to/xapiand harness/run_all.sh
```

Exit code is 0 (GREEN) only if no layer failed.
