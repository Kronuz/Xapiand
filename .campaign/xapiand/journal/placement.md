# placement: journal

State: working (task repair waiting on jobs; see its journal placement--repair.md)
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Tasks `root` (merged at strata:ab0e1beb) and `scan` (merged at strata:2620ba75) are on the quest
branch; `repair` is open on `quest/xapiand/placement--repair` (its journal holds its jobs and
blocks). Resumed 11-21 05h.

- Quest CI at strata:debadea4 (run 44879854555) failed asan and tsan on one test,
  `placement_root_group`: `lone()`'s burst check looped over `namespace_at(...)->record.shards`,
  a member of a destroyed temporary (ASan stack-use-after-scope, TSan heap-use-after-free at
  root_group_test.cc:163). Fixed at strata:d44ae7da (holds the record, fails when absent); the
  proof is CI on d44ae7da against run 44879854555's failure.

## Next

1. Read CI on strata:d44ae7da (asan, tsan green on `placement_root_group`).
2. Merge task repair once its group cases pass at the consensus pin the lead moves (repair's
   Blocked on); then rerun A1-A4 evidence at the quest head (`campaign brief` lists the jobs).
3. Merge main (`campaign land placement --merge-main`, the lead's) and the correctness review.

## Blocked on

- Task repair's group cases on strata's consensus pin (the lead's CMakeLists.txt): see
  placement--repair.md.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | root: 261120-184051-b7da1d (at ab0e1beb; rerun at the head); repair's S: 261121-054430-e8d994 (pending) |
| A2 | scan: 261120-181209-4c19f8 (rerun at the head) |
| A3 | task repair (placement--repair.md) |
| A4 | task repair: 261121-004906-0a1389; fails without 261121-004909-30fad7, 261121-002729-8d49af |
| A5 | `docs/mechanisms/placement-root.md`, `docs/mechanisms/placement-repair.md` |

## Result

Not yet: task repair is open.

## Rulings I made

- The ASan/TSan fix in `root_group_test.cc` (the root task's file, merged) is made on the quest
  branch, not a task branch, since the quest's CI is what it turns red; repair merged it.
- Other CI failures on the repair branch (run 44886359089), outside placement's files and not on
  the quest branch's run: `manifest_log` (asan) and `catalog_stress` (macos-x86_64); reported to
  the lead, not fixed here.
