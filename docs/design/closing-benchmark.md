> Status: DRAFT (2026-11-03) revision 1, Phase 8b's closing benchmark: Xapiand on strata against the final Xapian-only release, Elasticsearch and Quickwit on one Linux x86-64 metal host. Folds in the preliminary review (`docs/reviews/closing-benchmark-design.md`: L1 to L4, CB-R5 to CB-R13, C2) and the owner's rulings on Q1 to Q6. State: designing (revision 1, ready for the architect's re-check). Workstream `design/closing-benchmark`, ledger `docs/status/closing-benchmark.md`.

# Closing benchmark (Phase 8b)

## Summary

One benchmark, once, before the release: Xapiand on strata (S) against Xapiand on Xapian (X, the
final Xapian-only release), Elasticsearch (E) and Quickwit (Q), through each system's HTTP API, on
every axis the release will claim: indexing, queries by class, many small indexes, mixed load,
restart and recovery, memory and disk. Every system gets the same cores, memory and disk, is
configured as its own documentation recommends with result caches off, and runs pinned to a
recorded version. The published numbers come from an AWS `c7i.metal-24xl` in one-shot `lease run`s
of at most 45 minutes each; a `c7a.8xlarge` dry run proves the harness and measures every unit
first.

The harness is the one Xapiand already has, given a system adapter: `loadtest.py` (indexing,
latency per class), `stall_load.py` (open-loop throughput and mixed load), `wide_search_check.py`
(many indexes) and `closing_bench.sh` with `closing_report.py`. `es_loadtest.py` folds into the
adapter and is deleted. The engines-alone pair (strata against Xapian 2.1.0) runs prospect's
existing harness on the same lease.

Revision 1 changes, by review finding: result caches off and count modes pinned per class (D13,
L1); the durability tier renamed, with every system's loss window and one crash audit for all (D5,
T7, L2); a run plan of whole units packed into 45-minute runs, its cost redone (D14, L3); a
"better" rule that `ab_compare.py` computes, against a fixed comparator, at five rounds (D10, D11,
L4).

## What this reads, depends on and changes

| Document | Relation |
|---|---|
| `docs/reviews/closing-benchmark-design.md` (`Xapiand:33ccf9e3e`) | **Folded in:** L1 (D13), L2 (D5, T7), L3 (D14), L4 (D10, D11); CB-R5 to CB-R13 and C2 as the ledger lists |
| `docs/status/road-to-release.md:33` (Phase 8b: strata against Xapian 2.1.0, and X against S end to end, "Review: none") | **Changes it:** adds E and Q, the metal window (Q7) and this design's reviews. The run itself stays unreviewed (published numbers); the lead runs it |
| `docs/status/road-to-release.md:32` (Phase 8a: `sortable_serialise` replaced by the 8-byte map) | **Depends on it:** S is measured after 8a lands, since the codec sets range and sort cost |
| strata `docs/PLAN.md:691` (the closing benchmark: two pairs, every axis, once) | Implements both pairs; adds E and Q to the end-to-end pair. PLAN's Phase 7 rows against E and Q at 3, 30 and 300 nodes stay 7b's |
| strata `docs/PLAN.md:182` (Margins: queries ≤ 0.7×, indexing ≥ 1.3×, memory ≤ 0.7×, disk ≤ 0.8×) | The "by the margin" level of D11 |
| strata `docs/BENCHMARKS.md:19` (noise band), `:46` (equal semantics), `:137` (query speed), `:155` (memory in cgroup v2), `:161` (disk) | Adopted at system level (D5, D8, D11), with two stated departures: T4 publishes raw times beside a client floor instead of subtracting a pipe floor (`:137`), and T3's gate figure is natural bytes, not force-merged (`:161`), since neither Xapiand nor Quickwit has a force merge over its API (T3). `:58` names `es_loadtest.py`; D7 replaces it, and strata's line is updated in the same step |
| strata `docs/design/product-landscape.md:173` (T2 many indexes) and `:175` (T4 memory against E) | T8 to T10 and T11 here are those rows, single-node, at 10^4 and 10^5 tenants |
| strata `docs/perf/c1-field-tenants.md` (C1: E index per tenant stopped at the 1,000-shard limit, `:45`; 3,700 indexes in 576 s with it raised, about 6.4 a second and falling, `:46`) | T8 to T10 reuse C1's method (a fixed total of documents split into tenants) and cap E's and Q's index-per-tenant creates at 30 minutes (D14) |
| `docs/design/phase7c.md` rows T4, T5, T12 and `docs/perf/phase7c-baseline.md` | Same tools (`stall_load.py`, `loadtest.py`); no 7c row changes |
| `docs/gates/phase7c-linux-dryrun.md` | Facts used: clang 20 and libc++ 20 build Xapiand, clang 18 does not (`:4`, `:17`); a Release LTO build takes 2.5 min on a `c7a.4xlarge` (`:6`); one Python generator is bound above about 3,200 requests/s (`:35`) |
| campaign `references/measurement.md`, `references/lease.md` (dotfiles `0c3c203`) | Smoke, screen and confirm; run-level intervals with an A/A; calibrate every baseline, then time against the fastest (D10). One command per `lease run`, `--timeout` bounding boot to park, at most `MAX_RUN_MIN` (D14) |

## Decisions

**D1. Four systems under test, versions pinned and recorded.**

| | System | Build |
|---|---|---|
| S | Xapiand on strata | master at Phase 8a's landing, Release with LTO, clang 20 and libc++ 20 |
| X | Xapiand on Xapian | the final Xapian-only release cut from the `xapiand` branch (owner, Q1). `xapiand` is at `Xapiand:e692a64b1` today, 22 commits past `v1.0.0-alpha.10` (`Xapiand:3fe13a2fb`, which does not build on Linux). The release tag, or the branch commit used if the tag is not cut by the dry run, is recorded and labelled |
| E | Elasticsearch | the newest 9.x GA tarball with its bundled JDK on the dry-run day, sha512 checked (C1 ran 9.5.5) |
| Q | Quickwit | the newest release on the dry-run day (v0.9.3, released 2026-10-22, is the latest today), sha256 checked. Every Quickwit fact below is cited at v0.9.3 and rechecked at the pinned version in step 1 |

