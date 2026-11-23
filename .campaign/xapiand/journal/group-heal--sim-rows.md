# group-heal--sim-rows: journal

State: waiting on jobs 261123-053444-c51484 261123-053447-2bfa62 261123-053450-58e6b8 (not pushed: GitHub unreachable; strata:18525c5f and the journal are committed locally)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 2 (2026-11-23) at strata:bd994276. Round 1's suite 261123-045257-ebd91a failed 7 of 9:
- barriers_3/5: `corrupt_index: no manifest log`, strata main's first-commit crash (quest
  first-commit-crash). Probe 261123-052842-596b03: barriers rows on a scratch merge of
  quest/xapiand/first-commit-crash (strata:b77109dc, ref refs/scratch/group-heal--sim-rows-fcc,
  not on the task branch).
- membership_3/5, growth_1: row bugs. A member the core removes (replacement's `leaving`) or a
  removal removes never learns that commit, so settling before retiring it never ends (voters_test
  retires first). growth also promoted a third data voter beside a witness without `leaving`:
  consensus's validate_membership refuses 3 + 1 outside D9's window. Fixed: promote beside the
  leaving member, removed_by_core, retire, settle.
- faults_3/5: row bug. A cut upload answered in the drain isolated its leader and only a faults
  step expired isolation, so that replica stayed a term and 4 entries behind (new term's no-op,
  chunks 1-2, final). Also poll_upload read the answer after resetting its started<> (dangling):
  no upload was ever counted. Fixed: isolation expires in every step, finish heals after drain,
  the answer is held.
- mutants: FileNotFoundError on build-heal/strata/group_sim_test. Fixed in sim/CMakeLists.txt:
  group_sim_test's RUNTIME_OUTPUT_DIRECTORY is the build root, where mutants.py runs targets.
Round 2 jobs: rows 261123-052839-09443a at bd994276: faults_3/5, membership_5, growth_1,
device_3 pass; barriers abort as before; membership_3: `no node applied index 1` (the oracle's
merged_log reads live members only, and the row retires every founder). Probe 596b03 (barriers on
the first-commit-crash merge): both pass, so barriers are first-commit-crash and nothing else.

Round 3 at strata:18525c5f: the driver keeps retired members' records in the oracle's log
(checked for agreement). Jobs: rows c51484 (--task); mutants (plain --quest, --fails-without,
--for A5) apply-skips-entry 5df6d3, read-skips-probe 7d40db, apply-abandoned-chunk f9a12e, all on
`faults 3`; probes on the first-commit-crash merge strata:91dd7b54: all rows 2bfa62,
recovery-as-found on `barriers 3` 58e6b8. Mutants done, each a complete build and named
`faults 3` failures: apply-skips-entry 5df6d3 (`no node applied index 12`, accepted put without a
version), read-skips-probe 7d40db (histories not linearizable, 5 seeds), apply-abandoned-chunk
f9a12e (`upload N cut by a leader change writes nothing`, 5 uploads). (A plain probe with the rows' exact command superseded
the task's 59a63e: probes now carry a distinct regex argument.)

## Next

1. Read the round 3 jobs: each mutant's log must show a complete build and a named row failing.
   barriers and recovery-as-found wait on first-commit-crash's landing: then merge main, rerun the
   rows and that mutant at the head.
2. Put the suite's seconds per row in docs/gates/phase-3r.md and here.
3. A3's simulator half (`simulation_group_corruption`, skipped with 77) is enabled by task
   repair's merge.

## Blocked on

barriers_3/5 wait on quest first-commit-crash landing on strata main. A3's simulator half waits
for task `repair` (on consensus-request-install).

## Evidence

| Criterion | Evidence |
|---|---|
| A5 | pending: 261123-053444-c51484 (barriers wait on first-commit-crash); fails without: 261123-053422-5df6d3, 261123-053425-7d40db, 261123-053427-f9a12e; recovery-as-found probe 261123-053450-58e6b8 (scratch merge) |

## Result

Pending the jobs. Files (strata): `test/group/sim_test.cc` (rows), `test/group/sim_suite.sh`,
`test/group/sim_mutants.py`, `sim/CMakeLists.txt` (group_sim_test, ctest `simulation_group_*`),
`docs/gates/phase-3r.md` (stub: rows, mutants; the gate rows are the lead's).

## Rulings I made

- Round 2-3: the failing membership, growth and faults rows were driver bugs, not group liveness
  defects (each traced above); no budget was raised.
- The oracle's log includes retired members' records (sim_test.cc `merged`), since world.hh's
  merged_log is outside this task's files and reads live members only.
- group_sim_test's RUNTIME_OUTPUT_DIRECTORY is the build root, so mutants.py finds it unchanged.

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
