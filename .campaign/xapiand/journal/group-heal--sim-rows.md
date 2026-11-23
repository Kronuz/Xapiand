# group-heal--sim-rows: journal

State: waiting on jobs 261123-045257-ebd91a 261123-045305-643f0d 261123-045307-7047cc 261123-045310-66fba2 261123-045313-6aa91f
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 (2026-11-23) at strata:5902da15, pushed. Nothing built locally: the jobs are the first
compile. Plain rows: 261123-045257-ebd91a (`sh test/group/sim_suite.sh build-heal`, --task
sim-rows, --for A5; its ctest lines give each row's seconds: the suite cost). Mutants (plain
--quest, --commit 5902da15, --fails-without, --for A5): apply-skips-entry `faults 3` 643f0d,
recovery-as-found `barriers 3` 7047cc, read-skips-probe `faults 3` 66fba2, apply-abandoned-chunk
`faults 3` 6aa91f.

## Next

1. Read the jobs (full logs ~/.scratch/campaign/jobs/<id>/a1/log). A compile error or a row that
   fails without a mutant: fix, commit, resubmit the plain job and the mutants at the new commit.
2. Rows most likely to need a second round: `growth_1` (consensus may refuse a 1-to-2 voter
   promotion or a direct removal; D7's path) and `device_3` (whether 16 seeds give a non-prefix
   log, and whether the journal reopens on it); `barriers_*` cost (one run per crash point of the
   window: cap if the window is large).
3. Put the suite's seconds per row in docs/gates/phase-3r.md and here.
4. A3's simulator half (`simulation_group_corruption`, skipped with 77) is enabled by task
   repair's merge: damaged segment, manifest and file-set files fence and reinstall.

## Blocked on

Nothing for A5. A3's simulator half waits for task `repair` (on consensus-request-install).

## Evidence

| Criterion | Evidence |
|---|---|
| A5 | pending: 261123-045257-ebd91a; fails without: 261123-045305-643f0d, 261123-045307-7047cc, 261123-045310-66fba2, 261123-045313-6aa91f |

## Result

Pending the jobs. Files (strata): `test/group/sim_test.cc` (rows), `test/group/sim_suite.sh`,
`test/group/sim_mutants.py`, `sim/CMakeLists.txt` (group_sim_test, ctest `simulation_group_*`),
`docs/gates/phase-3r.md` (stub: rows, mutants; the gate rows are the lead's).

## Rulings I made

- The target lives in `sim/CMakeLists.txt` under `if(TARGET strata::group)` (the old group-sim
  decision: the simulator's tests are built by sim/; root CMakeLists.txt is not in my file set).
  Tests are named `simulation_group_*` so install_suite.sh's `^group_` (which builds a fixed
  target list) never selects them.
- `prefix` reads are checked outside the linearizability search, against a replay timeline (the
  key's state after some prefix at or below the replica's applied index, monotone per process),
  so they never branch the search (SIM-R20).
- Write outcomes: any error is `unknown` (sound); a read with no answer is unrecorded; what does
  not answer within 3,000 events of a drain is abandoned the same way.
- Uploads (D12): an upload's final writes only when its chunks and final share a term. A cut
  upload's final writing anything fails the row by value, the mutant apply-abandoned-chunk
  (blob_app.hh: an upload outlives its term) is killed there; the register history cannot see it,
  since every acknowledged chunk is in the staging file.
- Mutant "a recovery that ignores the checkpoint's file-set lengths" is recovery-as-found: no
  truncation, no deletion, every file under the prefix live at its disk size. Truncating lengths
  alone is equivalent for this application by construction: effects are positional and never
  shrink a file, and every byte past P_b's length belongs to a committed entry the replay rewrites
  identically, so the end state cannot differ. The names half is observable: a staging file
  retired at or below a published bundle whose unlink did not survive a power cut stays live.
- apply-skips-entry skips index 12 (a command in every faults run): marked applied, answered,
  never run.
- A3's simulator half is the named, skipped row `simulation_group_corruption` (exit 77).
- The non-prefix device is shown on the log device (journal checksums); the data device's
  unsynced bytes are discarded by recovery's truncation and the replay, and synced-byte damage is
  A3's corruption row.