Both Xapiand binaries are built once on the dry-run lease, returned through `$OUT`, and synced to
the metal lease byte for byte, as http's T6 run did (`http:docs/perf/t6-t7-2026-11-03.md`); every
result records each binary's sha256, `file` output and `ldd` list.

**D2. Equal resources, one system timed at a time, one host per round.** Each system runs in its
own cgroup v2 slice (`systemd-run --scope`) with `cpuset.cpus` = 8 physical cores (one hardware
thread each, the SMT siblings left idle) and `memory.max` = 32 GiB (Q5). The load generator runs on
16 other physical cores, siblings idle. Every system is told its core count where it has a setting
(Xapiand `--processors 8`, `src/opts.cc:189`; E `node.processors: 8`; Q `indexer.cpu_capacity`),
and the preflight asserts the effective value each reports.

- **Shards.** E and Q one shard per index: E's sizing guidance puts a shard at 10 to 50 GB
  (landscape `es9`), above both corpora's indexes. S and X run the screen at 1 and 5 shards
  (Xapiand's default is 5, `src/opts.cc:43`, `src/opts.h:75`; `--shards` at `src/opts.cc:153`),
  since a Xapian database has one writer and one shard would make X's T1 and T2 one-writer
  numbers. Each runs the confirm at its faster T1 setting, stated, and T4 to T7 query the index
  that setting built (CB-R5). Tenant indexes (T8 to T10) have one shard everywhere.
- **Untimed builds run four at once.** The full indexes and tenant sets (D14's builds) are not
  timed, so S, X, E and Q build them together in four disjoint 8-core, 32 GiB slices, the loaders on
  the other 16 cores: 48 physical cores and 128 GiB of the host's 192 GiB
  (aws.amazon.com/ec2/instance-types/c7i). Each index's shape (shards, merge policy, cores) is the
  one the timed rows use.
- **One host per round.** A stop and start can land on another physical host, so every round of a
  row, all its arms, runs inside one `lease run` (D14). Each run records the host's identity
  (`/sys/class/dmi/id/product_uuid`, board serial, CPU microcode, `lscpu`) and 10 s of `fio` fsync
  latency; whether the DMI fields identify the physical host on Nitro metal is checked in M0
  (unverified).
- Plain HTTP over loopback, keep-alive, E's security off, no TLS anywhere.

**D3. Corpora are public, fetched on each lease by script and pinned by sha256 at the dry run.**
Nothing large crosses the home link: each lease downloads from the sources below, checks the
sha256 the dry run recorded, and keeps the files on its parked disk.

| Corpus | Source | Size | Used by |
|---|---|---|---|
| C-wiki | search-benchmark-game's English Wikipedia (`make corpus`: `wiki-articles.json.bz2` through `corpus_transform.py`) | 5,032,104 documents (prospect `docs/perf/format13-gate-vm.md:14`), 8.33 GB (strata `docs/research/indexing.md:118`) | T3 (full), T4a, T5a, T7, T11; its first 500,000 documents are T1's sample (wiki-500k) |
| C-wiki queries | search-benchmark-game `queries.txt` (AOL queries, tagged term, intersection, union, phrase), sampled with prospect `bench/sample_queries.py` (40 per class, seed 7) | 160 queries | T4a, T5a |
| C-logs | Elastic Rally's `http_logs` track: `documents-181998.json.bz2` and `documents-191998.json.bz2` | 2,708,746 + 9,697,882 = 12,406,628 documents, 0.36 + 1.30 = 1.67 GB uncompressed (the track's `track.json`; publisher figures, checked at the dry run) | T3 (full), T4b, T5b, T6, T7; the first 1,000,000 documents of `documents-181998` are T2's sample (logs-1M) |
| C-logs writes | the same track's `documents-201998.json.bz2` | 13,053,463 documents, 1.74 GB (publisher figures) | T6's and T7's write stream |
| C-logs queries | templates after Rally's `http_logs` operations, one JSON line each (D7), range bounds drawn per run (D13) | 40 per class, 10 per aggregation class | T4b, T5b, T6 |
| C-tenants | C-wiki's first 1,000,000 documents in order, cut into 10,000 tenants of 100, or 100,000 tenants of 10 | 1,000,000 documents either way, as C1 held the total fixed | T8 to T10 |

`accounts.ndjson` is not used: `loadtest.py` replicates its 1,000 lines (`harness/loadtest.py:50-55`),
which flatters compression and term statistics. Fields used from C-logs: `@timestamp` (date),
`status` (integer, also keyword-like terms), `size` (integer), `request` (text), `clientip`.

**D4. Each system configured by its own documentation, every setting checked in.** Configuration
lives in `harness/closing/` (one file per system), and every result embeds the effective settings
read back from the running system. Caches and count modes are D13's.

- **E:** heap by ES's own auto-sizing for the 32 GiB cgroup (half, under the compressed-oops
  ceiling, landscape `es14`), `-Xms` equal to `-Xmx`, memory lock on; one shard, no replicas.
  During loads the bulk-load guidance (elastic.co, "Tune for indexing speed"):
  `refresh_interval: -1` then a refresh, replicas 0, bulk size found by doubling from 100 documents
  until throughput levels (screen), senders added until CPU or disk saturates. Every other row uses
  the default 1 s refresh. Mappings: `text` with the standard analyzer for text, `keyword` for
  terms, `long` and `date` for numbers and times. `cluster.max_shards_per_node` raised to 100,000
  for the index-per-tenant shape, as its documentation allows, and recorded.
