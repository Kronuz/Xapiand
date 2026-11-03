> Status: DRAFT (2026-11-03) revision 0, Phase 8b's closing benchmark: Xapiand on strata against the last Xapian release, Elasticsearch and Quickwit on one Linux x86-64 metal host. State: designing (ready for preliminary review). Workstream `design/closing-benchmark`, ledger `docs/status/closing-benchmark.md`.

# Closing benchmark (Phase 8b)

## Summary

One run, once, before the release: Xapiand on strata (S) against Xapiand on Xapian (X, the last
Xapian release), Elasticsearch (E) and Quickwit (Q), through each system's HTTP API, on every axis
the release will claim: indexing, queries by class, many small indexes, mixed load, restart and
recovery, memory and disk. Every system gets the same cores, memory and disk, is configured as its
own documentation recommends, and runs pinned to a recorded version. The published numbers come
from an AWS `c7i.metal-24xl`; a `c7a.8xlarge` dry run proves the harness first.

The harness is the one Xapiand already has, given a system adapter: `loadtest.py` (indexing,
latency per class), `stall_load.py` (open-loop throughput and mixed load), `wide_search_check.py`
(many indexes) and a `baseline_7c.sh`-shaped driver. `es_loadtest.py` folds into the adapter and is
deleted. The engines-alone pair (strata against Xapian 2.1.0) runs prospect's existing harness on
the same lease.

## What this reads, depends on and changes

| Document | Relation |
|---|---|
| `docs/status/road-to-release.md:33` (Phase 8b: strata against Xapian 2.1.0, and X against S end to end, "Review: none") | **Changes it:** adds E and Q, the metal lease, and this design's preliminary review. The run itself stays unreviewed (published numbers); the lead runs it |
| `docs/status/road-to-release.md:32` (Phase 8a: `sortable_serialise` replaced by the 8-byte map) | **Depends on it:** S is measured after 8a lands, since the codec sets range and sort cost |
| strata `docs/PLAN.md:691` (the closing benchmark: two pairs, every axis, once) | Implements both pairs; adds E and Q to the end-to-end pair. PLAN's Phase 7 rows against E and Q at 3, 30 and 300 nodes stay 7b's; this run is single-node and does not replace them |
| strata `docs/PLAN.md:182` (Margins: queries ≤ 0.7×, indexing ≥ 1.3×, memory ≤ 0.7×, disk ≤ 0.8×) | The "by the margin" level of D11 |
| strata `docs/BENCHMARKS.md:19` (noise band), `:46` (equal semantics), `:137` (query speed), `:155` (memory in cgroup v2), `:161` (disk) | Adopted as written, applied at system level (D5, D8, D11). `:58` names `es_loadtest.py` as the E harness; D7 replaces it, and strata's line is updated in the same step |
| strata `docs/design/product-landscape.md:173` (T2 many indexes) and `:175` (T4 memory against E) | T8 to T10 and T11 here are those rows, single-node, at 10^4 and 10^5 tenants |
| strata `docs/perf/c1-field-tenants.md` (C1: E index-per-tenant hit the 1,000-shard limit, `:45`; about 6.4 creates/s with it raised, `:46`) | The many-index rows reuse C1's method: a fixed total of documents split into more or fewer tenants, each system in its documented multi-tenant shapes |
| `docs/design/phase7c.md` rows T4, T5, T12 and `docs/perf/phase7c-baseline.md` | Same tools (`stall_load.py`, `loadtest.py`); no 7c row changes |
| `docs/gates/phase7c-linux-dryrun.md` | Facts used: clang 20 and libc++ 20 build Xapiand, clang 18 does not (`:4`, `:17`); one Python generator is bound above about 3,200 requests/s (`:35`) |
| campaign `references/measurement.md`, `references/lease.md` | Smoke, screen and confirm; run-level intervals with an A/A; metal takes the whole 96-vCPU quota |

## Decisions

**D1. Four systems under test, versions pinned and recorded.**

