> Status: CURRENT (2026-11-04) the ledger of the closing benchmark's harness: steps 1 to 5 of `docs/design/closing-benchmark.md` built and proven on a lease at smoke scale; C5 to C7 met.

# closing-bench-harness

- Design: `docs/design/closing-benchmark.md` revision 1 (`Xapiand:791a93ed4`); architect's re-check `Xapiand:cf75407dc`; how it works: `docs/mechanisms/closing-harness.md`
- Branch / worktree: `fix/closing-bench-harness` at `~/code/Kronuz/Xapiand-wt/closing-harness`
- State: steps 1 to 5 built, lint and type clean, offline tests green, live tests green on a c7a.2xlarge lease; T8 to T10 units, the E report units, B4/B5 and chunked builds are not run by `closing_units.py` yet (below)
- Done criteria (set by the lead; contributors tick, never edit):
  - [ ] K1 (to be set by the lead)
- Done (all `Xapiand:`): `2720bedcb` adapters; `a81277e18` tools and tests; `9c7dc9409` C7 and the audit filter; `ea04d3804` lease setup, corpus fetch, live proof; `bd429ad4f` marked write stream (C5) and query sets; `059ce076e` closing_bench.sh, closing_report.py (C6), closing_units.py, closing_rows.py; `dc9f858b1` cgroup read and Quickwit readiness fixes; `f51cb24d8` parsers raise ValueError (fuzz findings); `b70a0b237` two report mutants killed; `3593333a8`, `7cfe1bc22` Quickwit 503 retry, crash probe, reaping; the Quickwit-commit wait commit; strata `2769928` (BENCHMARKS.md:58 now names `loadtest.py --system es`, pushed to strata main)
- Next: T8 to T10 units (`closing_tenants.py` end to end per shape, B4/B5 tenant sets built once and restarted on), E1 to E4 report units, `--until-minutes` chunking of B1/B2, a tune step that writes `$CLOSING_DATA/settings.json`, the X binary build (the `xapiand` branch) in `setup.sh`, then the dry run (D9) on a c7a.8xlarge. Run the full B1/B2 on a lease to record C-wiki's sha256 and 5,032,104-line count (`fetch_corpus.sh wiki --record`; `corpora.pins` has `-` for it)
- Blocked on: AWS credentials: `lease audit` and `lease list` fail with AuthFailure since the last run (run 8 parked normally before it), so the final Quickwit T7 stream run with the poke-count fix (`Xapiand:` last commit) is not re-run on a lease and `lease audit` after run 8 could not print OK (it printed OK after run 7: $2.48 spent today across all sessions; this task about $1.6). Open for the lead: Quickwit's crash behavior (below) changes what T7 can claim for Q; `lease run` printed `lease: line 632: unexpected EOF while looking for matching` once after a run (results and park were fine)
- Findings folded in: L1 (CB-R1), C5 (CB-R16), C6 (CB-R17), C7 (CB-R18), C3 as far as the harness goes (data under `~/data`, a deadline inside `--timeout`), C4 (`probe_ranges` already expects only term-absence hits for Q)
- Last verified: against `Xapiand:791a93ed4` (revision 1), 2026-11-04

## Proof

