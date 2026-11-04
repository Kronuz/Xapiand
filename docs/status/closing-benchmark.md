> Status: CURRENT (2026-11-03) the ledger of Phase 8b's closing-benchmark design.

# closing-benchmark

- Design: docs/design/closing-benchmark.md (revision 1); reviews: docs/reviews/closing-benchmark-design.md (preliminary, REJECT on revision 0, `Xapiand:33ccf9e3e`)
- Branch / worktree: `design/closing-benchmark` at `~/code/Kronuz/Xapiand-wt/closing-benchmark` (local only, never pushed)
- State: designing (revision 1, ready for the architect's re-check)
- Done criteria (set by the lead; contributors tick, never edit):
  - [ ] K1 (to be set by the lead)
- Done: revision 0 (`Xapiand:6e7ac4104`); revision 1 folding in L1 to L4, CB-R5 to CB-R13 and C2 (this commit)
- Doing: nothing; stopped at ready for the architect's re-check
- Next: the architect's re-check of L1 to L4 (scoped to `33ccf9e3e..HEAD`); the lead forwards the design's "Harness changes in revision 1" list to the harness developer (`fix/closing-bench-harness`); owner answers Q7; then step 6, the dry run
- Blocked on: Q7 (the metal window: `METAL_OK`, `DAILY_USD`, dates agreed with Detent) blocks only the metal (step 7), as does Q4's `lease` disk options (dotfiles tooling, its owner's). Steps 1 to 6 are not blocked.
- Findings folded in (review `docs/reviews/closing-benchmark-design.md`):
  - L1 / CB-R1: D13 (result caches off, filter caches stated with counters, range bounds drawn per run, count modes per class, the step-1 probe)
  - L2 / CB-R2: D5 (tier renamed "acknowledged before the write is durable"; per-system process-crash and power-loss windows with sources: S and X from code, E from elastic.co, Q from Quickwit v0.9.3 source); T7's crash leg uses `engine_crash_check.sh`'s audit for every system, process crash stated
  - L3 / CB-R3: D14 (units never split across runs, one host per round, 45-minute runs, `--plan` packing, 30-minute caps on E and Q index-per-tenant creates, corpora fetched on the lease, the owner's prerequisites in Q7 and D6, cost redone with boots, builds, calibration and reruns)
  - L4 / CB-R4: D10 and D11 (calibration rounds 1 and 2 fix the anchor per row; five rounds for every claimable row; "better" is `ab_compare.py`'s verdict plus every round on S's side and a 3% median; T11's heap search against E's own repeat)
  - CB-R5: D2 (S and X screened at 1 and 5 shards; the faster published, stated)
  - CB-R6: T3 (durable commit before `du`, WAL and translog column, natural bytes the gate figure with the departure from strata `BENCHMARKS.md:161` stated, E force-merged reported)
  - CB-R7: D4 (warm-up by time, length checked in the screen)
  - CB-R8: D13 (client floor published beside T4's raw times)
  - CB-R9: D8 (fallback named: publish the class with its drop rate and list; no pre-tokenized input)
  - CB-R10: T8 (compared at N_min, the count every shape reached, plus each shape's curve)
  - CB-R11: D10 (per-row table of rebuild, restart or restore; build-to-build variance checked once in the screen)
  - CB-R12: T4 (CPU-seconds per query)
  - CB-R13: D1 records Q1's ruling; `lease.sh:144`; T6's write rate checked against T2 and its 10 ms lag resolution stated
  - C2: D9 (the dry run's ledger section and the packed plan before booking metal)
- Open (none of the review's findings left open): new owner question Q7 only.
- Design vs code: none (nothing built here). The harness developer's `harness/closing/xapiand.opts` fixes `--shards 1`, which D2 now makes an arm setting (in the harness-changes list).
- Last verified: file:line citations checked 2026-11-03 against this branch (Xapiand master `432299ed0` lineage), the `xapiand` branch at `Xapiand:e692a64b1`, strata `356acbc`, cpp-libraries `95ff17e`, dotfiles `0c3c203` (`lease.sh`, `lease.md`), Quickwit v0.9.3 and mrecordlog `3b3562ef` (fetched to `~/.scratch/closing-benchmark-qw/`), apache/lucene `f72004515e`; E facts from elastic.co docs pages named in D5 and D13. Run-plan estimates: `.scratch/closing-benchmark-r1/run_plan.py` and `run_plan.out` (worktree scratch, ignored by git).

## Log
- 2026-11-03: drafted revision 0. Found while reading: `DB_SYNCHRONOUS_WAL` is never set in `src/` (no durable-on-ack mode, Q2); the `v1.0.0-alpha.10` tag does not build on Linux (fixes on `xapiand`, Q1); `lease` launches a fixed 100 GB gp3 root (Q4).
- 2026-11-03: revision 1 folds in the preliminary review (REJECT, L1 to L4). Found while revising: Quickwit v0.9.3 has a predicate cache (256 MB, filter hit sets) beside the partial request cache, and its default ingest v2 WAL flushes to the OS every 5 s and never fsyncs (`state.rs:228-235`); Lucene never caches term filters and caches range filters after 2 uses (`UsageTrackingQueryCachingPolicy.java`); one metal launch measured 39 s of boot (`phases.tsv`), so boot is a small share of a 45-minute run; the honest cost is about $220 to $250 over 5 to 10.5 days, not revision 0's $65, which raised Q7.

## Owner rulings (2026-11-03)

- Q1: the baseline is the final Xapian-only release (the `xapiand` branch), not alpha.10 as tagged.
- Q2: compare at "acknowledged before fsync", with Elasticsearch's default translog setting reported beside it; a durable-ack option for Xapiand is a separate feature item.
- Q3: Elasticsearch and Quickwit only, plus the Xapian baseline.
- Q6: losses are published as losses and don't block the release.
- Q4 and Q5 stand as recommended (lease disk options; 8 cores and 32 GiB per system).
