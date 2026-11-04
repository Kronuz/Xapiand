# Xapiand review: Phase 8b closing benchmark (preliminary design)

- Date: 2026-11-03.
- Reviewed: `docs/design/closing-benchmark.md` revision 0 and `docs/status/closing-benchmark.md`
  (with the owner's rulings on Q1 to Q6) on `design/closing-benchmark` at `Xapiand:71b5f0275`.
- Checked against: strata `docs/PLAN.md:182` (margins) and `:691` (the closing benchmark),
  strata `docs/BENCHMARKS.md:46`, `:58`, `:137`, `:155`, `:161`, strata `docs/perf/c1-field-tenants.md`,
  `docs/gates/phase7c-linux-dryrun.md`, campaign `references/lease.md`, `scripts/lease.sh` and
  `references/bench-lease-policy.json`, cpp-libraries `tools/ab_compare.py`.
- Checked by: code reading at file:line on the branch (Xapiand master `432299ed0` lineage), the
  `xapiand` branch (`Xapiand:e692a64b1`), strata main, prospect main; probes in
  `.scratch/closing-benchmark-review/`:
  - `check_citations.sh` (output `check_citations.out`): the WAL's acknowledgement path, the
    crash checks' audits, `ab_compare.py`'s statistic, Xapiand's shard default, `lease`'s disk and
    deadline.
  - `stats_probe.py` (`stats_probe.out`): what a 95% percentile bootstrap of the median can say
    over 3 and 5 rounds.
  - `plan_probe.py` (`plan_probe.out`): the cost table against `lease`'s caps and C1's index
    creation rate.
  - Not run: no build (the host was at load 27 on 16 CPUs; nothing here needed one), no benchmark,
    no Elasticsearch or Quickwit started locally (their cache and count behavior below is from
    their documentation, to be confirmed by the step-1 probes L1 names), no AWS, no `lease`.

## Verdict

**REJECT (revision 0): the plan measures the right axes, but four things would make published
numbers wrong or attackable: result caches and count semantics are unpinned, the durability tier
is mislabelled and T7's crash audit cannot pass as written, the run does not fit one-shot
`lease run`, and D11's "better" rule is not what the tools compute.**

All four are design-text fixes; none needs a new mechanism. First steps 1 to 5 (the harness) are
not blocked: L1's settings belong in the adapters step 1 builds, so build them in.

What holds, and reproduces:

- The system set, version pinning and binary provenance (D1): `v1.0.0-alpha.10` is
  `Xapiand:3fe13a2fb`, `Xapiand:27191e0ff` is on the `xapiand` branch, 22 commits past the tag.
- Equal cores and memory by cgroup v2 and cpuset with SMT siblings idle, each system told its core
  count (D2): `--processors` at `src/opts.cc:189`, `--shards` at `:153`, `--database-pool-size` at
  `:159`, shard default 5 at `src/opts.cc:43` and `src/opts.h:75`.
- Public corpora pinned by sha256 (D3), and dropping `accounts.ndjson` for its 1,000 replicated
  lines (`harness/loadtest.py:50-55`): right call. The C-wiki figures match prospect
  `docs/perf/format13-gate-vm.md:14` and strata `docs/research/indexing.md:118`.
- One harness plus adapters (D7): `loadtest.py:93` (one RESTORE), `:114` (closed loop),
  `:70` (settle), `stall_load.py:875` (`--saturate`), the `es_loadtest.py` references at
  `docs/dev/testing.md:32`, `:260` and strata `docs/BENCHMARKS.md:58` all hold. The generator bound
  (about 3,200/s) is at `docs/gates/phase7c-linux-dryrun.md:35`, clang 20 at `:4` and `:17`.
- The count check before timing (D8), staged smoke, screen and confirm (D9), a rotated order with an
  A/A arm (D10), and Q6's "losses published as losses" (D11).
- `DB_SYNCHRONOUS_WAL` is never set on master (`src/database/flags.h:42`; read at every WAL call
  site, `src/database/shard.cc:747` to `:1081`); on the `xapiand` branch only replication sets it.
- The engines-alone pair's tools exist (`prospect/bench/query_trials.sh`, `index_trials.sh`,
  `sample_queries.py`, `phase2/gate.py`).
