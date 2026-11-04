> Status: CURRENT (2026-11-04) the ledger of the closing benchmark's harness (steps 1 to 3 of `docs/design/closing-benchmark.md`, with revision 1's harness changes folded in where cheap).

# closing-bench-harness

- Design: `docs/design/closing-benchmark.md` (revision 0 as built from; revision 1 at `Xapiand:791a93ed4` read for the harness changes); how it works: `docs/mechanisms/closing-harness.md`
- Branch / worktree: `fix/closing-bench-harness` at `~/code/Kronuz/Xapiand-wt/closing-harness`, off `design/closing-benchmark`
- State: steps 1 to 3 built and tested (see Done); steps 4 and 5 not started; waiting for the host (see Blocked on)
- Done criteria (set by the lead; contributors tick, never edit):
  - [ ] K1 (to be set by the lead)
- Done: see the Log below for commits and proof
- Next: `closing_bench.sh` and `closing_report.py` (step 4; revision 1 D10, D11, D14: units, `--plan`, anchors, host identity, the D11 rule); the corpus fetch scripts (step 5; D3 and revision 1's "fetched on the lease"); the sampled query sets for C-wiki and C-logs (`harness/closing/queries/` holds only the 14-query smoke set); strata `docs/BENCHMARKS.md:58` (another repository) still names `es_loadtest.py`
- Blocked on: nothing from the owner or architect. The host held a long exclusive `quiet` hold during the last part, so the final test run is queued (Log)
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

## Log
- 2026-11-04: read the design and the harness; `systems.py` (commit 1); `loadtest.py`, `stall_load.py`,
  `closing_tenants.py`, `closing_mixed.py`, `test_closing_harness.py`, mechanism note (commit 2).