- **Q:** index config with `record: position` on text (phrases need it), fast fields on every
  field used by range, aggregation and sort, the default tokenizer; `commit_timeout_secs` at its
  default (60, `docs/configuration/index-config.md:595` at v0.9.3), and at 1 s as a report arm of
  T6. Relevance queries pass `sort_by: _score`, since Q does not score by default (landscape
  `qw3`). Ingest through its ingest API, `commit=auto`, with `commit=force` on the last batch of a
  load.
- **S and X:** each binary's documented options with the core count and D2's shards; text fields
  without stemming or stop words; `--database-pool-size` (`src/opts.cc:159`, default 200 at
  `src/opts.cc:50`) sized to the tenants open in T8 to T10 and recorded.
- **Analysis parity:** lowercase, Unicode word splitting, no stemming, no stop words, positions on,
  BM25 with each system's default parameters. Where a tokenizer still differs, D8's count check
  catches the queries it changes.
- **Warm-up by time:** every arm, after its start and before its timed part, runs its unit's class
  mix for a fixed time (60 s, drawn from a seed the timed part does not use). The screen checks the
  length: every system's p50 over successive 10 s windows of a 180 s warm-up, and the warm-up
  becomes the first window after which every system stays inside its A/A range, at least 60 s,
  recorded (CB-R7).

**D5. Durability: compared at "acknowledged before the write is durable", each system's loss
window stated.** Q2's ruling stands: the comparison tier is the not-durable one, with E's default
`request` translog reported beside it. No system in the comparison makes an acknowledged write
durable; each loses a bounded recent window on a crash. SIGKILL (T7) tests a process crash, which
data already handed to the kernel survives, so the process-crash window is what the process still
held. Power loss is not tested; its window is stated from the same sources.

| System | What an acknowledgement means at the pinned version | Process-crash window (T7 audits it) | Power-loss window (stated, not tested) |
|---|---|---|---|
| S | the WAL line is queued to a writer thread and the request acknowledged before the line reaches the file (`src/database/wal.cc:1111-1115`, `enqueue` at `:848-856`); `DB_SYNCHRONOUS_WAL` (`src/database/flags.h:42`, read at `src/database/shard.cc:747`) is never set in `src/`. The fsync is debounced (hook at `wal.cc:108`): 500 ms, forced within 2.5 to 3.5 s under steady writes (`src/opts.cc:71-75`) | the writer queue at the kill; the repository's crash check bounds it at 2 s (`harness/engine_crash_check.sh:9-11`, `GRACE` default at `:130`) | the queue plus up to 3.5 s of debounce |
| X | the same code on the `xapiand` branch (`src/database/wal.cc:1158`, `:1178`, `enqueue` at `:941`, hook at `:116`; `src/opts.cc:121-125`); only replication sets `DB_SYNCHRONOUS_WAL` there | as S: 2 s | as S |
| E (`index.translog.durability: async`) | the translog is fsynced and committed every `index.translog.sync_interval` (default 5 s); "all acknowledged writes since the last automatic commit will be discarded" (elastic.co/docs/reference/elasticsearch/index-settings/translog) | 5 s (`sync_interval`, read back) | 5 s |
| E (`request`, the default; report arm) | fsync and commit after every request (same page) | 0 | 0 |
| Q (ingest v2, the default: `docs/ingest-data/ingest-api.md:81`, `quickwit-config/src/lib.rs:89-92` at v0.9.3) | the ingester's WAL is opened with `PersistPolicy::OnDelay { interval: 5 s, action: Flush }`: appends reach the OS at an append at least 5 s after the last flush and are never fsynced (`quickwit-ingest/src/ingest_v2/state.rs:228-235`; mrecordlog `src/persist_policy.rs:43-46`, `:63-76` at `3b3562ef`, the rev `quickwit/Cargo.toml:184` pins). `commit=auto` sets when documents become searchable, not durability (`docs/reference/rest-api.md:131`, `:156`) | 5 s under steady appends (longer when appends pause; T7's load does not pause) | until the kernel writes the file back |

`closing_report.py` publishes this table with T7's results; each window is read back from the
running system where it is a setting (`sync_interval`) and taken from the source above where it is
not.

