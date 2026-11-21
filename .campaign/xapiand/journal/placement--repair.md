# placement--repair: journal

State: waiting on jobs 261121-004526-522f59 261121-004906-0a1389 261121-004909-30fad7 261121-002719-b7c57d 261121-002720-cd7f2d 261121-002752-d9cb1d 261121-002723-86ef33 261121-002724-b25808 261121-002725-07f209 261121-002726-d6fee4 261121-002727-19a20a 261121-002729-8d49af
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Second round at strata `786d1e66` (code as of `c5125902`). Job 261121-000231-24e66d (990d5e80)
compiled; the plan test failed 43 property rounds, all "it waits only with no free node" at rf 1
from 2 + witness with a lost data voter; the group test failed every case on 3 seeds.

- Plan, fixed (`f2e10fea`): the wrong model was that a lost member leaves only by removal and a
  replacement only grows toward the target. Under D9, 2 + witness can drop neither a lost data
  voter (1 voter + witness is no shape) nor the witness first (one up voter of two, no quorum), so
  at rf 1 nothing was valid. Now a lost member no removal drops leaves beside a promoted learner
  whatever the target; the learner joins only when that promotion would be valid (checked on the
  membership with it added), so none joins to be removed. Value case `shrink_past_lost`, mutant
  `no-window-replace`.
