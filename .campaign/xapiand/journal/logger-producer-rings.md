# logger-producer-rings: journal

State: landed
Quest: .campaign/xapiand/quests/logger-producer-rings.md (the lead's; never edit it)

## Doing

Nothing: landed at logger:765528e on 2026-11-01 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | open: met on the M4 Pro and in bursts on the i9; back to back on the i9 83.6 ns against 62 (logger#1 stays open) in `docs/status/producer-rings.md` |
| A2 | `docs/status/producer-rings.md`: "exactly once at 1, 2, 4, 8 and 16 producers; order across 4 and 16 threads; due order with routine lines" |
| A3 | `docs/status/producer-rings.md`: "ASan+UBSan ctest 11/11 (`logger_queue` 11.8 s), TSan ctest 10/10 (`logger_queue` 10.8 s, quick sizes), no reports." |
| A4 | `docs/status/producer-rings.md`: "START ce27b90 and main a82454b against it, 5 rounds with A/A (CHANGELOG, README)" |

## Result

Landed at logger:765528e on 2026-11-01. Landed as logger:21e9642 and logger:fb843c0, the boundary test logger:765528e (2026-11-01).

- M4 Pro one producer 22.7 ns in bursts (main 53.3, START 254.2, fmt + mutex 28); i9 59.3 and 83.6 (fmt + mutex 65 and 62).
- Review: none recorded.
- Docs: `CHANGELOG.md`.
- Left open: A1.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