**D6. The machine: `c7i.metal-24xl` for the numbers, `c7a.8xlarge` for the dry run.** The metal
host is an Intel Xeon Platinum 8488C, 48 cores, 192 GiB, one NUMA node (http's T6 lease meta);
turbo off (`intel_pstate/no_turbo=1`), governor `performance`, both recorded. One launch of it
measured 39 s from `lease up` to ready (`~/.scratch/leases/phases.tsv`, `http-t6-metal`,
2026-11-03T21:31:38Z); M0 measures a resume. It takes the whole 96-vCPU quota while a run is up,
so the window is agreed with the other sessions (Q7; lease.md, "Metal is the owner's call").
Storage is EBS: the policy allows only `c7a`, `c7i`, `c7g`, `c8g`, `t3` and `t4g`
(`bench-lease-policy.json:31`), none with instance NVMe, and `lease` launches a fixed 100 GB gp3
root (`lease.sh:144`). Q4's ruling (a) stands: a 500 GB gp3 at 16,000 IOPS and 1,000 MB/s, which
needs `lease` to take a disk size, IOPS and throughput (not in `lease.sh` today; a prerequisite,
D14). The dry run fits the default 100 GB (estimate: its units hold the 1M and 2.7M screen indexes,
not the full ones). M0 records the volume's fsync latency (`fio`, 4 KiB write plus fsync, 30 s),
and every run 10 s of it, beside every result.

**D7. One harness shape: Xapiand's harness plus one adapter module.** `harness/systems.py` holds
one adapter per system (S and X share Xapiand's): start and stop in the cgroup, create an index
from a neutral schema, bulk load, durable commit, search from a neutral query spec in a count mode
(D13), read counts, every document id (T7's audit), disk bytes, cache counters and the effective
settings. The neutral query spec (`harness/closing/queries/`) is one JSON line per query or
template (`class`, `field`, `terms` or a bound template, `command`), translated per system:
Xapiand's DSL, E's DSL, Q's search API. Then:

- `loadtest.py` gains `--system`, `--corpus`, `--queries`, multi-client batched indexing
  (`--clients`, `--batch-docs`), a concurrency-1 latency phase per class with CPU-seconds per query
  from `cpu.stat`, the client floor (D13), and resumable loads (`--from`, `--until-minutes`: load a
  range, commit, record the offset) for the full builds that outlast one run. Its index phase today
  is one RESTORE request (`harness/loadtest.py:93`) and its query phase a closed loop (`:114`).
  `es_loadtest.py` is deleted; its callers (`docs/dev/testing.md:32`, `:260`, strata
  `docs/BENCHMARKS.md:58`) move to `loadtest.py --system es`.
- `stall_load.py` gains `--system` and `--procs N`: its open-loop schedule is split across N
  generator processes, since one is bound above about 3,200 requests/s on Linux
  (`docs/gates/phase7c-linux-dryrun.md:35`). Its `--saturate` (`harness/stall_load.py:875`) today
  stops at 10 times the unloaded p99 (`:891`) and a 5% rate error (`:899`); T5 needs a fixed p99
  bound per class and a 1% rate error, as options.
- `wide_search_check.py` gains `--system` and the tenant phases of T8 to T10; its `gate` mode
  (10,000 indexes, p50 and p99, peak pinned indexes) is T10 for S and X.
- T7's crash leg reuses the audit of `harness/engine_crash_check.sh` (`:9-12`, the checks at
  `:130-140`) for every system, through the adapters' id dump. `engine_mixed_load.py`'s model check
  is a SIGTERM audit (`harness/engine_mixed_load.py:13-15`) and stays out of T7.
- `harness/closing_bench.sh` runs units and plans runs (D14); `harness/closing_report.py`
  tabulates and applies D11 through cpp-libraries' `tools/ab_compare.py compare`.

No second framework: Rally, `esrally` tracks and Quickwit's own benchmark are sources of corpora
and query shapes only.

**D8. Correctness beside speed.** Before timing, every query runs once on every system, in the
exact count mode (D13):

1. Counts must agree. A query whose count differs by more than 1% between any two systems is
   dropped from every system's timed set and listed with the reason and the system or systems that
   disagreed. More than 5% dropped in a class stops the dry run until the analysis config is fixed
   as far as each system's documented options allow. If it still exceeds 5% (a phrase class may,
   since Quickwit's default tokenizer is not UAX #29 and Xapian's term generator differs from
   both), the class is published with its drop rate and dropped list; pre-tokenized input is not
   used, since analysis is part of what indexing costs (CB-R9).
2. S against X: counts equal, and top-10 ids compared in order, as `harness/search_order_diff.py`
   does for the docs suite's newman reports; every difference is listed as a known behavior change
   or a bug.
3. Aggregation results compared bucket by bucket (Q's terms counts are approximate, landscape
   `qw4`: its differences are listed, not dropped).

**D9. Stage every run: smoke, screen, confirm; the metal is booked on the dry run's evidence.**
On the `c7a.8xlarge`: a 20,000-document dry run of every row and system (each must give a
non-empty, count-checked result), D13's cache and count probes, the smoke (every arm 20 to 30 s,
effective settings asserted), and the screen: one round of every row at reduced size, which also
picks S's and X's shard count (D2), E's bulk size, the warm-up length (D4), E's smallest heap for
T11 and T6's write rate (T6), and checks build-to-build variance once (D10). On the metal: setup,
smoke again, builds, then calibration and confirm (D14). Only metal calibration and confirm numbers
are published.

Before the metal is booked, the dry run's ledger section shows, per system, the effective settings
read back, the cache and count probes, the count check's drop list, every unit's measured duration,
and the run plan `closing_bench.sh --plan` packs from them with its cost (C2). A plan more than 20%
over Q7's approved estimate goes back to the owner.

**D10. Rounds, arms, A/A and what restarts.** Every row the release may claim on runs five rounds
on the metal (D11 needs all five); T11's heap search and the Q2 report arms run two (D14).