- C1's numbers (`c1-field-tenants.md:45`, `:46`): 980 indexes at the 1,000-shard limit; 3,700 in
  576 s with it raised, still creating at about 6.4 a second.

What blocks: L1 (result caches and count semantics), L2 (durability label and T7's audit), L3 (the
run plan against `lease run`), L4 (D11's claim rule).

## Findings

**CB-R1. Result caches and count semantics are unpinned, so T4, T5 and T6 can time stored
answers and unequal work (blocking, L1).** T4 takes each query's fastest of 10 runs over a fixed
set of 160 wiki and a few dozen log queries, and T5 and T6 replay the same set open-loop. By their
documentation, Elasticsearch's shard request cache is on by default for `size: 0` requests, which
is exactly the count and aggregation classes, and its node query cache keeps the bitsets of
filters seen a few times (range and bool filters); Quickwit's partial request cache keeps leaf
search results for an identical request on a split. Runs 2 to 10 of those classes are then cache
hits, not searches, and the fastest of 10 picks them. D4 names none of these. Counts differ in
kind too: Elasticsearch counts exactly only up to `track_total_hits` (default 10,000; above it the
total is a lower bound, so D8's comparison sees "10,000"), Quickwit counts every hit by default,
and Xapiand estimates unless the request sets `check_at_least` (`src/server/search_views.cc:2384`).
A "top-10" class where one system counts exactly and another stops early measures different work.
*Fix:* D4 pins, per system and per class, (a) result caches off: Elasticsearch
`request_cache=false` on every request (or `index.requests.cache.enable: false`), Quickwit's
partial request cache capacity 0, and any result cache in Xapiand or strata named and off; (b)
filter caches (Elasticsearch's node query cache) left on as normal operation but stated, with T4's
range and term-with-range classes drawing bounds per run from a seeded set so a repeat cannot hit
a cached bitset; (c) the count mode: the top-10 class without an exact total everywhere
(`track_total_hits: false`, Quickwit `count_all: false`, Xapiand's default estimate) and the count
class exact everywhere (`track_total_hits: true`, Quickwit's default, `check_at_least` at the
index size), with D8 run in the exact mode. Step 1 proves each with a probe: the same query ten
times with cache hit counters read back (`_nodes/stats/indices/request_cache`, Quickwit's cache
metrics) staying at 0, and the effective settings embedded in every result.

**CB-R2. "Acknowledged before fsync" understates Xapiand's tier, and T7's crash audit cannot pass
as written (blocking, L2).** A non-synchronous WAL write is queued to a writer thread and the
request acknowledged before the line reaches the file, not only before fsync
(`src/database/wal.cc:1111-1115`, `enqueue` at `:848-856`; the same on the `xapiand` branch,
`wal.cc:1158`, `:1178`); the fsync is then debounced (`wal.cc:108`). The repository already knows
this: `harness/engine_crash_check.sh:9-11` requires only writes "acknowledged more than GRACE
seconds before the kill" (GRACE defaults to 2 s, `:130`) because "the async WAL may lose the last
moments". T7's crash leg says lost acknowledged writes "must be 0 outside D5's tier's documented
window", but D5 documents no window for any system, and the audit it names,
`engine_mixed_load.py`'s model check, is a SIGTERM audit (`harness/engine_mixed_load.py:13-15`)
that requires every acknowledged write; under SIGKILL S and X fail it by design. For Quickwit, D5
equates the tier with `commit=auto`, but that parameter sets when documents become searchable
(split publication), not how durable the acknowledgement is; that is its ingest WAL's persist
policy. Q2's ruling (compare at the not-durable tier) stands; the label and the audit are what
change. *Fix:* D5 names the tier "acknowledged before the write is durable" and gives each
system's loss window from code or documentation at the pinned version (S and X: the WAL queue,
then the debounced fsync; Elasticsearch `async`: `index.translog.sync_interval`, 5 s by default;
Quickwit: its ingest WAL's persist policy, cited from its source). T7's crash leg reuses
`engine_crash_check.sh`'s audit for every system: acknowledged writes lost, the age of the oldest
one lost against the stated window, nothing unwritten present, nothing duplicated. T7 states that
SIGKILL tests a process crash, not power loss.

**CB-R3. The run plan does not fit `lease run` (blocking, L3).** `lease` allows one command per
run with `--timeout` at most 45 min (`lease.sh:444`), results through `$OUT`. The design gives
stage totals (4 h on the `c7a.8xlarge`, 13.5 h on metal), not runs. `plan_probe.out`:

- 13.5 metal hours is at least 18 runs, each a resume of a parked metal machine whose boot is
  billed and uncounted in the table (1.5 to 4.5 h, $6 to $19 at 5 to 15 min a boot). The AWS stop is
  armed at launch plus timeout plus 10 min (`lease.sh:151-152`), so a boot and sync over 10 min
  cuts the run's end.
- A stop and start can land on another physical host. A round split across runs compares systems on
  different hosts; D10's per-round ratio needs every arm of a round in one run.
- `DAILY_USD` is 10: 2.3 metal hours a day, about six days for the $58 of metal, against the
  design's "can split across two days". `METAL_OK` is 0. Both are the owner's to raise for a
  window, and the design must ask. A window longer than `LEASE_PARK_DAYS` (3) loses the parked disk
  with its indexes and corpora.
- Elasticsearch index per tenant at 10^5: about 260 min of creates per round at C1's rate (falling
  as the cluster state grows), so it fits neither one run nor the 2 h budgeted for all of T8 to T10
  over three rounds; 10^4 takes about 26 min of one run's 45.
- "Sync binaries and corpora" to the metal sends gigabytes up the host's home link; the corpora
  should be fetched on the lease by D3's script, checked by sha256, and kept on the parked disk.

*Fix:* a run table: each lease run's name, type, content (which rows, rounds and arms), measured
duration from the dry run, `--timeout`, and what it writes to `$OUT`; metal boot-to-ready measured
at the first metal run and every run sized to 45 min minus it; every round inside one run; a time
cap for Elasticsearch's index-per-tenant creates as Quickwit has (30 min), its count reached
reported; the owner's prerequisites listed (`METAL_OK=1` and `DAILY_USD` for a stated window, the
window within `LEASE_PARK_DAYS`, Q4's volume); the cost table redone with boots and the S' builds
CB-R11 adds.

**CB-R4. D11's "better" is not what the tools compute, and at 3 rounds it is not a test (blocking,
L4).** `tools/ab_compare.py` reports the median of paired ratios, their range and the A/A ratio
(`ab_compare.py:4-6`); it has no bootstrap (0 occurrences) and compares two series, not S against
the best of three. A 95% percentile bootstrap of the median excludes 1.0 only when every round
favors S (`stats_probe.out`: 4 of 5 rounds 20% better still gives [0.80, 1.05]); that is a sign
test, one-sided p = 0.031 at 5 rounds and 0.125 at 3, where the bootstrap has 10 distinct
resamples. T8 to T10 run three rounds, so their "better" rests on three agreeing rounds. "S over
the best of X, E and Q per round" lets the comparator change between rounds. T11's search for
Elasticsearch's smallest heap uses S's A/A band (S'/S), which is S's noise, not Elasticsearch's.
*Fix:* state the rule as computed: every round's ratio on S's side, the median at least 3%, outside
the A/A range; five rounds for any row the release may claim "better" on, T8 to T10 included, or
those rows publish without the word; the comparator fixed per row as the system with the best
median over rounds; `closing_report.py` computes it from `ab_compare.py`'s paired ratios. T11's heap
search compares Elasticsearch with its own repeat at full heap.

**CB-R5. One shard for X makes it a single writer.** A Xapian database has one writer, and
Xapiand's default is 5 shards (`src/opts.cc:43`). At one shard X's T1 and T2 are one-thread
numbers on 8 cores, an easy target for "the baseline was handicapped", and it flatters S against D11's
"no row may be worse than X". *Fix:* in the screen, run X and S at 1 and 5 shards; T1 and T2 publish
each at its faster setting, stated, and T4 queries the index that run built.

**CB-R6. T3's bytes depend on when each system last flushed.** After a refresh without a flush,
Elasticsearch's translog holds the load until its flush threshold, and the bytes `du` sees depend on
timing. strata `BENCHMARKS.md:161` (adopted "as written") makes force-merged bytes the gate figure,
but T3 force-merges only Elasticsearch. *Fix:* every system's durable commit before `du`
(Elasticsearch `_flush`, Quickwit's last split published, S and X commit), WAL and translog bytes as
their own column, natural and force-merged bytes for all four, or the departure from `:161` stated.

**CB-R7. One untimed pass is a short warm-up for a JVM.** About 200 queries leave Elasticsearch
short of compiled steady state; "fastest of 10" hides some of it, T5 and T6 none. *Fix:* warm every
system the same way by time (for example 60 s of the class mix) or until the pass median settles
inside the A/A range, recorded.

**CB-R8. The client floor compresses T4's ratios.** A Python client at concurrency 1 adds the same
fixed cost to every request; strata `BENCHMARKS.md:137` subtracts a pipe floor, T4 does not. *Fix:*
record the client's floor (a loopback HTTP request to a trivial endpoint per system) beside T4 and
publish raw times with it.

**CB-R9. D8's stop rule may have no fix.** Quickwit's default tokenizer is not UAX #29 and Xapian's
term generator differs from both, so a phrase class may stay above 5% dropped whatever the
configuration. *Fix:* name the fallback before the dry run (publish the class with its dropped list,
or feed identical pre-tokenized text), and list each dropped query with the system that disagreed.

**CB-R10. T8's per-tenant memory compares different tenant counts.** Elasticsearch and Quickwit
index-per-tenant stop at a cap or a time limit while S and X reach 10^5; fixed costs amortize
differently. *Fix:* compare at the count every shape reached, plus each system's curve.

**CB-R11. "Fresh start" is ambiguous.** D10's S' runs "on a fresh data directory": for T4 to T7 that
means S' builds its own full C-wiki and C-logs indexes, which the cost table lacks. *Fix:* say
whether a round restarts the process on the existing index or rebuilds it, per row, and cost it.

**CB-R12. Parallelism inside one query.** At concurrency 1 Elasticsearch (where its concurrent
segment search applies) and Quickwit (splits in parallel) may spread one query over several of the 8
cores. Fair at the node level, but read T4 beside T5. *Fix:* T4 records CPU-seconds per query from
`cpu.stat` beside latency.

**CB-R13. Small corrections.** D1 still presents Q1 as open; record the ruling (the `xapiand` branch
at the release commit). D6 cites `lease.sh:138` for the 100 GB root; it is at `:144`. T6's 5,000
documents a second must sit below every system's T2 rate (checked in the screen), and its visibility
probe's poll interval, which sets the lag's resolution, is stated.

## Blocking items

- L1 (CB-R1): D4 pins result caches off, filter caches stated with T4's bounds varied per run, and
  the count mode per class for every system; step 1's probe shows cache hit counters at 0.
- L2 (CB-R2): D5 renames the tier and gives every system's loss window with its source; T7's crash
  leg uses `engine_crash_check.sh`'s audit for every system and states process crash, not power loss.
- L3 (CB-R3): a run table that fits `lease run` (each run at most 45 min minus measured boot, rounds
  within a run, Elasticsearch's tenant creates capped), the owner's prerequisites listed, the cost
  table redone.
- L4 (CB-R4): D11 states the rule it computes, five rounds for every claimable row, a fixed
  comparator per row, and `closing_report.py` computing it.

## Conditions

- C1 (CB-R5 to CB-R13) folded into revision 1, or each answered in the ledger with its reason.
- C2: the dry run's ledger section shows, per system, the effective settings read back, the cache
  probe, the count-check drop list, and metal boot time, before the metal is booked.

Not run: benchmarks, Elasticsearch or Quickwit locally, any build or test, AWS or `lease`.
