# placement--repair: journal

State: waiting on jobs 261121-054406-7d4001 261121-054409-747579 261121-054411-7b5102 261121-054413-b9e484 261121-054427-5c7193 261121-054430-e8d994 261121-054432-66bbee
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Third round (resumed 11-21 05h) at strata `e45b1d5d` (task head `d30901ae` merges the quest's
`d44ae7da`, a test-only fix). The second round's jobs all finished as expected: plan
261121-002711-934d7a passed; the ten plan mutants and `transfer-anyway` fail their checks; `defer`
3 seeds 261121-004906-0a1389 passed.

- Detent's probe 261121-012106-ce07f5 ran this test's five cases (seed 1) at consensus `20198fc`
  (the pin `b8de3b8` plus the leaving-removal fix `9a2476d`, branch
  `probe/leaving-removal-on-b8de3b8`) and every case passed. Their quest branch builds on consensus
  `main`, whose `Driver::recover` lacks the `closed` overload strata's pin has, so it does not
  build here (261121-011658-34495c): the pin must move to a commit with both.
- Queued at `e45b1d5d`, `--ref CONSENSUS=consensus@20198fc`, `--for A3`: `replace`
  261121-054406-7d4001, `replicas` 261121-054409-747579, `waits` 261121-054411-7b5102, `drain`
  261121-054413-b9e484 (3 seeds each); mutant `replace-now` `--fails-without` 261121-054427-5c7193.
- The settings record `S` is in the root's fuzz target (selector 6) and corpus (`e45b1d5d`):
  root tests and replay 261121-054430-e8d994 (A1), libFuzzer 5 min 261121-054432-66bbee.

## Next

1. Read the seven jobs; record them in Evidence.
2. When consensus has the fix on a commit that also carries `b8de3b8`'s reopen (Detent's
   consensus-leaving-removal after consensus-quiescence), the lead repins strata's consensus
   (CMakeLists.txt is the lead's) and the group cases and `replace-now` rerun at the quest head
   with no `--ref`. Until then `placement_repair_group` fails in CI on every platform (run
   44886359089: replace's fence), so the task cannot merge green.
3. Integrate against group-heal with `--ref` once its install binds.

## Blocked on

- **consensus pin (lead), blocks A3's group cases at the head.** The fence on D3's removal of a
  leaving member (consensus `b8de3b8`: a leader's unreserved `Tick`, and the `Persisted` after the
  commit, proposed the removal) is fixed by Detent's consensus-leaving-removal (option b,
  `9a2476d`, a Driver-level replacement test). It is on consensus `main`'s line, which lacks
  `b8de3b8`'s reopen; `20198fc` is the fix on `b8de3b8` and passes this test (Detent's
  261121-012106-ce07f5). Strata's consensus pin (CMakeLists.txt, the lead's) must move to a commit
  holding both before `placement_repair_group` passes in CI.
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
- Done for the lead's notes: placement-root.md lists `S` (strata:51bb7466); the fuzz target and
  corpus cover it (strata:e45b1d5d).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | root tests and corpus replay with `S` 261121-054430-e8d994 (pending); libFuzzer 261121-054432-66bbee (pending) |
| A3 | plan 261121-002711-934d7a passed (3,000 seeds); mutants fail: setting-ignored 261121-002719-b7c57d, drain-ignored 261121-002720-cd7f2d, no-leaving 261121-002752-d9cb1d, no-quorum-guard 261121-002723-86ef33, record-ignored 261121-002724-b25808, leader-not-first 261121-002725-07f209, shrink-order 261121-002726-d6fee4, no-window-replace 261121-002727-19a20a; group cases on consensus 20198fc pending (Doing) |
| A4 | `defer` 3 seeds 261121-004906-0a1389 passed; mutants fail: transfer-anyway 261121-004909-30fad7, transfer-anyway-plan 261121-002729-8d49af |
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
- The fuzz target picks the settings decoder on selector 6 (`data[0] % 7`), keeping 0-5 as they
  were so the corpus files keep their decoders; `root_fuzz.cc` is the root task's file, done here
  as the lead's 11-21 Next item.