- Offline: `test_closing_harness.py` 49+ tests (translations, count modes, D8 checks, probe, open-loop merge, audit and marked stream, the report through the real `ab_compare.py`, the plan, parsers fuzz).
- Live (c7a.2xlarge lease `cbh2-live`, Ubuntu 24.04, Xapiand Release gcc-13, Elasticsearch 9.3.4, Quickwit v0.9.3): the Live group passes: the three systems agree on the 14 smoke queries and on all 230 generated C-logs templates (kept 230, dropped 0, no order or aggregation differences), result-cache counters stay 0, Quickwit sort and Elasticsearch epoch-second ranges. Short runs of `loadtest.py` (load, latency, clean restart with the cache dropped), `stall_load.py` (saturate, mixed), `closing_tenants.py` (per-index and shared), `closing_mixed.py` (old mode and the marked stream), the cgroup envelope (cpuset 0-1 or 0-3 and the memory limit read back for all three systems), and `closing_bench.sh --smoke` (B1, B2, T1 x3 rounds, T4a, T4b, T5b, T6, T7) all complete against Xapiand and Elasticsearch; Quickwit as below.
- Found by the lease runs and fixed: `-I` hid the sibling modules; the mix schema had no id field, so bulk documents were audited under random ids and every system "lost" them; cgroup counters came from the harness's own cgroup under `sudo systemd-run --scope`; Quickwit answers readiness before its ingester joins; `drop_caches` needed sudo; a failed tool left a node running and blocked the next arm's port.
- Found by fuzz: `Query.parse` raised KeyError (missing class) and OverflowError (`1e999`), `read_rows` AttributeError (null document); each now ValueError (tests fail with the fix reverted, seen before the fix).
- Mutants (a copy of `harness/` with one edit, the named tests must fail; `.scratch/mutants.py`): killed: E request cache on, E index setting on, Q partial cache set, E exact count flipped, Q lower-bound mode dropped, `vary` constant, report accepting four pairs (C6), a throughput anchor read as a time, the A/A band ignored, a changed base passing, the grace inverted, stream documents unmarked, a unit over the budget packed, the deadline ignored, the startup retry removed, the tenant cut out of order. Two survived the first run (throughput anchor, A/A band) and got tests.
- Quickwit crash (qw_crash_probe, 11,900 acknowledged appends, SIGKILL, restart): the count is 5,000 (the published part) for 66 s, then 16,800: appended documents are replayed from the WAL and published at the first commit after the restart (`commit_timeout_secs` 60 s), not at once; a `commit=force` of a new document does not publish them. The T7 audit therefore waits `commit_timeout_secs + 15 s` after a Quickwit restart. The last Quickwit T7 smoke run (20,000-document base) had lost 0 beyond the window, 100 inside it, and a base count one short because the poke document was not yet searchable; the audit now counts the poke only when seen (offline tests green, not re-run live). Whether Q's acknowledgement-to-searchable time after a crash is a result to publish or only a wait is the lead's.

## Design vs code

1. `es_loadtest.py` is deleted and its Xapiand references moved (`docs/dev/testing.md:32` and `:260`,
   `docs/src/content/docs/benchmarks.md:103`, the `CMakeLists.txt:2060` comment) to `loadtest.py --system es`. That
   command takes a corpus and a schema, not the accounts mix: accounts rows carry Xapiand constructs (`_point`,
   `_namespace`, `_language`) with no neutral form, so the accounts-on-Elasticsearch comparison is retired. Strata
   `docs/BENCHMARKS.md:58` now names `loadtest.py --system es` (strata `2769928`).
2. The term class of C-wiki: `queries.txt` has one query tagged `term` (counts: union 301, intersection 300, phrase 300,
   intersection_union 40, negated 19, term 1, two-phase-critic 1), so "40 per class, 160 queries" does not hold. `closing_queries.py wiki`
   draws the term class's 40 queries from the vocabulary of queries.txt (seed 7); the other classes are the file's.
3. A query's `count` command is run on an aggregation only as a search (the aggregation's own count mode is the search's).
4. Quickwit's date-time range queries use RFC 3339 strings, plus `start_timestamp`/`end_timestamp` on the time field
   (its documented split pruning). Not in the design; a choice, in favor of what Quickwit's docs recommend.
5. T6's probe visibility is a single poller counting "any probe so far" (revision 1 D13/T6: one poller at 10 ms), so
   probe k is visible when the count first reaches k, which assumes probes become visible in write order.
6. `stall_load.py --system` loads nothing unless `--corpus` is given (T5 runs on an index `loadtest.py` loaded).
7. `closing_mixed.py` has two modes: the `mix` schema (ids `w<writer>-<n>`; Q is append-only) as a correctness run, and the
   marked C-logs write stream on an existing index for T7 (C5). The `logs` schema gained `stream` and `seq` fields.