- Group test, seed 1 each at strata `c40ca8aa`: `replace` 261121-003512-50dacf, `replicas`
  261121-003514-940aa0, `waits` 261121-003517-dd0df9, `drain` 261121-003522-a43d42 all stop at
  one consensus bug (Blocked on): the leader fences on D3's removal of the leaving member, in
  the root (replace, waits) and in shards (replicas' leaving witness, drain's leaving leader).
  `drain` also failed by construction before (idx is placed on the root's nodes 1-3): it now
  drains a node off the root that leads a shard. `defer` chose incarnation 0 for its hand
  transfer: a range-for over `state()->membership.members` read a destroyed temporary (`state()`
  returns an optional by value); fixed in `30c3d26b`, `defer` seed 1 passes (261121-004525-2c425b).

## Next

1. Read 261121-004526-522f59 (all group cases, seed 1, on the consensus probe): what fails past
   the removal bug. Fix what is repair's.
2. Evidence: plan 261121-002711-934d7a passed (A3); plan mutants `--fails-without` queued:
   261121-002719-b7c57d setting-ignored, 261121-002720-cd7f2d drain-ignored, 261121-002752-d9cb1d
   no-leaving, 261121-002723-86ef33 no-quorum-guard, 261121-002724-b25808 record-ignored,
   261121-002725-07f209 leader-not-first, 261121-002726-d6fee4 shrink-order, 261121-002727-19a20a
   no-window-replace (A3), 261121-002729-8d49af transfer-anyway-plan (A4); group `defer` 3 seeds
   261121-004906-0a1389 (A4), mutant transfer-anyway 261121-004909-30fad7 (A4).
3. Once consensus funds the removal (Blocked on): group `replace`, `drain`, `replicas`, `waits`
   with `--ref` at its fix, `--for A3`, and mutant `replace-now`; then rerun all at the head.
4. Integrate against group-heal with `--ref` once its install binds.

## Blocked on

- **consensus (Detent's library; lead to route), blocks A3's group cases.** Job
  261121-003512-50dacf (`replace`, seed 1): the root leader fences with "persistence lacks
  preowned reservation" on committing the promotion beside the lost voter, when its drive
  proposes D3's removal of the leaving member. Cause, consensus `b8de3b8` (also `main` bb961b3):
  `Worker::submit_retained` admits a leader's `Tick` with no append reservation
  (`if (leader && Tick) plan.count = 0`), but `Core::drive` on that Tick calls
  `propose_membership` for `committed.leaving` and emits a `Persist`; `ingest` throws. Every
  replacement window through the Driver hits it: `replace`, `waits` (the root's), `drain`,
  `replicas` at rf 2 to 3 (the leaving witness). Options: (a) the worker funds one
  configuration append on a leader Tick when a removal is due (probe below); (b) the core
  proposes the removal only on an admitted event that carries a reservation; (c) repair avoids
  `leaving` (not possible for 2 + witness, D9's windows need it). Recommend (a), sized by the
  membership's encoded bound, with a Driver-level replacement test in consensus. Probe: local
  branch consensus `probe/xapiand/repair-leaving-tick` (fd8d977, worktree
  ~/.scratch/xapiand/placement--repair/consensus-probe, not pushed); job 261121-004526-522f59 runs
  the group cases on it.
- group-heal (its install binding, itself on Detent's `consensus-driver-install`). These A3 steps
  wait on it; every other step is tested here by log catch-up (no checkpoint truncates a log in
  the tests):
  1. Every learner add (replace, drain, rf growth, window replacement) on a group whose log a
     checkpoint truncated: the learner needs install to catch up, else consensus answers the
     promotion `NotReady` and the plan retries it.
  2. A new root voter installing the root's records (the quest's outcome sentence): the group
     test's `replace` case adds one by log catch-up only.
- journal-group-delete (new quest): a left group cannot be rejoined yet. Production has one
  journal host per node; the test world gives each replica its own host, which steps around it.
  The cases that need it on a per-node host: `replicas` (rf 3 to 2 adds the witness's learner on
  node 3, which left the group at rf 2 to 3: plan trail `5:3:L`), `replace` and `drain` whenever
  the candidate is a node that held an earlier member of the group, and `waits` is unaffected (the
  same incarnation returns). When it lands, the world moves to one host per node and these run
  unchanged. Until then a binding must not place a learner on a node holding the group's old
  journal (the plan has no such input; the cluster quest's binding owns it).
- For the lead: `docs/mechanisms/placement-root.md` is outside this task's files, so its key table
  lacks the settings record; the row to add: `| S | the cluster settings | replace_after in ms
  (repair's, placement-repair.md); put, never erased |`. The root task's fuzz corpus does not
  cover `S` yet (`root_fuzz.cc`). The settings record stays in `records.hh` (lead's ruling).

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | plan 261121-002711-934d7a passed (3,000 seeds); plan mutants pending (Next 2); group cases blocked on consensus |
| A4 | `defer` seed 1 passed 261121-004525-2c425b; pending: `defer` 3 seeds 261121-004906-0a1389, mutants 261121-004909-30fad7, 261121-002729-8d49af |
| A5 | `docs/mechanisms/placement-repair.md` (repair's half) |

## Result

Not yet: see Doing.

Built (strata quest/xapiand/placement--repair): `include/strata/placement/repair.hh` (the plan,
the root's operations `set_replace_after`, `set_rf`, `drain`, `drained`, `record_shard`),
`include/strata/placement/records.hh` (settings record `S`), `test/placement/repair_plan_test.cc`,
`test/placement/repair_world.hh`, `test/placement/repair_group_test.cc`,
`test/placement/repair_mutants.py`, `test/placement/repair.cmake`,
`docs/mechanisms/placement-repair.md`.

## Rulings I made

- The settings record lives in the root as key `S` (see Blocked on): a format byte and LEB128
  milliseconds; absent means the 300 s default; a command may put it, never erase it.
- Repair plans, it does not drive: `plan()` is pure and level-triggered (one step per group per
  pass, from what the group is), so a lost or refused step is planned again. The binding (the
  cluster quest's) runs `change_membership`/`transfer_leadership` at the group's leader.
- The record follows the group: when they differ the first step records the group as it is;
  the planner never writes intent ahead of consensus.
- The shape is placement's (`placement.hh`): `min(rf, usable)` data voters, 2 take a witness on a
  third node, else voter + learner. Usable means not lost (lost: down for replace_after, or state
  not active).
- Every planned change passes consensus's `validate_change` and keeps a quorum of up voters;
  where none does, the shard waits ("no node or no valid change reaches rf yet").
- Leader balance: the preferred leader moves only to a healthy voter with two fewer leaderships,
  and a pass counts each record's load change before the next shard, so it never flaps.
- Every transfer the plan makes waits while the leader reports an open multi-entry command,
  including the one before retiring the current leader (drain), not only balance.
- The root group (consensus id 1000 in tests) is planned with no record and no balance.
- The `report`'s `open_command` comes from the application (Xapiand's open chunked commands; the
  blob test application's open uploads), not from strata::group, which holds no such state.
- A lost member that no direct removal drops is replaced through a window (promoted learner
  beside it) whatever the target rf; a learner joins for it only when that promotion is valid.
- The settings record's bytes are described in `placement-repair.md`, not the root's note (out of
  this task's files); the drain case drains a shard leader off the root's nodes.
- msg-1 (scan's, on the quest) is the lead's: nothing in this task changes for it.
