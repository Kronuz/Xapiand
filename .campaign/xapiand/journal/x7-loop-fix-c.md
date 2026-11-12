# x7-loop-fix-c: journal

State: waiting on jobs 261112-051110-ac40f0 261112-042106-305a1a
Quest: .campaign/xapiand/quests/x7-loop-fix-c.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design ready (A1): `docs/perf/x7-loop-stalls.md`, "Design: fix (c), RESTORE's writes off the loop, one shard per
child". Grounded in the A3 job's RESTORE samples (261112-012857-a77bf8, arms 3/6: loop's write in the turn >= 54-55%,
`finalise_write` 46%) and the A2 job's per-loop lag (261112-012855-b34e3c: idle p99 512 us, loaded p99 3.0-5.1 ms).
Sample parser for the shares: `.scratch/shares.py` (untracked).

## Next

0. Read diagnostic 261112-051110-ac40f0 (test_loop_yield at Xapiand:b6cebfc29, light, queued behind an exclusive
   hold): it prints `total` for num_par, num_seq and num_par2 and GET /num_*/117. The open failure is
   `test_new_numeric_ids_match_their_serial_model`: object 31 gets id 133 in one RESTORE and 132 one object per
   request. Both give the id-less object i=16 the new id 117, and the explicit id 117 (i=17) follows it. Per request,
   the explicit 117 replaces it (sim of that order gives 132). In one RESTORE the placement after the join seems to
   miss the just-added doc, so the explicit 117 adds a second one and the least-used counts shift (133). The second
   run gives the same ids, so it is deterministic. Read the totals:
   - num_par 41 vs num_seq 40: a duplicate id, a real bug. Check whether it is pre-existing: the same test with
     `parallel = false` (the old serial path in one request), as a light job. If the bug is new, make placement after
     a join see the serial write. Suspect `Shard::written_keys` vs an add whose key lands when its entry chores
     settle. If it is pre-existing, report it under Blocked on and keep the test's model on one request.
   - Equal totals: the counts differ by timing; find which `written_count` lags.
1. Then run the full suite with `campaign verify x7-loop-fix-c --for A4`. Remove the diagnostic prints in the
   numeric test (the second run and the totals) first, or keep the second run as a determinism check.
2. Mutant jobs (`campaign job submit --quest x7-loop-fix-c --for A4 --fails-without ...`). Each is a one-line sed of
   the fix that builds `xapiand_test_loop_yield`, runs it, and exits 0 only when the named test's CHECK fails:
   - sub-run reversed (iterate `writes` from the end in `apply_slice`) -> test_a_sharded_restore_matches_its_serial_model;
   - writes on the loop (`parallel = false` in `DocIndexer::index`) -> the off-loop test's `0.2` check;
   - children one at a time (`n_kids = 1`) -> the same test's peak check (the cap was 5 on this host);
   - no join before a new numeric id (`new_numeric` returns false) -> test_new_numeric_ids_match_their_serial_model;
   - a missed write fails (`write.error = current_exception()` instead of `missed`) -> test_a_write_missed_since_placement_is_placed_afresh;
   - guard not restoring on a throw (dtor returns early when `std::uncaught_exceptions()`) -> test_a_throwing_apply_slice_restores_the_engines_chores.
3. E4 (strict, still queued): job 261112-042106-305a1a. Its report is at
   `~/.scratch/campaign/jobs/261112-042106-305a1a/a1/log`. Read: a handler tail2 near the lag tail with cpu/wall ~1
   means H1 (long turns). A period tail without a handler tail means queued short turns. cpu/wall well under 1, or no
   period tail, means H3 (descheduled). Record the numbers here and in the note's T4/T5 paragraph.
4. A3 (T12, exclusive, <= 10 min), then A2 (T4/T5, exclusive).

Suite so far (261112-045157-27ee29 at Xapiand:7e19c4475): every test passes but test_loop_yield.
- Off-loop: the loop used 119 of the process's 1,356 ms of CPU (9%), against 34% before the writes moved. The
  threshold is now 20%, halfway. The children cap was 5 and the peak was 5.
