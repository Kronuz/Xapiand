# group-heal--sim-rows: journal

State: waiting on jobs 261123-052839-09443a 261123-052842-596b03
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
Rows: 261123-052839-09443a (--task sim-rows, --for A5).

## Next

1. Read 09443a and 596b03. faults/membership/growth/device green: submit the four mutants at the
   task head (plain --quest, --fails-without, --for A5). barriers waits on first-commit-crash's
   landing; then merge main and rerun.
2. Put the suite's seconds per row in docs/gates/phase-3r.md and here.
3. A3's simulator half (`simulation_group_corruption`, skipped with 77) is enabled by task
   repair's merge.

## Blocked on

barriers_3/5 wait on quest first-commit-crash landing on strata main. A3's simulator half waits
for task `repair` (on consensus-request-install).

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
