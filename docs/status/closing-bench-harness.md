> Status: CURRENT (2026-11-04) the ledger of the closing benchmark's harness (steps 1 to 3 of `docs/design/closing-benchmark.md`, with revision 1's harness changes folded in where cheap).

# closing-bench-harness

- Design: `docs/design/closing-benchmark.md` (revision 0 as built from; revision 1 at `Xapiand:791a93ed4` read for the harness changes); how it works: `docs/mechanisms/closing-harness.md`
- Branch / worktree: `fix/closing-bench-harness` at `~/code/Kronuz/Xapiand-wt/closing-harness`, off `design/closing-benchmark`
- State: steps 1 to 3 built, lint and type clean; proof incomplete (see Blocked on); steps 4 and 5 not started
- Done criteria (set by the lead; contributors tick, never edit):
  - [ ] K1 (to be set by the lead)
- Done: see the Log below for commits and proof
- Next: the three conditions from the architect's re-check at `Xapiand:cf75407dc` that land in later steps: C5 (T7's crash audit on the pristine C-logs copy: `closing_mixed.py` audits only ids matching `w<n>-<n>` against its log of everything sent, but runs on its own `mix` index, not on the C-logs copy), C6 (`closing_report.py` requires five pairs per row), C3 (corpora and indexes under `~/data`, outside `~/work`); `closing_bench.sh` and `closing_report.py` (step 4; revision 1 D10, D11, D14: units, `--plan`, anchors, host identity, the D11 rule); the corpus fetch scripts (step 5; D3 and revision 1's "fetched on the lease"); the sampled query sets for C-wiki and C-logs (`harness/closing/queries/` holds only the 14-query smoke set); strata `docs/BENCHMARKS.md:58` (another repository) still names `es_loadtest.py`
- Blocked on: the host, not the owner or architect. After the first live run, `quiet` had back-to-back exclusive holds and then more than four queued light holds; `test_closing_harness.py` waited 45 minutes for a light slot and was withdrawn. NOT RUN on the final code: `test_closing_harness.py` (offline and Live groups), the loadtest, stall_load, closing_tenants and closing_mixed runs against the three systems, the mutants and the revert checks. Run first: `quiet light cbh 30 "closing harness" -- python3 -I harness/test_closing_harness.py`, then `CLOSING_LIVE=1 CLOSING_XAPIAND=... CLOSING_ES=<tarball>/bin/elasticsearch CLOSING_QUICKWIT=... CLOSING_LOGS=<logs ndjson> python3 -I harness/test_closing_harness.py Live`
- Findings folded in: L1 (CB-R1) from the lead's message: result caches off, count modes, varied bounds, the cache probe. Revision 1 items 1 to 11 partly (below)
- Design vs code: see the section below
- Last verified: against `Xapiand:791a93ed4` (revision 1) D5, D10, D11, D13, D14 and the T4 to T8 rows, 2026-11-04

## Design vs code

1. `es_loadtest.py` is deleted and its Xapiand references moved (`docs/dev/testing.md:32` and `:260`,
   `docs/src/content/docs/benchmarks.md:103`, the `CMakeLists.txt:2060` comment) to `loadtest.py --system es`. That
   command takes a corpus and a schema, not the accounts mix: accounts rows carry Xapiand constructs (`_point`,
   `_namespace`, `_language`) with no neutral form, so the accounts-on-Elasticsearch comparison is retired. Strata
   `docs/BENCHMARKS.md:58` (another repository) still names the file.
2. The term class of C-wiki: `queries.txt` has one query tagged `term` (counts: union 301, intersection 300, phrase 300,
   intersection_union 40, negated 19, term 1, two-phase-critic 1), so "40 per class, 160 queries" does not hold. Not built
   yet (sampling script is step 5's); noted for the design.
3. A query's `count` command is run on an aggregation only as a search (the aggregation's own count mode is the search's).
4. Quickwit's date-time range queries use RFC 3339 strings, plus `start_timestamp`/`end_timestamp` on the time field
   (its documented split pruning). Not in the design; a choice, in favor of what Quickwit's docs recommend.
5. T6's probe visibility is a single poller counting "any probe so far" (revision 1 D13/T6: one poller at 10 ms), so
   probe k is visible when the count first reaches k, which assumes probes become visible in write order.
6. `stall_load.py --system` loads nothing unless `--corpus` is given (T5 runs on an index `loadtest.py` loaded).
7. `closing_mixed.py` writes its own `mix` schema (ids `w<writer>-<n>`), not C-logs writes; Q is append-only there (it
   has no delete or replace by id).
8. X's `--shards` is `Launch.shards` (an arm setting: 1 and 5); tenant indexes are one shard.
9. Local tests ran against Elasticsearch 9.3.4 (the newest 9.x with a `darwin-x86_64` tarball; 9.4 and later publish only
   `darwin-aarch64` and linux) and Quickwit v0.9.3, on an Intel Mac; the Linux versions are the lease's.

## Proof so far

- Live, before the host filled (adapters of commit `Xapiand:2720bedcb`, 20,000 log lines, Xapiand master build, Elasticsearch 9.3.4, Quickwit v0.9.3): all three systems agreed on every one of 14 smoke queries' exact counts and on the aggregation buckets (dropped: none; order differences: none), the result-cache hit counters stayed at 0 over a pass of every query ten times as search and count (E's filter cache hit 43 times, Q's predicate cache 0), effective settings read back (Q's `runtime_cpus` shows the host's 16 CPUs, as expected without a cpuset). A first run found and the fix cured: Elasticsearch read numeric range bounds as milliseconds (0 matches; now `format: epoch_second`), Quickwit's sort direction is the opposite of the usual (a minus sorts ascending), a raw `du` over Quickwit's sparse 256 MB WAL reported 269 MB for 3 MB (now `st_blocks`), and Quickwit's ingest needs a moment after index creation (now polled).
- Static: `ruff format`, `ruff check` and `pyrefly check` are clean on `systems.py`, `loadtest.py`, `stall_load.py`, `closing_tenants.py`, `closing_mixed.py`, `test_closing_harness.py`.
- Not yet: everything listed under Blocked on, including the mutants (revert E's `request_cache=false` and the index setting; give Q a nonzero partial request cache; flip a count mode; drop `vary`) that the tests are written to catch.

## Log
- 2026-11-04: `Xapiand:2720bedcb` adapters; `Xapiand:a81277e18` the tools, tests, note and ledger; a third commit adds C7 (segments settled after a load) and the marked-stream audit filter.
- 2026-11-04: read the design and the harness; `systems.py` (commit 1); `loadtest.py`, `stall_load.py`,
  `closing_tenants.py`, `closing_mixed.py`, `test_closing_harness.py`, mechanism note (commit 2).
