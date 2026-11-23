# group-heal--repair: journal

State: waiting on jobs 261123-045314-e451c4
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

## Doing

First build and run of group_repair at strata:9ba22c18 against consensus:59ae07f (job
261123-045314-e451c4, `--ref consensus=/Users/kronuz/code/Kronuz/consensus@59ae07f`,
`CONSENSUS_DIR=$JOB_REF_CONSENSUS sh test/group/repair_suite.sh build-repair "^group_repair$"`).
A red build or case is fixed next commit.

## Next

1. Green group_repair; then the whole group suite (`repair_suite.sh build-repair`, `^group_`)
   for install/voters/witness/footprint regressions from group.hh and install.hh.
2. Mutants as plain `--quest group-heal --fails-without` jobs (no `--task`), one per name in
   `test/group/repair_mutants.py --list`: `sh -c 'CONSENSUS_DIR=$JOB_REF_CONSENSUS python3
   test/group/repair_mutants.py build-repair-mut NAME'`.
3. Push the branch once the round is green.

## Blocked on

Nothing. The lead unblocked the task (file set widened to option (a), design approved).

## Design (approved by the lead)

Damage (strata `corrupt_index`/`corrupt_segment`, the set's `file_set_damage`) at open, recovery,
apply or a read writes the durable marker `rd` first, quarantines a damaged index for a deferred
one, fences the group locally (reads, proposals, checkpoints answer `replica_damaged`; delivered
ranges acknowledged unapplied; no pin or serve), and calls `Driver::reinstall` on the loop, again
each election timeout while damaged; a damaged leader hands off first. The image stages and
activates through install's path; activation now checks every staged file's CRC32C, and a
complete activation removes `rd` before its sweep. Files: include/strata/group/{repair.hh,
group.hh,install.hh}, test/group/repair_{test.cc,suite.sh,mutants.py}, CMakeLists.txt (target),
docs/mechanisms/group-install.md (Repair).

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | pending: 261123-045314-e451c4 (first build) |

## Result

Not yet.

## Rulings I made

- Unfencing happens when the activation's recovery from the bundle succeeds (`adopt`), which is
  the moment consensus completes the call with RepairCompleted; an unrequested image that
  activates (a lagging replica) repairs too. The marker is the damage, not the call.
- A staged file that fails its check at activation marks the replica damaged and still answers
  `activated`: `activation_failed` fences consensus's Core for good, while consensus holding the
  image as its base is harmless (the replica serves nothing and asks again).
- A live set file has no CRC32C in the bundle, so its damage at rest is not detected (F24, no
  scrubber); the staged check covers live files through the pin's CRC32C.
- The repair retry shares the stall hand-off's timer (one wheel entry per group).
- The test's `read` and `apply` cases inject strata's `corrupt_index` through the read function
  and the app's `before_write` hook: deterministic, and the same error strata raises.