| | System | Build |
|---|---|---|
| S | Xapiand on strata | master at Phase 8a's landing, Release with LTO, clang 20 and libc++ 20 |
| X | Xapiand on Xapian | the last Xapian release (Q1). The `v1.0.0-alpha.10` tag (`Xapiand:3fe13a2fb`) does not build on Linux; the fixes are on the `xapiand` branch (`Xapiand:27191e0ff` and 21 more commits) |
| E | Elasticsearch | the newest 9.x GA tarball with its bundled JDK on the dry-run day, sha512 checked (C1 ran 9.5.5) |
| Q | Quickwit | the newest release binary on the dry-run day (v0.9.3 is marked latest on its releases page today) |

Both Xapiand binaries are built once on the dry-run lease and copied byte for byte to the metal
lease, as http's T6 run did (`http:docs/perf/t6-t7-2026-11-03.md`); every result records each
binary's sha256 and `file` output.

**D2. Equal resources, one system up at a time.** Each system runs in its own cgroup v2 slice
(`systemd-run --scope`) with `cpuset.cpus` = 8 physical cores (one hardware thread each, the SMT
siblings left idle) and `memory.max` = 32 GiB (Q5). The load generator runs on 16 other physical
cores, siblings idle. Every system is told its core count where it has a setting (Xapiand
`--processors 8` (`src/opts.cc:189`), E `node.processors: 8`), and the preflight asserts the
effective value each reports. One shard per index everywhere: Xapiand's default is 5
(`src/opts.h:75`, `--shards` at `src/opts.cc:153`), E's is 1, and E's sizing guidance puts a
shard at 10 to 50 GB (landscape `es9`), above both corpora's indexes. Plain HTTP over loopback,
keep-alive, E's security off, no TLS anywhere.

**D3. Corpora are public, fetched by script and pinned by sha256 at the dry run.**

| Corpus | Source | Size | Used by |
|---|---|---|---|
| C-wiki | search-benchmark-game's English Wikipedia (`make corpus`: `wiki-articles.json.bz2` through `corpus_transform.py`) | 5,032,104 documents (prospect `docs/perf/format13-gate-vm.md:14`), 8.33 GB (strata `docs/research/indexing.md:118`) | T1, T3, T4 (text classes), T7, T11 |
| C-wiki queries | search-benchmark-game `queries.txt` (AOL queries, tagged term, intersection, union, phrase), sampled with prospect `bench/sample_queries.py` (40 per class, seed 7) | 160 queries | T4, T5 |
| C-logs | Elastic Rally's `http_logs` track: `documents-181998.json.bz2` and `documents-191998.json.bz2` | 2,708,746 + 9,697,882 = 12,406,628 documents, 0.36 + 1.30 = 1.67 GB uncompressed (the track's `track.json`; publisher figures, checked at the dry run) | T2, T3, T4 (range, aggregation, sort), T6, T7 |
| C-logs writes | the same track's `documents-201998.json.bz2` | 13,053,463 documents, 1.74 GB (publisher figures) | T6's write stream |
| C-tenants | C-wiki's first 1,000,000 documents in order, cut into 10,000 tenants of 100, or 100,000 tenants of 10 | 1,000,000 documents either way, as C1 held the total fixed | T8 to T10 |

`accounts.ndjson` is not used: `loadtest.py` replicates its 1,000 lines (`harness/loadtest.py:55`),
which flatters compression and term statistics. Fields used from C-logs: `@timestamp` (date),
`status` (integer, also keyword-like terms), `size` (integer), `request` (text), `clientip`.

**D4. Each system configured by its own documentation, every setting checked in.** Configuration
lives in `harness/closing/` (one file per system), and every result embeds the effective settings
read back from the running system.

- **E:** heap by ES's own auto-sizing for the 32 GiB cgroup (half, under the compressed-oops
  ceiling, landscape `es14`), `-Xms` equal to `-Xmx`, memory lock on; one shard, no replicas.
  During T1 and T2 the bulk-load guidance (elastic.co, "Tune for indexing speed"):
  `refresh_interval: -1` then a refresh, replicas 0, bulk size found by doubling from 100 documents
  until throughput levels, senders added until CPU or disk saturates. Every other row uses the
  default 1 s refresh. Mappings: `text` with the standard analyzer for text, `keyword` for terms,
  `long` and `date` for numbers and times.
