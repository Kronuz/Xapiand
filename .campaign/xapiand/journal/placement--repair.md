# placement--repair: journal

State: waiting on jobs 261121-000231-24e66d
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First build of the plan and group tests (job 261121-000231-24e66d, strata
quest/xapiand/placement--repair at 990d5e80; CI also runs on the pushed branch). Nothing has
compiled yet: fix what the build reports, then the tests.

## Next

1. Green `placement_repair_plan` and `placement_repair_group` (jobs `--for A3,A4`: plan test,
   group test per case).
2. Mutant jobs, one per mechanism, `--fails-without`: `python3 test/placement/repair_mutants.py build NAME`
   (transfer-anyway for A4; replace-now, setting-ignored, drain-ignored, no-leaving, no-quorum-guard,
   record-ignored, leader-not-first, shrink-order, transfer-anyway-plan for A3).
3. Integrate against group-heal with `--ref` once its install binds (see Blocked on).

## Blocked on

- Lead: commit strata `8f252987` edits `include/strata/placement/records.hh` (the root task's
  file, inside the quest's set) to add the settings record `S` (`replace_after_ms`, default 300 s,
  put but never erased) that the quest's outcome needs ("held in the root, changed at runtime").
  It also adds one row to `docs/mechanisms/placement-root.md`. Keep it, or move it and say where.
  The root task's fuzz corpus does not cover the new record yet (root's `root_fuzz.cc`).
- group-heal / Detent: A3's learners here catch up from the log (no checkpoint truncates it in
  the tests). These wait on group-heal's install binding (itself on Detent's
  `consensus-driver-install`): a learner added after a checkpoint truncated the log (every A3 step
  once logs are compacted), and "a new root voter installs the root's records" by install rather
  than by log catch-up. The group test's `replace` case already adds a new root voter by log.
- Finding for the lead (cluster quest, not repair's): a node that left a group cannot host a new
  incarnation of it on the same journal host, because `journal::Host` has no way to delete a
  group's journal; the test world gives each replica its own host to step around it.

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | pending: job 261121-000231-24e66d (build), then plan, group and mutant jobs |
| A4 | pending: group test `defer` and mutant `transfer-anyway` |
| A5 | `docs/mechanisms/placement-repair.md` (repair's half) |

## Result

Not yet: see Doing.

Built (strata quest/xapiand/placement--repair): `include/strata/placement/repair.hh` (the plan,
the root's operations `set_replace_after`, `set_rf`, `drain`, `drained`, `record_shard`),
`test/placement/repair_plan_test.cc`, `test/placement/repair_world.hh`,
`test/placement/repair_group_test.cc`, `test/placement/repair_mutants.py`, `test/placement/repair.cmake`,
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
