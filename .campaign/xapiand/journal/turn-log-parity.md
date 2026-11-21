# turn-log-parity: journal

State: waiting on jobs 261121-151646-4dcef9 261121-151648-9ffb16 261121-151714-b5123c 261121-151701-7bf803 261121-151703-21b6d5 261121-151716-4f387f 261121-151725-649183
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

CI-red pass (lead's three failures of run 44938627226) at strata:e59af09e, pushed:

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

1. Read the CI-red pass jobs: 151646-4dcef9 (fix, Release), 151648-9ffb16 (mutant must fail),
   151714-b5123c (main must fail), 151701-7bf803 (TSan), 151703-21b6d5 (TSan mutant must fail),
   151716-4f387f (ASan+UBSan), 151725-649183 (suite at strata:e59af09e); then CI at strata:e59af09e.
2. Read jobs 261121-145544-911db0 (suite), 145545-2e50d3 and 145547-24d332 (A1, A1 TSan),
   145549-3a3a17 and 145601-f05091 (A3 sweep, A3 fuzz), 145550-f81cd5 (A4), 145551-877bc0 and
   145553-9dd19f (A6 D9, D2/D10) into Evidence; tolerant-open's 145607-0e1ac9 (A2 test at
   strata:a16d117c, journal:1e3505a) and 145613-34602d (its opens-every mutant, `--fails-without`).
3. Lead: Blocked on items 1-3.

## Blocked on

1. Lead, task group-commit (strata:2f236c94) not merged: not proven on CI. Its jobs pass
   (A4 test 261121-101429-f8726f; mutant 261121-101433-65f5c5 failed as required; suite
   261121-114248-d19804; TSan in CI run 44934094130 passed), but the same CI run fails its own
   `barrier_cohort` on freebsd-x86_64 (K=4 I=4 L=1: 16 of 84 commits on a 2nd barrier) and
   macos-arm64 (K=4 I=2 L=2). Its exact "every commit rides the next barrier" check is timing-bound
   on shared runners. The base test is also flaky there (freebsd in large-tx's run 44938627226,
   line 130), so group-commit inherits the flake and widens it. Needs a fix in the task: a check
   robust to runner stalls (e.g. count writers whose 2nd barrier follows a release they observed
   late), proven on CI, before it merges.
2. Lead, task tolerant-open (strata:a16d117c) not merged: it builds only against
   journal-lost-bound (journal:1e3505a, in review; CI run 44937383488 fails to compile at today's
   pin: no `Host::bound`, `Host::lose`, `GroupStatus::bounded`), and its A2 test and mutant never
   ran (261121-104705-3d9542, 261121-104707-bd7691 failed at configure: Makefiles `build`). Reran
   as 145607-0e1ac9 and 145613-34602d. It merges with strata's journal pin bump once
   journal-lost-bound lands. Its standalone suite 261121-115239-ee5107 and journal mutant
   261121-115241-8bbc79 (exit 134) are done.
