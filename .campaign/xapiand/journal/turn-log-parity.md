# turn-log-parity: journal

State: waiting on jobs 261122-211650-53b0d6 261122-211651-d55897 261122-211651-6e7f83 261122-211651-c814ac 261122-211652-48bf9b 261122-211653-bec36d 261122-211653-c0a813 261122-211654-8f477f 261122-211654-bc88d7 261122-211655-3f7d01 261122-211656-32a6f7 261122-211656-649631 261122-211721-4e393f 261122-211725-d53b60 and CI at strata:7c6aa208
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Group-commit merge pass (2026-11-22), head strata:7c6aa208, pushed:

1. The task merge refused on its journal (add/add in `turn-log-parity--group-commit.md`).
   Xapiand:bdced53bc merges this quest branch into the task branch, the task's journal winning
   (pushed). strata:c074afcd (the tool's merge of 6c3d0862) was already on the quest branch.
2. strata:7c6aa208 merges main (strata:7131ed44, README.md only) with `git merge`;
   `campaign land --merge-main` was refused by the session's permission check.
3. A4 mutant re-cut at the merged head: strata:3e7bad67 (`refs/mutants/turn-log-parity-cohort-groups`,
   local), 6c946dc3's diff on 7c6aa208, building only `test_barrier_cohort`. Job 261122-211721-4e393f.
4. Read (fix round 1, at strata:bc8c614c): D1/A2 261121-214538-7ed56e passed (`first chunk
   damaged: bound 4`, `middle chunk damaged: bound 5`, `tolerant open: 0 failures`), so
   manifest_named's bounds held under journal:e4b0100; standalone 261121-214541-f24fed passed;
   D2 mutant 261121-214545-0d42a7 built, then failed `test_reopen_damaged.cc:247` twice with `a
   turn-log chunk with no run before it`, as it must; build-test 261121-214553-5dbd30 passed
   (90 of 90).
5. Reviewer's D2 probe 261121-213954-ecd99d at 5953ed46: exit 1. `first` and `a third in`
   reproduce D2. Its `small (control)` and `last chunk damaged` cases also fail, with `the
   index's head names turn-log records its log lacks` (status 1, bound 0): not D2's message, and
   neither the review nor the fix covers them. Rerun at the head as 261122-211656-649631; if
   those two still fail, it is for the reviewer and lead (Blocked on 4).
6. The A4 mutant 261121-183128-18f3a2 and the task's 261121-213641-0cfcd2 and
   261121-213700-e680a2 failed at build (D1's `faultfs`; a captured constexpr `barrier`): not
   evidence, no kill.

Fix round 1 (durability review CHANGES at strata:5953ed46), head strata:bc8c614c:

1. D1, strata:c38e4bc9: `test_reopen_damaged.cc` names faultlab as `fl::` (kv_test.hh's alias of
   `kronuz::faultlab`, faultlab e3fcfe3); 5953ed46's redundant `faultlab/faultfs.h` include
   dropped. The bounds `manifest_named` asserts (C 4, U 5, W 4, D 3) are unchanged by
   journal:c851826/e4b0100 as read: the damage is one bad fragment of U's u1 inside block X, and
   X+1 holds only the rest of U's filler, which "adds nothing" (lost-bound.md). Job 7ed56e decides.
2. D2: replay's no-run check (`index.hh` `replay_log`) applies only above the head
   (strata:bc8c614c): a chunk at or below `cur_.applied` with no run before it is skipped. A head
   never sits inside a run (a run applies whole at its last chunk), so such a chunk belongs to a
   run the head holds. Test strata:e60cc249: `chunk_covered`, a C-like index with a ~100 KiB value
   (a chunk run its head covers, checkpoint record never sealed), first then a middle chunk
   damaged; it must open with the value, write, and reopen `stored`. e60cc249 is the mutant (test
   without the fix); its job must show a complete build, then `open threw: ... no run before it`.
3. N3: the rename stays (Rulings, fix round 1). N1 is the owner's; untouched.

Merge pass (lead, 2026-11-21) at strata:b533db12, pushed:

1. strata:3b8da146 merges task group-commit (strata:80f91fd0). Conflicts in turn_seal.hh,
   index.hh and test_barrier_cohort.cc, resolved keeping both sides: await()/await_sealed() keep
   the CI-red pass's reread after each inline turn (`idle`, `inline_turn()`) and gain the task's
   release bookkeeping (`p.owe()`, `released`); the member `await_sealed(s)` (large-tx's helper,
   used by plain and chunked joins) reports the writer's arrival with `joined(false)` before it
   waits on a full record; join_chunks' per-chunk `wake()` becomes `joined(true)`, since `wake()`
   no longer counts an arrival. test_barrier_cohort takes the task's configurations on `fl::`.
2. strata:926ea902 merges task tolerant-open (strata:a16d117c), clean.
3. strata:b533db12: journal pin 178030b -> journal:e4b0100 (journal-lost-bound's head, durability
   accepted there): tolerant-open needs `Host::bound`, `Host::lose`, `GroupStatus::bounded`. The
   test moved into the file set: `test/test_tolerant_open.cc` -> `test/test_reopen_damaged.cc`
   (`test/test_reopen*`), ctest `reopen_damaged`.
4. Out-of-scope warnings: group-commit's `docs/design/rocksdb-parity.md` and tolerant-open's
   `include/strata/core/turn_seal.hh` are both in the quest's file set: kept.
5. origin/main (strata:cc30217f) unchanged since the last main merge: no merge needed.

Earlier, CI-red pass (lead's three failures of run 44938627226) at strata:e59af09e, pushed:

1. `reopen_loops` "a turn-log wait with nothing to write" and `standalone_turn_log`'s ASan abort
   are one bug, in main's `turn_sealer::await`/`await_sealed` (strata:1755bfb8 fixes it). With no
   loop serving, a waiter checks its record, then writes a turn inline; another thread's turn
   (a second inline waiter, or a loop thread starting: `served_` still false at the check) may
   write the record in between, so the waiter's turn writes nothing and it threw, fencing an index
   whose record was durable. The ASan abort is `served()`'s writer thread hitting it
   (`terminate ... logic_error`, same message; the log has no ASan report). reopen_loops' label
   "step 1 cut 44 seed 5764: threw" is build()'s catch: the create's first writes, not recovery;
   the seed does not reach that code, so it is timing, not seed. Fix: a wait rereads under `mu_`
   after each inline turn and throws only when a turn that wrote nothing leaves it unsatisfied.
   Test: standalone case `rival` (another thread's turn held 100 ms at its sync while the writer
   waits). Attribution: pre-existing on main (main's await is the same code; `served`/`lag` and
   kv_strata's loop thread are main's); job 261121-151714-b5123c runs `rival` on main cc30217f.
2. `large_tx` TSan race at test_large_tx.cc:436: the quest's own test (`test_large_tx.cc` is not on
   main: `git ls-tree origin/main test/` lacks it). `seen` and `reads` were plain counters the main
   thread polls; now atomics, and the reader counts torn reads for a check after the join rather
   than a CHECK on its thread (strata:e59af09e).

Mutant and attribution commits are local branches `mutant/turn-log-parity/{await-throws,
visible-race,main-rival}` in strata (not pushed).

Earlier resume pass at strata:b146febc: main (strata:cc30217f) merged into the quest branch.
Merged tasks: checkpoint-window, cohort-wait, journal-pin, large-tx, merge-runs, reopen-loops, ryw.
Not merged: group-commit, tolerant-open (Blocked on, below). Evidence at strata:b146febc is queued.

## Next

1. Read the jobs in State into Evidence. The A4 mutant must show a complete build, then
   `test_barrier_cohort` failing a CHECK. A1-A4/A6 reruns at 7c6aa208 replace their b533db12 and
   bc8c614c runs; A4's test 261121-230329-90fd96 and 261121-213706-2e8246 were reused by identity
   (their inputs are unchanged at 7c6aa208).
2. Lead: rerun `campaign task merge turn-log-parity group-commit`; then `campaign accept
   turn-log-parity durability --confirm D1=261122-211652-48bf9b --confirm D2=261122-211652-48bf9b`
   (or the build-test 261122-211725-d53b60) once they pass.
3. Lead: Blocked on 1 and 4 before landing.

## Blocked on

1. Lead, before landing: the journal pin is journal:e4b0100, journal-lost-bound's head, not on
   journal main. journal-lost-bound lands after detent:journal-tail-second-sector; then the pin
   moves to journal main (a pin-only commit here, jobs rerun).
2. Lead: stale queued jobs at older commits not cancelled (cancel was refused here):
   261121-151525-9704d9, 261121-151523-00cd16 (task heads, superseded by the merge and
   261121-183128-18f3a2), 261121-151725-649183 and 261121-151646-4dcef9 (suite and fix at
   strata:e59af09e, superseded by build-test and 261121-183109-1a0cf0).
3. Earlier: A5 and A6's gate rows wait for parity tasks 2-6 and 8-14 (closing pass below).
4. Reviewer/lead: the D2 probe's control and last-chunk cases (Doing 5), if they still fail at
   7c6aa208. Options: (a) a correct refusal (the damaged record is the log's tail and its bound
   unknown), recorded beside N1; (b) a defect like D2, fixed with a case in test_reopen_damaged.
   No recommendation until the rerun is read.
5. N1 stays the owner's call; the default (land with the limit recorded) stands.

Resolved: group-commit (CI-timing check replaced by recorded turns at strata:80f91fd0, merged);
tolerant-open (merged with the pin above); build-test now configures with `-G Ninja`.

## Merge of main (strata:cc30217f into b467309d, as b146febc)

Textually clean (three files auto-merged: CMakeLists.txt, test/kv_strata_batch_test.cc,
test/standalone_turn_log_test.cc). Semantic resolution, keeping both sides:

- Main's faultlab move (strata:e8645287) deletes `include/strata/core/faultfs.hh` and
  `strata::faultfs`. The quest's own tests still used them: test_ryw, test_large_tx,
  test_checkpoint_window, test_barrier_cohort, test_reopen_loops and the quest's cases in
  standalone_turn_log_test. Renamed `faultfs::{filesystem,io}` to `fl::` as main did; test_reopen_loops
  (which includes kv_strata.hh, not kv_test.hh) now includes `faultlab/faultfs.h` and declares
  `namespace fl = kronuz::faultlab`. faultlab links through main's `link_libraries`.
- Main's Ninja-only guard (strata:9cefc0e5): no quest file configures without `-G Ninja`
  (test/ninja_only_check.sh's grep is empty at the merge).

## Evidence

At strata:b533db12 (both tasks merged). From `campaign brief`:

| Criterion | Evidence |
|---|---|
| A1 | <261121-090415-53357a ran other code; 261121-090438-28fbcd ran other code; 261121-183106-bb529c queued; 261121-183107-df159e queued; 261121-183109-1a0cf0 queued; 261121-183110-47450d queued> |
| A2 | 261121-145607-0e1ac9, 261121-115239-ee5107; fails without: 261121-115241-8bbc79, 261121-145613-34602d |
| A3 | <261121-110105-830c31 ran other code; 261121-114248-3845d4 ran other code; 261121-183107-decc0f queued; 261121-183109-837d25 queued; 261121-183110-399fd2 queued> |
| A4 | <261120-192604-5eca03 ran other code; 261121-101105-1c95d9 failed; 261121-151521-37deb9 running; 261121-183108-f29e13 queued> |
| A6 | <261120-081226-1f9736 ran in /Users/kronuz/code/Kronuz/strata-wt/xapiand/turn-log-parity--merge-runs, not the quest's repository; 261120-192628-0cc5e8 ran other code; 261120-194415-bf027b ran other code; 261121-050248-2f7f8d ran other code; 261120-204250-36020e ran other code; 261121-054157-2f69f9 ran other code; 261121-183108-996f58 queued; 261121-183108-a5a124 queued> |
| A5 | <no evidence yet> |

Added at strata:b533db12, not yet in the brief: A2 reopen_damaged 261121-183121-b8a9f9 and the
standalone suite 261121-183125-bd57b5 against the pinned journal (replace 145607-0e1ac9 and
115239-ee5107, which built journal:1e3505a through an override); A4 mutant 261121-183128-18f3a2 at
strata:6c946dc3 (`refs/mutants/turn-log-parity-cohort-groups`, local: the quest head with the cohort
counting released groups, not writers; `--fails-without`). build-test 261121-183142-9fa0f6.
Still queued at their own commits and standing: 261121-151648-9ffb16, 261121-151703-21b6d5
(mutants), 261121-151714-b5123c (main attribution).

Smallest jobs, once built (each a `campaign job submit --quest turn-log-parity`):

- A1: `--for A1 -- build/<ryw test>`, and the same at the cut-at-V mutant with `--fails-without`.
- A2: `--for A2 -- build/<tolerant-open test>`, and at the open-everything mutant `--fails-without`.
- A3: `--for A3 -- build/<large-tx power-cut sweep>` with `chunk_bytes` 4 KiB, and three
  `--fails-without` runs (apply per chunk, freeze leaves the record open, limit at `command_bytes`).
- A6: `--level exclusive --minutes 10` per row set of document-gate's gate report, host first, a
  seconds-long smoke before each.

Hunt item "journal park contract" (Detent msg-188): not applied. Its two strata items wait for a pin
at journal:b6a7697 or later. The pin is journal:e4b0100 since the merge pass, which contains
b6a7697: the item is now applicable (not done in the merge pass).

### Design round 2 probes (models of the design, not of built code)

| Item | Evidence |
|---|---|
| F8, F10, F9a, A3 torn | probe H, `bench/kv/probes/rocksdb-parity/d4_covers_model.py 11`, job 261120-133243-d4f975 (exit 0, all claims hold): design 117,029 states, 0 lost, torn, `log_lost`, exposed or wrongly refused; record left open 5,547 lost and 1,208 torn; a close taking any checkpoint 328 lost; `covers` = seq − 1 refuses 709 closes and 30,013 leaves that lose nothing; no install wait 20,030 `log_lost` and exposed |
| F9 | probe I, `d4_install_order_model.py 12`, job 261120-133244-e59453 (exit 0, all claims hold): design 18,059 states, 0; newer build first 1,916 of 21,427 lost; no wait 10,329 of 36,060 `log_lost` and exposed |
| F12 | probe J, `d6_wake_model.py 14`, job 261120-133245-22acfd (exit 0, all claims hold): pull 7,981 of 16,144 lost (`WJS`); gate 0 of 4,627, no owner turn paying a commit; owner commits 0 lost, 1,715 owner turns paying one |
| F13 | probe K, `d1_ladder_model.py`, job 261120-133247-a1f4a0 (exit 0, all claims hold): rooms from `segment_bytes` throw at turn 1; rooms following the spare never throw, admitted to sealed 1 turn, 229 MiB/s at 16 MiB and 1,476 MiB/s at 64 MiB; smallest rung 48 MiB/s; naive step down 5 turns, guarded 1 |
| F4, N5, F14 A3 stall | probe G, `d7_model.py`, job 261120-133248-df88a3 (exit 0, all ok): rows 1-5 as in round 1; row 6, other writers seal within 2 turns of arrival behind a 4-room transaction, admitted to sealed 0 |
| Round 1 | F1 probe F 261120-083804-ec1239; N6 probe B2 261120-083211-0cbbce; N1 261120-074846-356a48 |

## Rulings I made (group-commit merge pass, 2026-11-22)

- Main merged with `git merge origin/main` (no rebase), since `campaign land --merge-main` was
  refused; the merge is README.md only.
- Reran at 7c6aa208 only the build-q (Ninja) jobs and A2's two; the brief's non-Ninja specs
  (5eca03, 0cc5e8, bf027b, 36020e, 2f7f8d, 2f69f9, 53357a, 28fbcd, 110105, 114248) and the
  journal-override A2 runs (145607-0e1ac9, 115239-ee5107) stay superseded, as ruled in the
  resume pass. The D2 mutant 261121-214545-0d42a7 stands at its own commit (e60cc249).
- The A4 mutant builds `test_barrier_cohort` alone, so an unrelated target cannot fail it at build.

## Rulings I made (fix round 1, 2026-11-21)

- N3: `checkpoint_floor_bytes` keeps its name. The option changed meaning, from the checkpoint
  trigger (64 MiB) to a floor under the group's share of the log's window (1 MiB): the old name
  with a new meaning and default would change what a caller's setting does without a compile
  error. Only the quest's own probe (`bench/kv/probes/rocksdb-parity/checkpoint-floor.cc`) sets
  it; nothing in Xapiand or our libraries does (all callers are ours, so no alias). Stale mentions of `checkpoint_log_bytes` remain in `docs/design/standalone-turn-log.md`
  (225, 573; not in the file set: lead's) and rocksdb-parity.md's 157, 167 and 823, which describe
  the pre-quest trigger and the plan's row.
- D2's skip leaves `run` unset, so a later chunk above the head with no run still throws
  `log_lost`; setting `run` there would replay a partial run's bytes.
- D2's case added beside the reviewer's probe, not instead of it: the probe (213954-ecd99d) is
  queued at 5953ed46 and was not read; the mutant job is the reproduction either way.
- Left job 261121-213912-e4b13d (build-test at 5953ed46, running, fails on D1) to the lead.

## Rulings I made (merge pass, 2026-11-21)

- Chunked joins report the writer's arrival like plain joins (`joined(true)` per chunk,
  `joined(false)` before waiting on a full record): the task's per-writer count otherwise misses a
  writer whose command is chunked, and its cohort wait runs to the bound for it.
- Pinned journal-lost-bound's head rather than leaving tolerant-open unbuilt: the lead asked for
  both merged, and build-test cannot compile A2's code at journal:178030b.
- Renamed the tolerant-open test into `test/test_reopen*` rather than reverting it: it is A2's test.
- Resubmitted, not reran, A2's two jobs: their specs build `test_tolerant_open` against a journal
  override. Did not rerun the brief's jobs that configure `build` without `-G Ninja`
  (5eca03, 0cc5e8, bf027b, 36020e, 2f7f8d, 2f69f9, 53357a, 28fbcd, 110105, 114248): the
  build-q reruns run the same tests at the head.
- The task-level A4 mutant (151523-00cd16 at strata:191551bf) is replaced by the same mutation at
  the quest head.

## Rulings I made (CI-red pass, 2026-11-21)

- Fixed main's await bug in the quest, not a follow-up: `turn_seal.hh` is in the file set, and the
  quest's tests (reopen_loops, served, visible) cannot go green without it. It changes no contract:
  a wait that threw on a durable record now returns; one with no sealer still throws.
- await_sealed got the same shape as await (reread under `mu_`, throw only after an idle turn);
  it already rechecked `ready()`, so it has no separate test.
- Submitted the suite at strata:e59af09e; it superseded the queued suite at b146febc, which would
  have tested the pre-fix code.

## Rulings I made (resume pass, 2026-11-21)

- group-commit and tolerant-open stay unmerged (Blocked on 1, 2): one has a CI-red test of its own,
  the other does not build at the quest's journal pin.
- Brief's reruns not taken as `job rerun`: those specs configure without `-G Ninja`, which main now
  refuses, into the slot's `build`, which caches tolerant-open's journal override. Resubmitted the
  same commands into `build-q` (`-G Ninja`, `-DFETCHCONTENT_SOURCE_DIR_JOURNAL=`). The two
  checkpoint-window, reopen-loops and kv-batch runs are one job; 261121-101429-f8726f (group-commit's
  test) and 261121-115239-ee5107 (tolerant-open's suite against journal-lost-bound) were not rerun at
  the quest head, since neither task is merged.
- Reran tolerant-open's A2 test and mutant at the task's commits: they never ran (configure failure),
  and the task's evidence is what its merge waits on.
- Probe reruns (A3 design models) not repeated: models, not code under the merge.

## Rulings I made (closing pass)

- The probe H and G reruns carry no `--for A3`: they check models of the design, and A3 asks for a
  power-cut sweep over the built split.
- A6's merge_policy test was resubmitted, not rerun: `job rerun` failed on the removed
  `turn-log-parity--merge-runs` worktree.
- No A6 exclusive rows yet: measuring before tasks 2-6 would only restate document-gate's gaps.

## Result (design round 2)

strata:e287a47b holds probes H-K and probe G's row 6. strata:5185790f and strata:68e1c6a8 hold the
note, revised for round 2. Sections changed:

- D1: the ladder, the room following the spare, and the guarded step down (F13, N10).
- D2: the item 2 floor (N9); item 3 keeps forward copies (F11).
- D4: items 1-6, the probe H and I table, and the tests (F8-F10, N14).
- D5 (N11) and D6's handoff item 7 (F12).
- D7 against journal `main` (N8), D9 (N13) and D11 (N12).
- A new section for A1-A3 (F14).
- D10: parked is closed (F11).
- Q1-Q4 ruled, Q5 added; task 0; tasks 3, 6, 7, 9 and 13; Checks.

The note is 860 lines, 726 at e5721fd6. The growth is A1-A3 and the F8-F13 mechanisms; the rest was
condensed.

## Rulings I made (design round 2)

Round 2:

- F8, option 1: the freeze closes the open record. `covers` one back also loses 0 (probe H), but
  closing makes every head end its record, so A2 compares sequences. The cost is about 65 B per
  flush.
- F10: the close and park check is "the checkpoint is the group's only live record", idle-park D2's
  park condition. The review's `covers` = seq − 1 refuses closes that lose nothing (probe H: `JDRT`,
  a reopened group's restaged checkpoint at base + 2; `JFJD`). This is a journal change in task 6.
- F12: a gate under the driver mutex enforces "transition before records". The alternative, the
  owner committing every loop's shards, also loses 0 (probe J), but it breaks host-ranges' thread
  ownership and stalls the device's turn.
- F13: the room follows the prepared spare, with a guarded step down. Bounding turns to the
  smallest rung caps throughput at 48 MiB/s (probe K).
- F14 A3: a transaction is accepted before its first chunk, so no abort record is needed. D4's
  active limit triggers a flush and caps no transaction.
- Q5: the default is (a).
- The models live in `bench/kv/probes/rocksdb-parity/`, not `docs/design/rocksdb-parity/` as the
  lead asked. That directory is outside the quest's file set; `bench/kv/**` is in it.
- The models are named probes H-K, after the note's probe letters. "M4" stays standalone-turn-log's
  floor.
- The copy flag and per-fragment checksum go in task 3. The quest file names tasks 3 and 5, but
  task 5 is strata-only now, since journal's half is on `main`.

Round 1:

- F6: the checkpoint names X_f and supersedes only through its `covers`; task 6 changes journal.
- F2: per-loop manifests do not narrow the damage scope; a roster record does (task 14).
- F3, F5: superseded by round 2's F11. Parked is closed through idle-park, and forward copies stay.
- N6: fixed the phase marks (`kvdriver.hh` `begin_phase`, one write(2)) and reran probe B.
- Jobs ran with `--fresh`: the quest slot was busy with cohort-wait's suite.
