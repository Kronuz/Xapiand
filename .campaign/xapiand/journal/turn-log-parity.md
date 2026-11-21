# turn-log-parity: journal

State: blocked on lead: A1-A3, A5 and A6 have no built mechanism (tasks ryw, tolerant-open, large-tx, design-note, parity 2-6 and 8-14 not run); jobs 261120-192604-6f5291 261120-192604-94fa59 261120-192606-891335 261120-192628-0cc5e8 pending
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Closing pass at strata:962e6c74 (tasks cohort-wait, merge-runs and journal-pin merged). Built on
the quest branch: parity task 0 (journal pin, now 178030b), task 1 (D5 cohort wait, A4) and task 7
(D9 sorted runs). Not built: A1 read-your-writes, A2 tolerant open, A3 large transactions, the
design note's Q3-Q5 closure (A5), and parity tasks 2-6 and 8-14 that A6's rows need
(`docs/design/rocksdb-parity.md`, "Tasks for turn-log-parity" and "A1-A3").

## Next

1. Lead: the quest cannot close on its three tasks. Launch the tasks the criteria need: `ryw` (A1),
   `tolerant-open` (A2, needs parity task 6's `covers` and task 3's copy flag), `large-tx` (A3),
   parity tasks 2-6 and 8-14 (A6), then `design-note` (A5) once A1-A3 are built.
2. Read jobs 261120-192604-6f5291, 261120-192604-94fa59 (probe reruns), 261120-192606-891335
   (build-test) and 261120-192628-0cc5e8 (A6 merge_policy test) into the table below.

## Blocked on

Lead: tasks for A1-A3, A5 and A6 (above). No job can prove a mechanism that is not built; the
smallest step per criterion is its task, each with the test and mutant its row names.

## Evidence

At strata:962e6c74 unless stated.

| Criterion | Evidence |
|---|---|
| A1 | none: not built. Proof once task `ryw` lands: its faultfs test (owner get sees the held-barrier `ack=none` write, a view does not) `--for A1`, and `--fails-without` at the mutant whose owner read is cut at V (rocksdb-parity.md "A1") |
| A2 | none: not built. Proof once task `tolerant-open` lands: the C/U/W damaged-block test `--for A2`, `--fails-without` at the mutant that opens every damaged group (rocksdb-parity.md "A2") |
| A3 | design models only, not the built path: probe H rerun 261120-192604-6f5291 (pending; at e287a47b 261120-133243-d4f975 passed, 0 torn states), probe G rerun 261120-192604-94fa59 (pending; at e287a47b 261120-133248-df88a3 passed, row 6 seals within 2 turns). The criterion's power-cut sweep over the built split, with its three mutants, comes with task `large-tx` |
| A4 | test_barrier_cohort K=4: 261120-192604-5eca03 passed at 962e6c74 (rerun of 261120-082445-11a61b); `--fails-without` 261120-082448-f562c7 at the mutant strata:0ceb74fb (no wait in `serve()`), failed as required |
| A5 | none: `docs/design/standalone-turn-log.md` still reads "Q3 to Q5 are ruled (D103)" and each Q says "quest `turn-log-parity` closes the gap after `document-gate` lands", so it records them open. Closing them in the note before A1-A3 are built would be false |
| A6 | partial, no gate rows: task 7's merge_policy test 261120-192628-0cc5e8 (pending; rerun of 261120-081226-1f9736, whose worktree is gone), `--fails-without` 261120-081225-8f09de at the mutant ignoring runs. No document-gate row has been measured at this head: the exclusive rows (host, Linux VM, FreeBSD VM; at most 10 minutes each) wait for tasks 2-6 and 8-12, since rows 2-6 move disk, bytes written, peak memory, p99.99 and CPU |
| build-test | 261120-192606-891335 (pending) |
| journal pin | t5d 261120-174858-0afc2a passed at strata:37197958; mutant at 9e11713 261120-174913-536451 exited 1 as required; suite 261120-174901-9520ed passed |

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
