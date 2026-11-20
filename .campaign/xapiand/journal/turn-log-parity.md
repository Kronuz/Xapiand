# turn-log-parity: journal

State: ready for review
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Task design-fixes, round 2 (design assignment): strata `docs/design/rocksdb-parity.md` revised for
the design review's round 2 (`.campaign/xapiand/reviews/turn-log-parity-design.md`, CHANGES at
strata:e5721fd6), F8-F14 and N8-N14, on `quest/xapiand/turn-log-parity` in
`~/code/Kronuz/strata-wt/xapiand/turn-log-parity`.

## Next

1. Lead: close the design review's round 2 on strata:68e1c6a8 from the probe jobs below (F8-F14,
   N8-N14); no text round is left.
2. Lead: send the journal changes in the note's task list (tasks 3, 6, 10, 13, 14 and
   `tolerant-open`, each with its journal test) to Detent's lead, and Q5 to the owner.
3. Lead: the quest file predates round 2. Task 13 now keeps forward copies and indexes live records
   by segment. The journal tasks are 2, 3, 6, 9, 10, 13, 14 and `tolerant-open`, and task 0 moves
   the pin. The note's directory `docs/design/rocksdb-parity/` is outside the file set (see
   Rulings).

## Blocked on

Nothing for this step. The owner holds Q5, read-your-writes off the owner's thread; the default is
(a), routing reads to the owner, with no new public API.

## Evidence

| Item | Evidence |
|---|---|
| F8, F10, F9a, A3 torn | probe H, `bench/kv/probes/rocksdb-parity/d4_covers_model.py 11`, job 261120-133243-d4f975 (exit 0, all claims hold): design 117,029 states, 0 lost, torn, `log_lost`, exposed or wrongly refused; record left open 5,547 lost and 1,208 torn; a close taking any checkpoint 328 lost; `covers` = seq − 1 refuses 709 closes and 30,013 leaves that lose nothing; no install wait 20,030 `log_lost` and exposed |
| F9 | probe I, `d4_install_order_model.py 12`, job 261120-133244-e59453 (exit 0, all claims hold): design 18,059 states, 0; newer build first 1,916 of 21,427 lost; no wait 10,329 of 36,060 `log_lost` and exposed |
| F12 | probe J, `d6_wake_model.py 14`, job 261120-133245-22acfd (exit 0, all claims hold): pull 7,981 of 16,144 lost (`WJS`); gate 0 of 4,627, no owner turn paying a commit; owner commits 0 lost, 1,715 owner turns paying one |
| F13 | probe K, `d1_ladder_model.py`, job 261120-133247-a1f4a0 (exit 0, all claims hold): rooms from `segment_bytes` throw at turn 1; rooms following the spare never throw, admitted to sealed 1 turn, 229 MiB/s at 16 MiB and 1,476 MiB/s at 64 MiB; smallest rung 48 MiB/s; naive step down 5 turns, guarded 1 |
| F4, N5, F14 A3 stall | probe G, `d7_model.py`, job 261120-133248-df88a3 (exit 0, all ok): rows 1-5 as in round 1; row 6, other writers seal within 2 turns of arrival behind a 4-room transaction, admitted to sealed 0 |
| Round 1 | F1 probe F 261120-083804-ec1239; N6 probe B2 261120-083211-0cbbce; N1 261120-074846-356a48 |

## Result

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

## Rulings I made

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