3. Lead, the build-test check (`cmake -B build ...`) has no `-G Ninja`. Since main's 9cefc0e5 a fresh
   configure without it is refused, and the slot's `build` holds tolerant-open's cache
   (`FETCHCONTENT_SOURCE_DIR_JOURNAL` = the slot's `ref-journal`, RelWithDebInfo), so `campaign
   verify` would build the quest against journal-lost-bound. I did not run it; suite
   261121-145544-911db0 stands in (`-G Ninja -B build-q`, journal override cleared). Suggested check:
   `cmake -G Ninja -B build-q -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JOURNAL= && ...`.
4. Resolved in the CI-red pass (Doing): fix and tests at strata:e59af09e, jobs pending. Was: the
   quest head is red on CI before this merge. Large-tx's run 44938627226 (merged at
   b467309d): `standalone_turn_log` aborts under ASan; `large_tx` has a TSan data race at
   test/test_large_tx.cc:436 (`visible()`); `reopen_loops` on linux-arm64-gcc throws "strata: a
   turn-log wait with nothing to write" (turn_seal.hh:241/269) during recovery, seed-dependent (also
   in group-commit's run: step 1 cut 20 seed 2621; large-tx's: cut 44 seed 5764). The last one is a
   recovery-path logic_error, a durability bug in a merged task, not a test flake. CI on b146febc
   will show whether they persist.
5. Earlier: A5 and A6's gate rows wait for parity tasks 2-6 and 8-14 (closing pass below).

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

At strata:b146febc (main merged) unless stated. Earlier passing runs ran pre-merge code; their
replacements are the pending jobs named here. Mutant runs (`--fails-without`) stand: they ran
at mutant commits.

| Criterion | Evidence |
|---|---|
| A1 | test_ryw + t4 served 261121-145545-2e50d3, TSan 261121-145547-24d332 (pending; replace 261121-090415-53357a, 261121-090438-28fbcd). Fails without: 261121-090430-5cd461 (owner-cut-at-v), 261121-090433-fa1d1f (install-before-log-wait), 261121-090436-c180a4 (v-follows-apply) |
| A2 | not on the quest branch (Blocked on 2). Task tolerant-open: 261121-145607-0e1ac9 (pending), suite 261121-115239-ee5107; fails without: 261121-115241-8bbc79, 261121-145613-34602d (pending) |
| A3 | test_large_tx 261121-145549-3a3a17, fuzz 261121-145601-f05091 (pending; replace 261121-110105-830c31, 261121-114248-3845d4). Fails without: 261121-110107-c64ff6, 261121-110108-aa171b, 261121-110110-5b9806, 261121-110112-a647c1 (and round 1 261121-104113-4f8cbb, 261121-104116-816fc6, 261121-104119-d0dd11) |
| A4 | base cohort test (per group) 261121-145550-f81cd5 (pending; replaces 261120-192604-5eca03); fails without: 261120-082448-f562c7. The per-writer count is group-commit's, not merged (Blocked on 1) |
| A5 | none: the note still records Q3-Q5 open; design-note task not run |
| A6 | partial, no gate rows: merge_policy 261121-145551-877bc0, checkpoint window + reopen loops + kv batch 261121-145553-9dd19f (pending; replace 261120-192628-0cc5e8, 261120-194415-bf027b, 261120-204250-36020e, 261121-050248-2f7f8d, 261121-054157-2f69f9); fails without: 261120-081225-8f09de, 261120-204252-7f2e9c, 261120-204253-47f1cc |
| build-test | suite 261121-151725-649183 at strata:e59af09e (pending; stands in for the check, Blocked on 3; superseded the queued 261121-145544-911db0 at b146febc) |
| await reread (CI-red 1, 3) | fix 261121-151646-4dcef9, ASan+UBSan 261121-151716-4f387f, TSan 261121-151701-7bf803 (pending). Fails without: 261121-151648-9ffb16 (pending). Pre-existing on main: 261121-151714-b5123c (pending; passes when `rival` fails at main cc30217f plus the test) |
| large_tx race (CI-red 2) | TSan 261121-151701-7bf803 (pending). Fails without: 261121-151703-21b6d5 (TSan at the plain-counter mutant, pending) |
| CI | strata:e59af09e pushed; run pending (b146febc's 44959280523 superseded) |

Smallest jobs, once built (each a `campaign job submit --quest turn-log-parity`):

- A1: `--for A1 -- build/<ryw test>`, and the same at the cut-at-V mutant with `--fails-without`.
- A2: `--for A2 -- build/<tolerant-open test>`, and at the open-everything mutant `--fails-without`.
- A3: `--for A3 -- build/<large-tx power-cut sweep>` with `chunk_bytes` 4 KiB, and three
  `--fails-without` runs (apply per chunk, freeze leaves the record open, limit at `command_bytes`).
- A6: `--level exclusive --minutes 10` per row set of document-gate's gate report, host first, a
  seconds-long smoke before each.

Hunt item "journal park contract" (Detent msg-188): not applied. Its two strata items wait for a pin
at journal:b6a7697 or later; this branch pins journal:178030b, and b6a7697 is only on
`quest/detent/journal-idle-groups`, not an ancestor of 178030b.

### Design round 2 probes (models of the design, not of built code)

| Item | Evidence |
|---|---|
| F8, F10, F9a, A3 torn | probe H, `bench/kv/probes/rocksdb-parity/d4_covers_model.py 11`, job 261120-133243-d4f975 (exit 0, all claims hold): design 117,029 states, 0 lost, torn, `log_lost`, exposed or wrongly refused; record left open 5,547 lost and 1,208 torn; a close taking any checkpoint 328 lost; `covers` = seq − 1 refuses 709 closes and 30,013 leaves that lose nothing; no install wait 20,030 `log_lost` and exposed |
| F9 | probe I, `d4_install_order_model.py 12`, job 261120-133244-e59453 (exit 0, all claims hold): design 18,059 states, 0; newer build first 1,916 of 21,427 lost; no wait 10,329 of 36,060 `log_lost` and exposed |
| F12 | probe J, `d6_wake_model.py 14`, job 261120-133245-22acfd (exit 0, all claims hold): pull 7,981 of 16,144 lost (`WJS`); gate 0 of 4,627, no owner turn paying a commit; owner commits 0 lost, 1,715 owner turns paying one |
| F13 | probe K, `d1_ladder_model.py`, job 261120-133247-a1f4a0 (exit 0, all claims hold): rooms from `segment_bytes` throw at turn 1; rooms following the spare never throw, admitted to sealed 1 turn, 229 MiB/s at 16 MiB and 1,476 MiB/s at 64 MiB; smallest rung 48 MiB/s; naive step down 5 turns, guarded 1 |
| F4, N5, F14 A3 stall | probe G, `d7_model.py`, job 261120-133248-df88a3 (exit 0, all ok): rows 1-5 as in round 1; row 6, other writers seal within 2 turns of arrival behind a 4-room transaction, admitted to sealed 0 |
| Round 1 | F1 probe F 261120-083804-ec1239; N6 probe B2 261120-083211-0cbbce; N1 261120-074846-356a48 |

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