- Model equality passes: largest range 300, 240 ids repeated within a sub-run.
- The throw test passes: 1 of 20 writes failed with the slice, and the chores were restored.
- B1 failed because the seam wrote with version 0, a conflict. Fixed in Xapiand:80d00a57f; diagnostic
  261112-050521-14bfc2 shows 3 deferred and the check passing.
- `async_check --strict` has not run yet, because ctest's failure stops the chain. Expect the two allowlist counts
  under Blocked on.

## Blocked on

- Lead: `harness/async_allowlist.tsv` (outside the file set) needs the fix's new hits. `async_check --strict` counts
  `lock_shard` in handler.cc at 26 (row covers 24: the children's and placement's `endpoints[shard]` locks) and
  `sync_wait` in shard.cc at 9 (row covers 7: the apply slice's `do_replace` and `commit_blocking`'s). Rows to add:
  `src/database/shard.cc	sync_wait	entry\.do_replace	allow	2	a RESTORE's apply slice writes on a WorkPool worker in the index's turn, engine chores inline (x7-loop-fix-c)`
  and `src/database/handler.cc	lock_shard	endpoints\[shard\]	allow	2	a RESTORE's placement and apply children, one shard's turn each under engine::counted (x7-loop-fix-c)`;
  or raise the two existing rows' counts to 9 and 26.
- Lead: T4/T5 (A2) cannot be shown to pass from this file set on current evidence. Fix (c) moves RESTORE only (1.3%
  of loop time in the mix). The loaded tail (p99 3-5 ms over a 0.5 ms idle floor) needs multi-ms events; E4 decides
  between long turns (H1, likely the per-hit frame buffer's `madvise`) and descheduling (H3). If H1: fix 3's files
  (`src/engine/strata/shard.cc`, `doc_view.{h,cc}`, `src/aggregations/bucket.h`) are outside the set. Options:
  (a) widen this quest after E4; (b) a sibling quest for fix 3 and A2 moves there; (c) keep A2 here and accept a MISS
  report. Recommendation: (b), with E4 run here first.

## For x7-gate

Amendment text for `docs/design/phase7c.md` (lead routes it; it does not block this quest):

- D4, the chore-class table's `flush` row, its work column gains: "RESTORE writes: a shard's writes in its turn,
  engine chores inline (x7-loop-fix-c)".
- D12, the RESTORE sentence becomes: "A RESTORE prepares its plain objects in query chores of its own group, and
  writes them in flush chores of that group: a range's writes are placed in one turn per shard, and a child task per
  shard, at most W = min(sub-runs, max(1, (threads - 1) / 2)) at once, holds that shard's turn while each chore writes
  a slice of about 1 ms with the engine's chores inline. Its loop places, joins and records results; a new numeric
  id, and a write whose document left since placement with every later write of its id, take the serial path after
  the join, in input order (docs/perf/x7-loop-stalls.md, fix (c))."

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | [artifact] `docs/perf/x7-loop-stalls.md`, "Design: fix (c), RESTORE's writes off the loop, one shard per child" (review pending) |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Each shard's child runs under `engine::counted`, so "a task holds one index's turn at most" holds per child
  rather than being relaxed; the parent holds no turn while it awaits them.
- The write chore reuses `commit_blocking`'s shape (inline engine chores, `BlockingAllowed`, `sync_wait`) as one
  guard for both, rather than splitting strata's write API (outside the set).
- Placement asks each shard once per range; `NEW_NUMERIC_KEY` and a vanished `must_exist` document fall back to the
  serial path after a join, so placement equals a serial restore's exactly.
- Turns are held one chore (about 1 ms) at a time, not per object: fewer hops; the same-index row reports the cost.
- The batch-end drain stays (D1's batch rule unchanged); carrying a run across batches is the next lever only if A3
  falls short.