- **Calibrate, then race the anchor** (measurement.md, "Calibrate every baseline once"). Rounds 1
  and 2 run every arm: S X E Q S' (T8 to T10: every shape). After them, `closing_report.py` fixes
  each row's comparator, the anchor: the system or shape with the best median over the two rounds
  among X, E and Q, per metric (per class in T4 and T5). Rounds 3 to 5 run S, S', X, the anchor,
  and every system whose calibration median is within 10% of the anchor's; a unit holding several
  metrics runs the union of their anchors. X runs every round (D11's regression check).
- **Rotation.** Each round runs its arms in a rotated order (S X E Q S', then X E Q S' S, ...),
  every arm from a stopped process with the page cache dropped, then D4's warm-up.
- **S'** is S's binary again: the A/A control. What it starts from, and what every arm starts from,
  depends on the row (CB-R11):

| Rows | Each arm, each round |
|---|---|
| T1, T2, T3 | builds its own fresh data directory from the sample (S' too), deleted after T3's reading |
| T4, T5, T7 clean, T11 | restarts on the full indexes built once in B1 to B3; S' restarts on S's index. Build-to-build variance is therefore outside the A/A band: the screen builds S's 1M wiki index twice and runs T4a's term class on both, and a difference outside the A/A range makes S' build its own full indexes (two more build runs) |
| T6, T7 crash | restores its own pristine copy of the full C-logs index (made in B3) before it starts |
| T8 to T10 | restarts on the tenant sets built once in B4 and B5 |

- Each row keeps every round's per-run statistic. Disk and memory bytes are single exact readings
  per round. Every run records its phase times (boot, sync, setup, each arm's start, load, warm-up,
  timed part and teardown) and its total.

**D11. What "better" means, as `closing_report.py` computes it.** Per row metric, per class where
the row has classes, `closing_report.py` runs `ab_compare.py compare --a S --b ANCHOR --aa S'
--output csv` over the five rounds' statistics (`--higher-better` for throughputs). Its columns are
the median paired ratio, its range (`low`, `high`), the A/A deviation (the largest `|S'/S - 1|`),
and a verdict, `slower` when the anchor is slower than S beyond the A/A band
(`tools/ab_compare.py:167-178`, `:406-435` at cpp-libraries `95ff17e`). The ratio is S's metric
over the anchor's for times and bytes and the anchor's over S's for throughputs, so below 1
favors S.

- **Better:** the verdict is `slower`, `high` is below 1 (all five rounds favor S), and the median
  ratio is at most 0.97. Five of five is a sign test at one-sided p = 0.031; three rounds would
  give 0.125, and a percentile bootstrap of five rounds excludes 1.0 only at five of five anyway
  (`.scratch/closing-benchmark-review/stats_probe.out`).
- **By the margin:** better, and `high` meets PLAN's Phase 6 ratio for the axis: at most 0.7 for
  query time, 1/1.3 (0.77) for indexing throughput, 0.7 for memory, 0.8 for disk.
- **Worse:** the verdict is `faster`, `low` is above 1, and the median ratio is at least 1.03.
- **Not distinguishable:** anything else, published as measured.
- **Against every finalist.** A row is "better" only when it is better against every system that
  ran all five rounds (the anchor and any within 10% of it). A system 10% or more behind the
  anchor in calibration is reported from its two rounds and named as such.
- A system that cannot serve a row is marked "not supported" and left out of the comparator (Q has
  no update or delete by id and no document fetch by id; no row here needs them).
- **Against X separately:** the same computation with `--b X`. No row may be worse; a worse row is
  a regression and is published as one (Q6).

**D12. The engines-alone pair runs prospect's harness on the same lease.** After the system rows,
strata against Xapian 2.1.0 (PLAN's first pair) runs prospect's quick tier
(`bench/query_trials.sh` with `queries-sample` and `terms-sample`, scored by `bench/phase2/gate.py`)
and its indexing trials on 1M documents (`bench/index_trials.sh`), with Lucene and Tantivy beside
as the anchors they already are, each in one run (D14). Nothing here changes that harness.

**D13. Result caches off, filter caches stated, count modes pinned per class (L1).** T4 takes each
query's fastest of 10 runs and T5 and T6 replay a fixed set, so any cache that stores answers would
time stored answers.

| Cache | Default at the pinned version | Setting here | Probe counter |
|---|---|---|---|
| E shard request cache: whole `size: 0` responses, counts and aggregations (elastic.co/docs/reference/elasticsearch/rest-apis/shard-request-cache) | on for `size: 0` | **off:** `index.requests.cache.enable: false` at creation and `request_cache=false` on every search | `_nodes/stats/indices/request_cache` `hit_count` |
| Q partial request cache: leaf results for an identical request on a split | 64 MB (`quickwit-config/src/node_config/mod.rs:396-399`, `:638` at v0.9.3) | **off:** `searcher.partial_request_cache_capacity: 0` | `/metrics` `cache_hits_total{component_name="partial_request"}` (`quickwit-storage/src/metrics.rs:249-253`, `:328-329`) |
| S and X | no result cache: `src/` holds schema and endpoint LRUs (`src/database/schemas_lru.cc`, `src/index_resolver_lru.cc`) and no query or result cache; strata's only decode cache lives for one call (`include/strata/core/reader.hh:539-548`, strata `356acbc`) | nothing to turn off | none; T4's CPU-seconds per query would show a stored answer |
| E node query cache: filter bitsets | on, 10% of heap, per segment of at least 10,000 documents and 3% of the shard (elastic.co/docs/reference/elasticsearch/configuration-reference/node-query-cache-settings); Lucene's policy caches point and range filters after 2 uses in the last 256 filters, other filters after 5, term queries never (`UsageTrackingQueryCachingPolicy.java:45-60`, `:122-135`, apache/lucene `f72004515e`, rechecked at the bundled Lucene in step 1) | **on, as normal operation, stated;** range bounds vary per run (below) | `_nodes/stats/indices/query_cache` `hit_count`, `cache_count` |
| Q predicate cache: the hit set of a query on a split | 256 MB (`node_config/mod.rs:400-403`, `:639`; `quickwit-search/src/leaf_cache.rs:197-230`) | **on, stated;** range bounds vary per run | `cache_hits_total{component_name="predicate"}` (`metrics.rs:332-333`) |
| Q fast field and split footer caches: index data, as the page cache is | 1 GB and 500 MB (`node_config/mod.rs:387-395`, `:636-637`) | on | `cache_hits_total` for `fastfields`, `splitfooter` |
| Page cache | | dropped before every arm's start, then D4's warm-up | `memory.stat` `file` |

- **Range bounds vary per run.** The range classes (range on `@timestamp`, range on `size`, a term
  with a range) are templates: a field and a width. Each of a template's 10 T4 runs draws its start
  from a seeded sequence (the same sequence for every system, the seed recorded), so a bound repeats
  within Lucene's 256-filter history only by chance; T5 and T6 draw per request. A template's T4
  statistic is the median of its 10 draws: the draws are different queries, and their fastest
  would pick the cheapest range. The other classes keep the fastest of 10 identical runs.
- **Count modes per class.** Exact counting and early termination are different work, so every
  class names its mode and every system runs it:

| Mode, classes | E | Q | S and X |
|---|---|---|---|
| top-10: term, boolean AND, boolean OR, phrase, the range classes, sort | `size: 10`, `track_total_hits: false` | `max_hits=10`, `count_all=false`, an underestimate (`quickwit-serve/src/search_api/rest_handler.rs:206-240` at v0.9.3) | `limit 10`, the default estimate (`check_at_least` 0, `src/engine/search.h:82`) |
| count: the four C-wiki classes again, and D8's check | `size: 0`, `track_total_hits: true` | `max_hits=0`, `count_all=true` (its default) | `limit 0`, `check_at_least` at the index's document count (`src/server/search_views.cc:2384`) |
| aggregation: `date_histogram` by hour, terms on `status`, stats on `size` | `size: 0`, `track_total_hits: false` | `max_hits=0`, `count_all=false` | aggregations see every match whatever `check_at_least` says (`src/database/handler.cc:1223-1227`) |

- **The client floor.** Per system, the cheapest request it answers (E `GET /`, Q
  `GET /api/v1/version`, S and X a `GET` of a missing document id), timed like T4's queries and
  published beside T4's raw times; nothing is subtracted (CB-R8).
- **Step 1's probe**, run again in the dry run, its output in the dry run's ledger section, per
  system:
  1. Each class's first query ten times identically under D13's settings: the request and partial
     request hit counters stay at 0. Then once with the system's default cache settings: the
     counters rise, so a 0 is not a dead counter.
  2. A range template ten times with one bound, then ten draws: E's query cache and Q's predicate
     cache show hits for the first and none, or the stated few, for the draws.
  3. A term matching more than 10,000 documents: in the top-10 mode E returns no exact total and Q
     an underestimate; in the count mode all four return the same exact count.
  4. The effective settings read back and embedded in the result.

  Every timed unit then reads the hit counters before and after and publishes the filter caches'
  hit ratio beside its row.

**D14. The run plan: whole units in one-shot runs (L3).** `lease run` takes one command; its
`--timeout` bounds the whole run from launch to park, at most `MAX_RUN_MIN` = 45 min
(`lease.sh:444`, the run's clock from `:459`; CMD gets what remains and the run refuses under 60 s,
`:492-493`), and AWS stops the instance at launch plus timeout plus 5 minutes (`:460`,
`DEADLINE_MARGIN_MIN` at `:401`). So:

- **A unit** is one round of one row over its arms (D10), or one build or setup step. A unit never
  splits across runs, since a round's arms share a host (D2). A unit that outgrows a run shrinks its
  sample or phases, stated; long builds are chunked, each chunk a unit (D7's resumable loads).
- **A run** is `lease run closing-metal c7i.metal-24xl --timeout T --sync BUNDLE --setup setup.sh --
  harness/closing_bench.sh --units LIST`, holding whole units of at most 40 content minutes (45
  minus boot, sync, setup, the results stream and a margin); `T` is the measured units plus 5
  minutes, never more than 45. `$OUT` gets each unit's raw per-round files, effective settings,
  cache counters, phase times, host identity, the `fio` line and the run's log; nothing is read from
  the machine afterwards.
- **Packing.** `closing_bench.sh --plan` packs units first-fit decreasing within each phase, from
  the dry run's measured unit times scaled by the metal smoke's, and prints the run list; the lead
  runs it in order. Phases: A setup, B builds, C calibration (rounds 1 and 2), D confirm (rounds 3
  to 5, after the anchors are fixed), E report units.
- **Caps.** Elasticsearch's and Quickwit's index-per-tenant creates stop at 30 minutes each, in B4
  and B5 beside S's and X's tenant builds, and the count each reached is recorded (C1's rate, about
  6.4 a second and falling, bounds E near 11,500). T8 compares at the count every shape reached.
- **No gap of three days.** A lease parked past `LEASE_PARK_DAYS` (3, `lease.sh:42`, pruned at
  `:313`) is deleted with its disk, corpora and indexes; the window runs on consecutive days.

| Phase | Units (minutes are estimates until the dry run measures them) | Runs (estimate) |
|---|---|---:|
| Dry run, lease `closing-dry`, `c7a.8xlarge` | D0 toolchain, build S; D1 build X; D2 C-logs, E and Q; D3 C-wiki, C-tenants; D4 the 20,000-document dry run with D13's probes; D5 smoke; D6 to D10 the screen (T1 and T2 at 1 and 5 shards, E's bulk size; the 1M and 2.7M screen builds; T4, T5, warm-up length, T11 heap; T6, T7, T6's write rate; tenants 10^4 and T8 to T10) | 11 |
| A setup, lease `closing-metal` | M0a runtime libraries, E and Q, C-logs, preflight, resume boot; M0b C-wiki fetch and transform, C-tenants; M1 smoke | 3 |
| B builds | B1 to B3 full C-wiki and C-logs, four systems at once, chunked, and T6's pristine copies; B4 tenants 10^4 and B5 tenants 10^5, every shape, E and Q index per tenant capped at 30 min | 5 |
| C calibration | rounds 1 and 2 of T1, T2, T4a, T4b, T5a, T5b, T6, T7 crash, and T8, T9, T10 at both scales, every arm (21 to 35 min a unit) | 24 |
| D confirm | rounds 3 to 5 of the same, S S' X and the anchor (8 to 28 min a unit, up to three to a run) | 23 |
| E report units | the Q2 report arms (E `request` beside E `async` in T1, T2 and T7; T6 with E `request` and Q at a 1 s commit), T11's E heap, T12's two prospect runs | 6 |

The estimate comes from `.scratch/closing-benchmark-r1/run_plan.py` (output `run_plan.out`): 61
metal runs, 73 with 20% reruns.

**Cost (estimates).** On-demand rates from `lease.sh:55`, `:58`: `c7a.8xlarge` $1.642 an hour,
`c7i.metal-24xl` $4.284.

| Item | Hours | Dollars |
|---|---:|---:|
| Dry run: 11 runs, 20% reruns | 8.3 | 14 |
| Metal: 73 runs; boot measured once at 39 s, 3 min a run for boot, sync, results and park (estimate) | 43 | 184 |
| Parked 500 GB gp3 at 16,000 IOPS and 1,000 MB/s, about $4.6 a day from AWS's published us-east-2 gp3 prices | 5 to 10.5 days | 24 to 48 |
| **Total** | | **about $220 to $250** |

Revision 0's $65 counted 13.5 metal hours, with no boots, builds, calibration rounds or reruns in
the shape the review required. `DAILY_USD` is 25 for every lease on the account
(`~/.scratch/leases/config`) and is checked only before a run starts (`lease.sh:447`), so the plan
counts each metal run at its worst case, timeout plus the 5-minute stop margin ($3.57): 7 a day,
about 10.5 days; at 50, 14 a day, about 5 days (Q7).

## Gate rows

Rows are measured once, by the lead, at the commits D1 names, in D14's runs. "Better" is D11's in
every row; E's `request` arm and Q's 1 s commit appear only where a row names them.

| Row | Metric and direction | Procedure |
|---|---|---|
| T1 Indexing, text | Documents per second to searchable (higher), and CPU-seconds per 1,000 documents from `cpu.stat` (lower), at equal cores | `loadtest.py --system SYS --corpus wiki-500k` (C-wiki's first 500,000 lines): clients and batch per D4, timed from the first request to the durable commit after the last; peak `memory.current` recorded. Five rounds, S and X at their shard setting (D2). E at `request` beside E in two rounds (report). The full 5,032,104-document builds (B1, B2) are reported, not ranked |
| T2 Indexing, structured | Same as T1 | The same on logs-1M (the first 1,000,000 documents of `documents-181998`); the full C-logs builds (B2, B3) reported |
| T3 Index size | Bytes on disk per document (lower); WAL, translog and metastore bytes as their own column | After each T1 and T2 round's load: each system's durable commit (E `_flush`, Q's last split published by `commit=force`, S and X commit), bytes stable 1 s (`harness/loadtest.py:70`), then `du` of the data directory: five readings per system and corpus. Natural bytes are the gate figure, a stated departure from strata `BENCHMARKS.md:161`: Xapiand has no merge or compact command (`src/server/`) and Quickwit's REST reference lists no force merge (`docs/reference/rest-api.md` at v0.9.3). The full builds' bytes, and E force-merged to one segment, reported |
| T4a, T4b Query latency by class | Per class and mode (D13), the geometric mean over its queries of each query's fastest of 10 identical runs, or of each range template's median over its 10 draws (lower); p50 and p99 over all runs, and CPU-seconds per query from `cpu.stat`, reported; the client floor beside | `loadtest.py --latency` at concurrency 1 on the full indexes, after D4's warm-up. T4a (C-wiki): term, boolean AND, boolean OR, phrase, each in the top-10 and count modes. T4b (C-logs, after Rally's `http_logs` operations): range on `@timestamp` and on `size`, a term with a range (Rally's `200s-in-range`), aggregation (`date_histogram` by hour, terms on `status`, stats on `size`), sort (`@timestamp` descending and `size` ascending, with and without a term filter). Read beside T5: E and Q may spread one query over several cores (CB-R12) |
| T5a, T5b Query throughput | Highest open-loop rate per class whose p99 stays at or under the class bound and whose achieved rate is within 1% of offered (higher); server CPU per query (lower) | `stall_load.py --system SYS --saturate --procs N`, one class at a time, D13's draws per request. T5a: term and phrase (C-wiki); T5b: a term with a range, and `date_histogram` (C-logs). Bounds: 25 ms, and 250 ms for the aggregation. The other classes are T4's only |
| T6 Mixed read and write | Search p50 and p99 (lower) and write-acknowledgement p99 (lower) at a fixed offered load; visibility lag p99 (lower) | Open-loop writes of C-logs writes in batches of 100 at 5,000 documents a second, or half the slowest system's T2 rate if that is lower (the screen checks; the rate used is stated), beside open-loop searches of T4b's term-with-range class at half the slowest system's T5b rate for it. Visibility: one probe document a second, and one poller query every 10 ms asking for every outstanding probe id, so the lag's resolution is 10 ms. 180 s timed after 30 s of warm-up, each arm on its pristine copy (D10). Report arms, two rounds: E at `request`, Q at a 1 s commit |
| T7 Restart and recovery | Clean: time from process start to the first answered search, and that search's latency (lower). Crash: the same after SIGKILL, and the audit | Clean: every T4a and T4b arm's start is this measurement (stopped process, page cache dropped, start, a T4a term query polled every 10 ms on both full indexes). Crash: each arm on its pristine C-logs copy under T6's load, its writer logging each acknowledgement's time; SIGKILL at 60 s; restart; poll; dump every id through the adapter; then `engine_crash_check.sh`'s audit (`:130-140`) with D5's process-crash window (2 s for S and X, 5 s for E and Q): no write acknowledged longer than the window before the kill missing, nothing present that was never written, none duplicated, every document intact; the missing writes inside the window counted, and the oldest one's age published against the window. SIGKILL is a process crash, not power loss (D5). E at `request` beside E, two rounds (report): 0 lost |
| T8 Memory per open index | Anonymous and total `memory.current` per open tenant over the idle baseline (lower); descriptors per tenant (lower) | C-tenants at 10^4 and 10^5. Shapes: S and X index per tenant; E index per tenant (capped, D14) and one shared index filtered by a tenant field; Q one shared index with a tenant partition key, and index per tenant (capped). Restart on B4's or B5's set, read the idle baseline, open tenants in order (one query each), and read `memory.stat` and `/proc/PID/fd` at a quarter, half and all of N_min, the count every shape reached (after 60 s idle at N_min: the gate figure), and at each shape's own full count (its curve) (CB-R10) |
| T9 Cold open | First-query latency for a tenant not touched since start, p50 and p99 (lower) | Restart, drop the page cache, query 1,000 random tenants among the first N_min once each |
| T10 Wide search | One search across every tenant: p50 and p99 over 30 searches (lower); peak memory during them (lower) | `wide_search_check.py gate --system SYS --indexes N` (S and X; N = 10,000 or 100,000); E and Q over their index pattern or shared index. The shared-index shapes answer with one index; the table shows each shape and its tenant count |
| T11 Serving memory | `memory.current`, anonymous and file, idle with both full indexes open (read at every T4a arm's start) and peak under T5a's term load (lower) | `memory.stat` sampled every 1 s. E's smallest heap at which T4a's term class is not slower than E at its full heap: found in the screen with `ab_compare.py compare --a E --b E-small --aa E'`, confirmed once on the metal (E, E-small, E', twice) and reported, not ranked |
| T12 Engines alone | Prospect's ratchet rows, strata against Xapian 2.1.0 (and Lucene, Tantivy) | D12 |
| T13 Validity | Generator headroom (achieved ≥ 99% of offered, generator processes under 80% CPU), A/A deviation per row, cache hit counters (D13), count modes read back, dropped-query lists, effective settings, host identity and `fio` per run | Recorded by every run; a failed assertion voids the unit, which is rerun whole |

The many-indexes axis carries the same weight as the single-index rows: the release claims
nothing about "many indexes" unless T8 to T10 show it.

## Harness changes in revision 1

For the developer of steps 1 to 5 (`fix/closing-bench-harness`), by decision:

1. **Caches (D13):** E `index.requests.cache.enable: false` at creation and `request_cache=false` on
   every search; Q `partial_request_cache_capacity: 0`; E's query cache and Q's predicate cache
   left on. Adapters read the hit counters before and after each unit.
2. **Count modes (D13):** top-10, count and aggregation modes per class, with the per-system
   parameters in D13's table; D8 runs the count mode.
3. **Range templates (D13):** a field and a width, starts drawn from a recorded seed per run; T4's
   statistic for a template is the median of its 10 draws.
4. **Step 1's probe** (D13, four parts), its output kept for the dry run's ledger.
5. **Warm-up by time (D4):** `--warm-secs`, 60 by default, on a seed apart from the timed part.
6. **`loadtest.py`:** resumable loads (`--from`, `--until-minutes`), the client floor, CPU-seconds
   per query, the durable commit before T3's `du`, WAL and translog bytes split out.
7. **`stall_load.py --saturate`:** a fixed p99 bound per class and a 1% rate tolerance as options
   (today 10 times the unloaded p99 and 5%, `harness/stall_load.py:891`, `:899`).
8. **T6:** the write rate as min(5,000, half the slowest T2 rate); one visibility poller every 10 ms
   over all outstanding probe ids.
9. **T7 crash:** adapters dump every id; the writer logs acknowledgement times;
   `engine_crash_check.sh`'s audit (`:130-140`) per system with D5's window.
10. **Shards (D2):** S and X at 1 and 5 shards as an arm setting in the screen (the fixed
    `--shards 1` in `xapiand.opts` goes); tenants created with one shard.
11. **T8:** tenants opened in order with readings at N_min/4, N_min/2, N_min and the full count; E
    and Q index-per-tenant creates capped at 30 minutes, the count recorded.
12. **`closing_bench.sh`:** units, `--plan` packing (D14), anchors fixed after rounds 1 and 2 with
    the 10% rule (D10), host identity and 10 s of `fio` per run, untimed builds four systems at
    once, the Q2 report arms.
13. **`closing_report.py`:** D11 through `ab_compare.py compare --a S --b ANCHOR --aa S'` (and
    `--b X`), against every finalist.
14. **Corpora (D3):** fetched on the lease, never synced; the wiki-500k and logs-1M samples.

## Open questions (owner)

Q1 to Q6 are answered (ledger, "Owner rulings"): X is the final Xapian-only release (D1); compare
at the not-durable tier with E's default reported beside it (D5); E and Q only; Q4 (a) and Q5 (a)
as recommended (D6, D2); losses published as losses, not blocking the release (D11).

**Q7. The metal window.** Metal needs `METAL_OK=1` for an approved window, and the plan is about 73
runs at up to $3.57 each (D14). Each run takes the account's whole 96-vCPU quota while it is up
(Detent shares the account), and each parked day costs about $4.6 for the volume. (a) `DAILY_USD`
stays 25: 7 metal runs a day, about 10.5 consecutive days, about $48 of parked volume, and every
other session's leases come off the same $25; (b) `DAILY_USD` 50 for the window: 14 runs a day,
about 5 consecutive days, about $24 of parked volume; (c) 100: about 3 days, the quota held most of
each day. `MAX_RUN_MIN` stays 45 in every option: one metal launch measured 39 s of boot, so longer
runs would save little. Recommendation: (b), on dates agreed with Detent's lead, starting once the
dry run's ledger section (D9) and Q4's `lease` disk options are in place: the same run spend as
(a) in half the days, at half the parked cost.

## First steps

1. `harness/systems.py` with the Xapiand, E and Q adapters, the neutral query spec and its
   translations, D13's cache settings and count modes, D8's count check, and D13's probe, tested
   against a local Xapiand and against E and Q started from their tarballs in a scratch directory.
2. `loadtest.py --system`, batched multi-client and resumable indexing, the latency phase with
   CPU-seconds and the client floor; delete `es_loadtest.py` and move its three references.
3. `stall_load.py --system --procs` with T5's saturation options; `wide_search_check.py --system`
   with the tenant phases and T8's readings; T7's crash audit for every system.
4. `closing_bench.sh` (cgroups, cpusets, units, rotation, S', anchors, `--plan`, phase times, host
   identity) and `closing_report.py` (D11 through `ab_compare.py`).
5. Corpus fetch scripts for C-wiki, C-logs and C-tenants with sha256 checks, run on the lease.
6. The dry run on a `c7a.8xlarge` (D9, D14's eleven units) and its ledger section.
7. The metal window, once Q7 is answered and `lease` takes Q4's disk options.

Q7 blocks only step 7, and so does Q4's `lease` change (shared dotfiles tooling, its owner's to
make). Steps 1 to 6 are not blocked.
