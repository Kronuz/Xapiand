# turn-log-parity: journal

State: ready for review
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Task design-fixes (design assignment): strata `docs/design/rocksdb-parity.md` revised for the
design review's F1-F6 and N1-N7 (`.campaign/xapiand/reviews/turn-log-parity-design.md`), on
`quest/xapiand/turn-log-parity` in `~/code/Kronuz/strata-wt/xapiand/turn-log-parity`. F7 is the
owner's.

## Next

1. Design review of strata `docs/design/rocksdb-parity.md` at strata:e5721fd6 (lens design or
   durability), F1-F6 and N1-N7; F7 stays the owner's.

## Blocked on

Nothing.

## Evidence

| Item | Evidence |
|---|---|
| F1 handoff | probe F, `bench/kv/probes/rocksdb-parity/probe-f.sh` (`handoff.cc`), job 261120-083804-ec1239 (exit 0): pull = push within noise, ask slower; the wake (9-17 µs) is the cost |
| F4, N5 | probe G, `d7_model.py` extended with one bound per log, job 261120-083208-36162a (exit 0, `all ok`) |
| N6 | probe B2, `kvdriver.hh` phase marks through write(2), job 261120-083211-0cbbce (exit 0): run-phase syncs per op 1.00/0.61-0.80/0.42-0.43/0.20 today, 1.00/0.50/0.25-0.26/0.13-0.14 cohort |
| N1 | probe table cites 261120-074846-356a48 |

## Result

strata:181b57dc (probe F, model), strata:d96f6595 (phase marks), strata:e5721fd6 (the note).
Note sections changed: D2 item 3 (F5), D3, D4 checkpoint position (F6), D5 (N3, N4), D6 and its
new "handoff" (F1) and "damage scope" (F2) subsections, D7 (F4, N5), D10 gaps 1 and items 2-3
(F3, N2), Q1, tasks 1-14, Checks.

## Rulings I made

- F6: the checkpoint names X_f and supersedes only through its `covers`; journal today supersedes
  every earlier record and refuses `covers` below the last sequence, so task 6 changes journal
  (covers in the checkpoint's bytes; recovery keeps records after it). Task 6 gains journal files.
- F5: forward copies are removed by a new task 13 (D10 items 2, 3, 5 after A6, plus a checkpoint
  asked of quiet groups), not by tasks 2-4.
- F2: per-loop manifests do not narrow the damage scope; a per-segment roster record does (16 B
  per live group per segment); recorded as task 14 after parking, not needed for A6.
- F3: the durable `parked` table state shares its name with Detent's in-memory `TurnDriver::park`
  (journal-idle-groups); the note says the lead settles the name and carries the text to Detent.
- N6: fixed the phase marks (kvdriver.hh `begin_phase` writes with one write(2); stdio bypassed
  the interposer) and reran probe B rather than relabel the column.
- Jobs ran with `--fresh`: the quest slot was busy with cohort-wait's suite.
