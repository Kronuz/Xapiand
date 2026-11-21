# split-workpool: journal

State: waiting on jobs 261121-130515-92a210
Quest: .campaign/xapiand/quests/split-workpool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Only build-test 261121-130515-92a210 at workpool:22acbb0 remains (queued behind other light holds after one 600 s wait). Branch pushed; CI run 44949736609 queued on GitHub.

## Next

1. When 92a210 passes and CI is green, State "ready for review". The `split` gate check fails only on docs/source-layout.md (the landed tool reads .md as source); the lead reruns it once split-check-sources lands.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130515-92a210 at workpool:22acbb0 (queued). `campaign brief` also lists the gate's split job 261121-130515-ab5a4c under A1: I ran the second `campaign verify` with `--for A1`, which tagged both gate checks; its failure is the .md issue in A2, not a test |
| A2 | split_check from cpp-libraries-wt split-check-sources: 261121-130517-25263e at 22acbb0 passed, 0 errors, 0 warnings, 1 note (work_pool.hh 1,039 lines); a local run at 7d93de5 gave 0 errors, 0 warnings, 1 note (work_pool.hh 1,039 lines). The landed tool's `split` check (261121-130515-ab5a4c) reads docs/source-layout.md as source and fails on it, failed 4 errors, 17 warnings, all in the .md, as at d958170 (261121-130433-254720); the lead reruns A2 once split-check-sources lands |
| A3 | `ctest -N`: before 261121-130449-723d31 at workpool:406d2f3 (main), after 261121-130520-f98c9e at 22acbb0, both passed; the `Test #` and `Total Tests` lines diff empty: 6 tests (thread_restart, work_pool, work_group, work_counts, work_scheduling, work_wait) |
| A4 | docs/source-layout.md (workpool:22acbb0) |
| A5 | CI run 44949736609 at 22acbb0 (queued) |
| A6 | selfcontained.sh over the three new headers: 261121-130523-77f795 at 22acbb0 passed, 3 ok, 0 failed; at d958170 (same headers) 261121-130440-45a7f8 passed, 3 ok |
| A7 | the lead's read |

Other jobs: 261121-130434-7c3da2 cancelled (superseded by the head job with the same command;
resubmitted with SIDE=before as 723d31); 261121-130437-47f635 (ctest -N at d958170, 6 tests,
done); 261121-130433-9a29f1 build-test at d958170 cancelled (doc amended).

split_check warnings: none at the head with the split-check-sources tool, so none to answer.

## Result

`work_pool.hh` (1,535 lines) became an umbrella of 1,039 lines that keeps the scheduler
(`WorkClass`, `WorkItem`, `WorkIdle`, `BoxedItem`, `Lane`, `GroupState`, `Frame`, `Here`,
`WorkPoolWorker`, `WorkPool`, `WorkGroup`) and includes three pieces, moved unchanged:

- `work_pool_sync.hh` (200): the per-OS futex block and functions, `cpu_relax`, `ticks`,
  `ticks_per_ns`, `SpinLock` (old lines 54-75, 113-254).
- `work_pool_idle.hh` (180): `Parker`, `Idle`, `backoff` (old 256-394).
- `work_pool_ready_ring.hh` (234): `ReadyRing` (old 472-664).

Layout note: workpool docs/source-layout.md, naming work_pool.hh kept whole at 1,039 lines and
why. No include path, target or test changed.

## Rulings I made

- Flat `work_pool_*.hh` names beside the umbrella, not a `work_pool/` directory: the repo is
  flat and every header sits at its root.
- WorkPool and WorkGroup stay together in work_pool.hh: friends both ways, WorkGroup runs the
  pool's turn/helpable/waiters_due/take_idle/fire_idle on the shared GroupState under its lock,
  and the out-of-line state_of/sleep_until need both classes. The group-state types stay with
  them as their shared data.
- Added lines only: the license comment and a one-line description per piece, `#pragma once`,
  standard includes and `#include "work_pool_sync.hh"`, `namespace workpool_detail` braces,
  `struct GroupState;` forward declaration in the ring piece, and a provenance comment (Tokio,
  Vyukov) per piece; the umbrella's own provenance comment stays.
- `backoff` has no caller in the repo; moved unchanged with Idle (a removal is not a move).
- Out of my file set, for the lead: AGENTS.md's repo map and ARCHITECTURE.md's "references point
  at work_pool.hh" could name the three pieces.
- Open-branch check: of workpool's branches only explore/fairness-load is ahead of main (3
  commits, test/load_fairness.sh only); none edits work_pool.hh.