8. X's `--shards` is `Launch.shards` (an arm setting: 1 and 5); tenant indexes are one shard.
9. Local tests ran against Elasticsearch 9.3.4 (the newest 9.x with a `darwin-x86_64` tarball; 9.4 and later publish only
   `darwin-aarch64` and linux) and Quickwit v0.9.3, on an Intel Mac; the Linux versions are the lease's.

## Proof history

- Live, before the host filled (adapters of commit `Xapiand:2720bedcb`, 20,000 log lines, Xapiand master build, Elasticsearch 9.3.4, Quickwit v0.9.3): all three systems agreed on every one of 14 smoke queries' exact counts and on the aggregation buckets (dropped: none; order differences: none), the result-cache hit counters stayed at 0 over a pass of every query ten times as search and count (E's filter cache hit 43 times, Q's predicate cache 0), effective settings read back (Q's `runtime_cpus` shows the host's 16 CPUs, as expected without a cpuset). A first run found and the fix cured: Elasticsearch read numeric range bounds as milliseconds (0 matches; now `format: epoch_second`), Quickwit's sort direction is the opposite of the usual (a minus sorts ascending), a raw `du` over Quickwit's sparse 256 MB WAL reported 269 MB for 3 MB (now `st_blocks`), and Quickwit's ingest needs a moment after index creation (now polled).
- Static: `ruff format`, `ruff check` and `pyrefly check` are clean on `systems.py`, `loadtest.py`, `stall_load.py`, `closing_tenants.py`, `closing_mixed.py`, `test_closing_harness.py`.
- Not yet: everything listed under Blocked on, including the mutants (revert E's `request_cache=false` and the index setting; give Q a nonzero partial request cache; flip a count mode; drop `vary`) that the tests are written to catch.

## Log
- 2026-11-04: `Xapiand:2720bedcb` adapters; `Xapiand:a81277e18` the tools, tests, note and ledger; a third commit adds C7 (segments settled after a load) and the marked-stream audit filter.
- 2026-11-04: read the design and the harness; `systems.py` (commit 1); `loadtest.py`, `stall_load.py`,
  `closing_tenants.py`, `closing_mixed.py`, `test_closing_harness.py`, mechanism note (commit 2).
10. D11's "worse" is judged against every finalist, not only the anchor: a row worse than any system that ran five rounds
    is reported worse. "Better" follows the design. The anchor value is the median of rounds 1 and 2.
11. `ab_compare.py` is installed by `setup.sh` from cpp-libraries `95ff17e` (sha256 pinned); `closing_report.py` runs it per
    finalist through its table format, one file per round (a missing round is a placeholder file so rounds stay paired).
12. T7 clean is read from T4's run (`--restart` after the latency pass), not from each arm's start; its rows are written
    under `t7-t4a` and `t7-t4b`.
13. B1 and B2 build the four systems together in disjoint cpusets, resumable but not chunked by time (`--until-minutes` is
    not built); D14's B3 is folded into B2. Units `t8-*`, `E1` to `E4`, `B3` to `B5`, `M0a`, `M0b`, `M1` are in the plan
    but `closing_units.py run` refuses them.
14. Arm settings (clients, batch, shards per system) come from `$CLOSING_DATA/settings.json` when it exists, else 4, 1000, 1;
    no step writes it yet. T6's rates come from the same file (default 5,000 writes and 200 searches a second).
15. `setup.sh` builds Xapiand with gcc-13, LTO off (the CI toolchain), not clang 20 with LTO; the numbers the lead
    publishes need D1's build. The JDK is Elasticsearch's bundled one.
16. Quickwit: the crash audit waits `commit_timeout_secs + 15 s` after a restart (see Proof); the 503 Quickwit answers for
    a moment after a start is retried by the writers for up to 20 s.
