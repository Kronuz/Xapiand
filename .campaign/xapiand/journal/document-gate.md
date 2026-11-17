# document-gate: journal

State: design fixed
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 design revision 1: strata `docs/design/standalone-turn-log.md` at strata:8e919415 (pushed),
folding the design review (Xapiand:dc0590b32, CHANGES) F1-F6, N1, N2, N4, N6, N7; N3 and N5 left
as owner questions Q3, Q4. Ready for the re-check.

## Next

1. Design review of standalone-turn-log.md (lens design, durability).
2. After review and the file-set widening: build first steps 2-5 of the design.

## Blocked on

- Lead: the file set lacks `include/strata/core/**`, `test/**`, `CMakeLists.txt` and
  `docs/design/standalone-turn-log.md`; building A1 needs them.
- Owner (design Q1-Q4): per-core logs on macOS (P2), the wal1 label, read-your-writes under
  ack=none (N3), a tolerant open after log damage (N5). Defaults recommended in the doc.
- Journal (Detent reviews): `TurnDriver::seal`; strata's journal pin moves to journal main for
  the host layer. Until seal lands, one record per transaction.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | design only: strata:8e919415 docs/design/standalone-turn-log.md; checks listed there as jobs |
| A2-A8 | <no evidence yet> |

## Result

Design for A1 in strata `docs/design/standalone-turn-log.md`: turn log as WAL, commit() as a
background checkpoint, ack and visibility at the turn barrier, one record per index per turn,
crash tests T1-T9, mutants M-a to M-f, measurements M1-M5. Nothing measured yet.

## Rulings I made

- Review fold-in (revision 1), by finding:
  - F1: lease from seal to checkpoint record; V and acks published by a watcher without the lease. Tests T1, T11; mutant M-g.
  - F2: owed verdicts run their chore at replay; no checkpoint until release. Test T7b; mutants M-h, M-i.
  - F3: position taken with the append under the record mutex; sealing by the driver. Test T12; mutant M-j.
  - F4: 64 MiB command limit at admission (replicated ceiling) and a half-segment turn budget. Test T8; mutant M-l.
  - F5: every flush on a logged index awaits the log. Test T4; mutant M-k.
  - F6: T1 writes after recovery (M-b); T10 refuses at runtime (M-f).
  - N1: D7 and D9 follow the host group table and fail-stop.
  - N2: Q1 uses P2, and M3 gains an N-log row.
  - N4: parity claimed for throughput and median only.
  - N6: group ids are never reused.
  - N7: a failed turn reports `outcome_unknown`.
- No strata turn runner: journal main's `TurnDriver::run()` and `journal::run` drive turns.

- Wrote `docs/design/standalone-turn-log.md`, outside the quest's file set, because the lead
  assigned that path.
- Apply-first (log only accepted transactions), not log-first (D2).
- Visibility waits for the barrier on logged indexes (D4).
- Checkpoint waits for the log through its position (I1), accepting a 1-2 barrier writer stall
  per checkpoint until past-generation checkpoints exist.
- Kept Track C's torn-turn rule, rejecting the manifest log's zero classifier for recycled
  segments (D10).
- The doc runs about 3,800 words, over the design-doc ceiling, because the assignment needs
  its tests, jobs and questions spelled out; most of the length is tables.
