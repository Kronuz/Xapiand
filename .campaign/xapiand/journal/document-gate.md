# document-gate: journal

State: design ready for review
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 design: strata `docs/design/standalone-turn-log.md` at strata:9b958322 (pushed on
quest/xapiand/document-gate). Ready for a design-lens review. No code.

## Next

1. Design review of standalone-turn-log.md (lens design, durability).
2. After review and the file-set widening: build first steps 2-5 of the design.

## Blocked on

- Lead: the file set lacks `include/strata/core/**`, `test/**`, `CMakeLists.txt` and
  `docs/design/standalone-turn-log.md`; building A1 needs them.
- Owner (design Q1-Q4): journal group retirement, journal failed-turn undo (T3 needs it),
  per-core vs per-device log on macOS, the wal1 row's label. Defaults recommended in the doc.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | design only: strata:9b958322 docs/design/standalone-turn-log.md; checks listed there as jobs |
| A2-A8 | <no evidence yet> |

## Result

Design for A1 in strata `docs/design/standalone-turn-log.md`: turn log as WAL, commit() as a
background checkpoint, ack and visibility at the turn barrier, one record per index per turn,
crash tests T1-T9, mutants M-a to M-f, measurements M1-M5. Nothing measured yet.

## Rulings I made

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