- **Q:** index config with `record: position` on text (phrases need it), fast fields on every
  field used by range, aggregation and sort, the default tokenizer; `commit_timeout_secs` at its
  default (60) except in T6, which also runs it at 1 s and says so. Relevance queries pass
  `sort_by: _score`, since Q does not score by default (landscape `qw3`). Ingest through its
  ingest API with `commit=force` on the last batch of a load.
- **S and X:** each binary's documented options with the core count and one shard; text fields
  without stemming or stop words; `--database-pool-size` (`src/opts.cc:159`) sized to the
  tenants open in T8 to T10 and recorded.
- **Analysis parity:** lowercase, Unicode word splitting, no stemming, no stop words, positions on,
  BM25 with each system's default parameters. Where a tokenizer still differs, D8's count check
  catches the queries it changes.

**D5. Durability: compared at one tier, the other reported beside it.** Xapiand acknowledges a
write before its WAL reaches disk: `DB_SYNCHRONOUS_WAL` exists (`src/database/flags.h:42`) and is
read on every WAL write (`src/database/shard.cc:747`), but nothing in `src/` sets it, so no
request can ask for durable-on-acknowledgement. The comparison tier is therefore "acknowledged
before fsync": E with `index.translog.durability: async`, Q with `commit=auto`, S and X at their
default. E's default (`request`, an fsync per bulk) runs as a report row in T1, T2 and T6 (Q2).

