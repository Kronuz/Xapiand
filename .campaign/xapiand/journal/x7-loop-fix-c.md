# x7-loop-fix-c: journal

State: waiting on jobs 261112-042106-305a1a
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

0. E4 (lead's msg, 11-12): build 261112-042104-fcdc82 (light, done; first try 041113-b08090 failed on macro clashes, MS and L; xapiand with Asio custom handler tracking,
   `~/.scratch/x7-loop-fix-c/e4/e4_tracking.hpp` via `-include e4_pre.h`; binary to `~/.scratch/x7-loop-fix-c/e4/<commit>/`),
   then 261112-042106-305a1a (strict, 10 min): the T4/T5 loaded mix on it with `E4_OUT`, then on the plain fix
   binary (0072ba93e) for overhead; `e4_report.py` gives per loop the handler and busy-period tails (fraction of the
   window a 1 ms tick would wait >= 1/2 ms) and wall vs thread CPU. Read: handler tail2 ~ lag tail with cpu/wall ~1
   = H1 (long turns); period tail without handler tail = queued short turns; cpu/wall well under 1 in long
   handlers or periods, or no period tail at all = H3 (descheduled). Record the numbers here.
1. Design review CHANGES (`.campaign/xapiand/reviews/x7-loop-fix-c-design.md`): B1, B2, N1, N2, N3, N5, N6, N7
   folded as text in the design; N4 written as the owner's options (default kept). Tests for B1/B2/N5 come with
   the build.
2. Build step 2 of the design (apply chore + guard shared with `commit_blocking`, range placement, children under
   `engine::counted`), its tests and mutants; suite as a job.
3. A3 (T12) exclusive job; then A2 with fix (c) and E4 (handler-duration histogram) exclusive jobs.

## Blocked on

- Lead: the build needs `src/database/shard.h` (a `Shard` method for the apply slice, since `ShardEndpoint`'s
  `do_replace*`, `engine_` and `chores` are private to `Shard`, `ReplayTarget`, `DatabasePool`) and
  `src/database/pool.h` (N5's debug accessor beside the chores guard) in the file set.
- Lead: the design review (A1).
- Lead: phase7c.md D4 `flush` row ("RESTORE writes") and D12's RESTORE sentence need amending with the build;
  `docs/design/phase7c.md` is outside the file set. Widen, or the lead amends it.
- Lead: T4/T5 (A2) cannot be shown to pass from this file set on current evidence. Fix (c) moves RESTORE only (1.3%
  of loop time in the mix). The loaded tail (p99 3-5 ms over a 0.5 ms idle floor) needs multi-ms events; E4 decides
  between long turns (H1, likely the per-hit frame buffer's `madvise`) and descheduling (H3). If H1: fix 3's files
  (`src/engine/strata/shard.cc`, `doc_view.{h,cc}`, `src/aggregations/bucket.h`) are outside the set. Options:
  (a) widen this quest after E4; (b) a sibling quest for fix 3 and A2 moves there; (c) keep A2 here and accept a MISS
  report. Recommendation: (b), with E4 run here first.

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
