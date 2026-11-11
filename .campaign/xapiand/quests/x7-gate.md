# x7-gate: Phase 7c's gate on master

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-gate`.

## Outcome

Phase 7c's gate is run on master and reported: every row T1 to T13 of `docs/design/phase7c.md`
section 4 has its result, at a named master commit, in `docs/gates/phase7c.md`, with the Linux and
FreeBSD runs owed since 7a, and every condition road-to-release carried to the gate is closed or
measured. Xapiand never blocks a reactor, has no offload pool, and loses no throughput against the
pre-7c baseline. Phase 8 starts on this report.

## Acceptance criteria

Rows (`docs/design/phase7c.md` section 4; timing rows T4, T5, T11, T12 by the lead on master, five
interleaved trials, 95% bootstrap interval, under `exclusive` jobs of at most 30 minutes each):

- A1 T1 no offload pool: the thread census lists only section 2's threads during `stall_load.py`; `async_check.sh --strict` exits 0: `harness/thread_census.sh` job
- A2 T2 E2E on strata: 0 assertion regressions, 0 guard hits, 0 ASan reports; a `stored` upload of 2x the body cap and one just under it read back byte-equal: the `asan` check (`engine_e2e.sh`, `stored_stream.sh`)
- A3 T3 recovery: `wal_recovery_check.sh` and `engine_crash_check.sh --reactors 2` pass; writers on two reactors killed mid-run replay to the model: the `recovery` check
- A4 T4 reactor stall: p99 lag at most the 1,024 µs edge on every loop under the mixed load, p99.9 and max reported; under the standard mixed load, a script-heavy row reporting the per-call bound instead (X4-R1 carried condition): `stall_load.py --phase loaded` jobs
- A5 T5 light-query p99 under heavy load at most 2x its unloaded p99 + 1 ms and below the pre-7c baseline; the same-index run reported as Q6's cost: `stall_load.py` jobs
- A6 T6 reactor faults per 1,000 queries at 2x RAM reported (gated at Phase 6, D15): a Linux VM job, `--index-size 2xram`
- A7 T7 concurrency and shutdown: 0 TSan and 0 ASan reports; the final dump equals the model; SIGTERM at 300 s exits 0 with every acknowledged write durable: the `t7-tsan` check and an ASan run
- A8 T8 overload: light and heavy overloads each give non-zero 503s within one slice of the cap; in-flight and demoted peaks reported; RSS within 10% of T4's run; a 16-record registry gives 503s, not errors: `stall_load.py --overload` jobs, `test_readers_cap`
- A9 T9 cancellation: a disconnected read frees its reactor within one slice and the cancel counter rises; a half-closed write completes durably: the `build-test` check
- A10 T10 Lua bounds: every case of the row passes, all 4xx; T4 holds with four looping scripts and four looping library calls; with 16 clients each sending a worst-case call, a light GET's p99 stays near the cap's 20 ms (X4-R1: one call holds a reactor about 20-26 ms): `harness/lua_bounds.py` and `stall_load.py --lua-loop 4` jobs
- A11 T11 dispatch cost per light request at most half the offload hop's, CPU and wall (D6): `harness/profile_light.sh --compare` jobs
- A12 T12 RESTORE docs/s and saturated query QPS each at least 1.0x pre-7c master, on a host with more than 10 CPUs, the CPU count stated: `profile_bulk.sh`, `loadtest.py` jobs
- A13 T13 every gate: ASan, UBSan, TSan; coverage at TESTING.md's targets; restore corpora replayed; per-open-index cost at 10,000 open indexes and the reader registry's bytes stated (X3-5 carried): the `asan` and `tsan` checks and a coverage job
- A14 [artifact] The gate report, every row with its commit and jobs: `docs/gates/phase7c.md`
- A15 C5: Linux and FreeBSD runs of the gate's functional rows (owed since 7a; arm64 before 7b): Linux VM jobs and CI's FreeBSD job

Carried conditions (road-to-release "Carried conditions"; ledgers named):

- A16 Wide searches: a 10,000-index search's latency and peak pinned views measured (wide-search ledger Next; carried "Wide searches"); SR3 closed with `stall.py` against real indexes: `harness/wide_search_check.py gate` job
- A17 Owed async rows decided (D4 to D7) so `--strict --owed` gates (carried "Before X7"; met by `phase7c-gate-prep`): the `build-test` check
- A18 [artifact] D16 restated to the code's order, and an index that cannot close within the 35 s bound exits 75, documented (carried "Before X7"): `docs/design/phase7c.md`
- A19 X6: the stop's worst case is one bound, not the graceful wait plus 70 s; `--committers`, `num_committers` and the `DatabaseCleanup` box are gone (carried "X6"): the `removed` check and `graceful_shutdown_check.sh`
- A20 X3-1 C3: `StrataReaders::claimed()` uses the registry's non-allocating `claimed()`, and the admission gate stays at `readers_per_index` until X3-2 (carried "X3-1's C3"; `pins-2026-11` records C3 met): the `build-test` check
- A21 One shape everywhere: CPU per request and p99 on Linux x86-64 plus one macOS check, allocations and handlers per request no worse than head; L3's review probes rerun on the new API (carried "One shape everywhere"): jobs, against the threshold the owner sets (PLAN open question 7)
- A22 X3-4: `Shard::view()` measured at 1 and 8 threads on one index (phase7c-x3 ledger, Remaining): job
- A23 [decision] T4's treatment of the control loop's 100 ms idle probe (probe at 1 ms always, or read only while busy) (phase7c S1 finding 1): a Decisions row
- A24 Perf: the sliced-search residual and the keep-alive watch byte's extra read settled before T12 (road-to-release Perf; `perf-watch-peek`; the prospect half landed at prospect:5c98f81, 1.010 to 1.028 of the pre-sliced loop, and the lead confirms it suffices): T12's jobs at a master pinning both

## Contracts

Consumes master after `x-pool` and `perf-watch-peek` land. Provides `docs/gates/phase7c.md`, which
Phase 8's quests cite.

## Decisions

- road-to-release names an architect gate review: here one correctness review of the report and
  its evidence, plus a security review for the wide-search re-check the wide-search and wide-sr8
  ledgers left open (SR1, SR2, SR4 to SR7, the nested walk).
- T11's second build pins http's `trace/hop` (http:10c5074, never merged; S1 K7's commands);
  S1's ledger says delete it after the post-7c T11 run.
- Open (lead): the pre-7c baseline the rows compare against (S1 K7, `docs/perf/phase7c-baseline.md`)
  is the developer's preliminary run; the lead's re-measurement on master is part of A5, A11, A12.
- Prior evidence (Linux, AWS `c7a.8xlarge`, 2026-11-03, Xapiand:432299ed0 against Xapiand:142b2ca77,
  smoke values): `docs/perf/phase7c-linux-2026-11-03/`. Every functional row passes except T10 under
  ASan (a light GET took 134.5 ms beside the scripts, limit 50 ms: A10). T12's loadtest RESTOREs at
  0.18x pre-7c (median 1,800 against 10,244 documents/s; queries 1.22x): A12 fails there by about
  5.7x unless a later commit changed it. The local closing baseline of 2026-11-04 adds a T11 idle
  memory REGRESSION against the Xapian-only release, 1.28x (`docs/perf/closing-baseline-2026-11-04/`).

- Lead, 11-11 (R4): T8's functional overloads run at `--processors 2`. The 503-within-a-slice claim
  does not depend on the reactor count, and 16 reactors would need about 20,000 loopback connections
  (past one address's 16,384 ephemeral ports). The 16-reactor timing suite keeps its own rows.
- Lead, 11-11 (A13): the restore corpora are quest x7-coverage's A2 (`tests/corpora/`); A13 cites its
  replay job once x7-coverage lands, and waits on it until then.

## May land early

None: the report is one landing.