**D6. The machine: `c7i.metal-24xl` for the numbers, `c7a.8xlarge` for the dry run.** The metal
host is an Intel Xeon Platinum 8488C, 48 cores, one NUMA node (http's T6 lease meta); turbo off
(`intel_pstate/no_turbo=1`), governor `performance`, both recorded. It takes the whole 96-vCPU
quota, so the lead agrees the day with other sessions (lease.md, "Metal takes turns"). Storage is
EBS: the policy allows only `c7a`, `c7i`, `c7g`, `c8g`, `t3` and `t4g`
(`bench-lease-policy.json:31`), none with instance NVMe, and `lease` launches a fixed 100 GB gp3
root (`lease.sh:138`) at gp3's baseline throughput. Both corpora, four systems' indexes, E's and
Q's binaries and the tenant sets need more room and throughput than that (Q4). The preflight
records the volume's fsync latency (`fio`, 4 KiB write plus fsync, 30 s) beside every result.

**D7. One harness shape: Xapiand's harness plus one adapter module.** `harness/systems.py` holds
one adapter per system (S and X share Xapiand's): start and stop in the cgroup, create an index
from a neutral schema, bulk load, commit or refresh, search from a neutral query spec, read
counts, disk bytes and the effective settings. The neutral query spec (`harness/closing/queries/`)
is one JSON line per query (`class`, `field`, `terms` or bounds, `command`), translated per
system: Xapiand's DSL, E's DSL, Q's search API. Then:

- `loadtest.py` gains `--system`, `--corpus`, `--queries`, multi-client batched indexing
  (`--clients`, `--batch-docs`) and a concurrency-1 latency phase per class. Its index phase today
  is one RESTORE request (`harness/loadtest.py:93`) and its query phase a closed loop
  (`:114`). `es_loadtest.py` is deleted; its callers (`docs/dev/testing.md:32`, `:260`, strata
  `docs/BENCHMARKS.md:58`) move to `loadtest.py --system es`.
- `stall_load.py` gains `--system` and `--procs N`: its open-loop schedule is split across N
  generator processes, since one is bound above about 3,200 requests/s on Linux
  (`docs/gates/phase7c-linux-dryrun.md:35`). Its `--saturate` (`harness/stall_load.py:875`) is T5's
  search for the highest rate that holds the latency bound.
- `wide_search_check.py` gains `--system` and the tenant phases of T8 to T10; its `gate` mode
  (10,000 indexes, p50 and p99, peak pinned indexes) is T10 for S and X.
- `engine_mixed_load.py` gains `--system` and `--kill-at` for T7's crash leg; its model check
  (every acknowledged write present) is T7's audit for S and X.
- `harness/closing_bench.sh` and `harness/closing_report.py` run and tabulate, as
  `baseline_7c.sh` and `baseline_report.py` do for 7c. Ratios and intervals come from
  cpp-libraries' `tools/ab_compare.py` with `--aa`.

No second framework: Rally, `esrally` tracks and Quickwit's own benchmark are sources of corpora
and query shapes only.

**D8. Correctness beside speed.** Before timing, every query runs once on every system:

1. Counts must agree. A query whose count differs by more than 1% between any two systems is
   dropped from every system's timed set and listed with the reason; more than 5% dropped in a
   class stops the run until the analysis config is fixed.
2. S against X: counts equal, and top-10 ids compared in order, as `harness/search_order_diff.py`
   does for the docs suite's newman reports; every difference is listed as a known behavior change
   or a bug.
3. Aggregation results compared bucket by bucket (Q's terms counts are approximate, landscape
   `qw4`: its differences are listed, not dropped).

**D9. Stage every run: smoke, screen, confirm.** On the `c7a.8xlarge`: a 20,000-document dry run
of every row and system (each must give a non-empty, count-checked result), the smoke (every arm
20 to 30 s, effective settings asserted), and the screen (one round of every row at reduced size,
which also calibrates E's bulk size and its minimum heap for T11). On the metal: preflight, smoke
again, then confirm. Only confirm numbers are published.

**D10. Rounds, A/A and variance.** Five rounds per timed row (T4 to T7, T11), three for the
many-index rows. A round runs every system once in rotated order (S X E Q S', then X E Q S' S,
...), each from a fresh start with the page cache dropped, warmed by one untimed pass. S' is S's
binary again on a fresh data directory: the A/A control. Each row keeps every round's per-run
statistic; variance is reported as the spread over rounds and the A/A band (the range of S'/S).
Disk and memory bytes are single exact readings per round (median over rounds). Every run records
its phase times (start, load, warm-up, timed part, teardown) and the run's total.

**D11. What "better" means, per row.** For each row's metric in its own direction:

- The ratio is S over the best of X, E and Q on that row (beating the best beats the rest),
  per round, with a 95% bootstrap interval over rounds.
- **Better:** the interval excludes 1.0 on S's side, the median effect is at least 3%, and it lies
  outside the A/A band. **By the margin:** the interval's bound on S's side meets PLAN's Phase 6
  ratio for the axis (queries ≤ 0.7× the time, indexing ≥ 1.3× the throughput, memory ≤ 0.7×, disk
  ≤ 0.8×). **Tie:** inside the band. **Worse:** the interval excludes 1.0 against S.
- A system that cannot serve a row is marked "not supported" and left out of the best (Q has no
  update or delete by id and no document fetch by id). Neither a win nor a loss.
- Against X separately: no row may be worse. A worse row is a regression, published as one.

**D12. The engines-alone pair runs prospect's harness on the same lease.** After the system rows,
strata against Xapian 2.1.0 (PLAN's first pair) runs prospect's quick tier
(`bench/query_trials.sh` with `queries-sample` and `terms-sample`, scored by `bench/phase2/gate.py`)
and its indexing trials on 1M documents (`bench/index_trials.sh`), with Lucene and Tantivy beside
as the anchors they already are. Nothing here changes that harness.

## Gate rows

Rows are measured once, by the lead, at the commits D1 names. "Better" is D11's in every row.

| Row | Metric and direction | Procedure |
|---|---|---|
| T1 Indexing, text | Documents per second to searchable (higher), and CPU-seconds per 1,000 documents from `cpu.stat` (lower), at equal cores | `loadtest.py --system SYS --corpus wiki-1M` (C-wiki's first 1,000,000 lines): clients and batch per D4, timed from the first request to the commit or refresh after the last; peak `memory.current` recorded. Five rounds. Once per system, the full 5,032,104 documents (the index T4 and T7 use), reported, not ranked |
| T2 Indexing, structured | Same as T1 | The same on `documents-181998` (2,708,746 documents); the full C-logs once per system for T4 |
| T3 Index size | Bytes on disk per document (lower), WAL, translog and metastore included | `du` of each system's data directory after the load's commit and a settle (bytes stable 1 s, as `loadtest.py:70`), for C-wiki 5M and C-logs 12.4M; E force-merged to one segment as a report row |
| T4 Query latency by class | Per class, the geometric mean over its queries of each query's fastest of 10 runs (lower), for top-10 and count; p50 and p99 over all runs reported | `loadtest.py --latency` at concurrency 1 on the full indexes. Classes: term, boolean AND, boolean OR, phrase (C-wiki); range on `@timestamp` and on `size`, a term with a range (Rally's `200s-in-range`), aggregation (`date_histogram` by hour, terms on `status`, stats on `size`), sort (`@timestamp` descending and `size` ascending, with and without a term filter) (C-logs, after Rally's `http_logs` operations) |
| T5 Query throughput | Highest open-loop rate per class whose p99 stays at or under the class bound and whose achieved rate is within 1% of offered (higher); server CPU per query (lower) | `stall_load.py --system SYS --saturate --procs N`, one class at a time. Bounds: 25 ms for term, boolean, phrase, range and sort; 250 ms for aggregations |
| T6 Mixed read and write | Search p50 and p99 (lower) and write-acknowledgement p99 (lower) at a fixed offered load; visibility lag p99 (lower) | Open-loop writes of C-logs writes at 5,000 documents/s in batches of 100, beside open-loop searches of the T4 log classes at half of the slowest system's T5 rate; one probe document a second, timed from acknowledgement to the first search that finds it. 180 s timed after 30 s warm-up |
| T7 Restart and recovery | Clean restart: time from process start to the first answered search, and that search's latency (lower). Crash: the same after SIGKILL, plus lost acknowledged writes (must be 0 outside D5's tier's documented window) | Clean: stop under no load, drop the page cache, start, poll a T4 term query every 10 ms on both full indexes. Crash: `engine_mixed_load.py --system SYS --kill-at 60` under T6's load, restart, poll, audit |
| T8 Memory per open index | Anonymous and total `memory.current` per open tenant over the idle baseline (lower); descriptors per tenant (lower) | C-tenants at 10^4 and 10^5. Each system in its documented shapes: S and X index per tenant; E index per tenant (to its ceiling, raised only as its docs allow, the limit recorded) and one shared index filtered by a tenant field; Q one shared index with a tenant partition key, and index per tenant to the count it reaches in 30 minutes of creates. Create and load, query every tenant once, idle 60 s, read `memory.stat` and `/proc/PID/fd` |
| T9 Cold open | First-query latency for a tenant not yet touched since start, p50 and p99 (lower) | After T8, restart, drop the page cache, query 1,000 random tenants once each |
| T10 Wide search | One search across every tenant: p50 and p99 over 30 searches (lower); peak memory during them (lower) | `wide_search_check.py gate --system SYS --indexes 10000` (S and X); E and Q over their index pattern or shared index. The shared-index shapes answer with one index; the table shows each shape |
| T11 Serving memory | `memory.current`, anonymous and file, idle with both full indexes open and peak under T5's term load (lower) | `memory.stat` sampled every 1 s. E also at the smallest heap at which its T4 term class stays inside the A/A band (found in the screen), stated |
| T12 Engines alone | Prospect's ratchet rows, strata against Xapian 2.1.0 (and Lucene, Tantivy) | D12 |
| T13 Validity | Generator headroom (achieved ≥ 99% of offered, generator processes under 80% CPU), A/A band per row, dropped-query lists, effective settings | Recorded by every run; a failed assertion voids the unit |

The many-indexes axis carries the same weight as the single-index rows: the release claims
nothing about "many indexes" unless T8 to T10 show it.

## Cost (estimates)

On-demand rates from `lease ledger`: `c7a.8xlarge` $1.642 an hour, `c7i.metal-24xl` $4.284.

| Stage | Hours (estimate) | Dollars (estimate) |
|---|---:|---:|
| Dry run on `c7a.8xlarge`: toolchain, corpora download, both Xapiand builds, E and Q install, the 20,000-document dry run, smoke, screen | 4 | 7 |
| Metal: boot, setup, sync binaries and corpora, preflight, smoke | 1 | 4 |
| Metal: T1, T2 (five rounds of five arms at 1M and 2.7M documents) and the full builds | 2.5 | 11 |
| Metal: T4, T5, T6, T7, T11 (five rounds) | 4 | 17 |
| Metal: T8 to T10 (three rounds; E's index-per-tenant creates dominate: C1 measured about 6.4 a second on the VM) | 2 | 9 |
| Metal: T12 (prospect's quick tier and 1M indexing) | 1.5 | 6 |
| Metal: 25% for reruns | 2.5 | 11 |
| **Total** | **about 4 h `c7a.8xlarge` + 13.5 h metal** | **about $65**, plus Q4's volume (provisioned gp3 is billed per month, pro-rated, a few dollars over the run's days; estimate) |

The metal hours can split across two days if another session needs the quota.

## Open questions (owner)

**Q1. Which Xapian build is X?** (a) The `v1.0.0-alpha.10` tag plus the build fixes, i.e. the
`xapiand` branch at a recorded commit; (b) the final Xapian-only release cut from `xapiand`
(alpha.10 plus the backports), tagged before the run; (c) alpha.9. Recommendation: (b), since it is
what users will compare against; (a) if the final release is not cut by the dry run, labelled
"alpha.10 with build fixes" at its commit.

**Q2. The durability tier.** (a) Compare at "acknowledged before fsync" (D5) and report E's
default fsync-per-request beside it; (b) first wire `DB_SYNCHRONOUS_WAL` to a per-index setting and
compare both tiers; (c) compare against E's defaults only. Recommendation: (a) for 8b; durable
acknowledgement belongs with 7b's `ack` and gets its own rows then.

**Q3. Which "similar products".** (a) E and Q; (b) also OpenSearch and Meilisearch in the rows they
support. Recommendation: (a). OpenSearch shares E's engine and shard limits; Meilisearch documents
one shared index for tenants. The adapter shape makes (b) a later addition.

**Q4. Storage for the metal run.** (a) `lease` takes a disk size, IOPS and throughput, and the run
uses a 500 GB gp3 at 16,000 IOPS and 1,000 MB/s (a `lease.sh` change; the policy's volume
conditions to check); (b) add an instance-store family (`c6id.metal` or `i4i.metal`) to the lease
policy; (c) the default 100 GB gp3. Recommendation: (a), with the fsync latency published beside
the numbers; (b) only if the screen shows a row device-bound for any system. (c) cannot hold the
data.

**Q5. Resources per system.** (a) 8 cores and 32 GiB; (b) 4 cores and 16 GiB; (c) the whole host.
Recommendation: (a): a common node size, and E's heap rule gives it 16 GiB. The per-index,
one-thread rows are T12's (PLAN, "The workload is many indexes").

**Q6. What the release claims.** (a) Publish every row, losses as losses; claim "faster" or
"smaller" only for rows that are better, and "by the margin" only where met; a loss does not block
the release; (b) the release waits until every claimed axis is better than E and Q. Recommendation:
(a); Phase 8b is a measurement, and each loss becomes a perf item.

## First steps (not blocked by the questions)

1. `harness/systems.py` with the Xapiand, E and Q adapters, the neutral query spec and its
   translations, and D8's count check, tested against a local Xapiand and against E and Q started
   from their tarballs in a scratch directory.
2. `loadtest.py --system`, batched multi-client indexing and the latency phase; delete
   `es_loadtest.py` and move its three references.
3. `stall_load.py --system --procs`; `wide_search_check.py --system` with the tenant phases;
   `engine_mixed_load.py --system --kill-at`.
4. `closing_bench.sh` (cgroups, cpusets, rotation, S', phase times) and `closing_report.py`.
5. Corpus fetch scripts for C-wiki, C-logs and C-tenants with sha256 checks.
6. The dry run on a `c7a.8xlarge` (D9), its ledger section, then the metal run once Q1, Q2 and Q4
   are answered.

Q1 blocks only X's build, Q2 the durability settings of T1, T2, T6 and T7, and Q4 the metal lease.
