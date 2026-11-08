# hashes-cl-1: journal

State: landed
Quest: .campaign/xapiand/quests/hashes-cl-1.md (the lead's; never edit it)

## Doing

Nothing: landed at hashes:1543e21 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cl-1.md`: "crc32c.hh, test/crc32c_test.cc, bench/crc32c.cc removed; README, ARCHITECTURE, AGENTS," |
| A2 | `docs/status/cl-1.md`: "configures, builds and runs in both declaration orders" |
| A3 | `docs/status/cl-1.md`: "configures and generates with journal switched to the Detent replay branch" |
| A4 | `docs/status/cl-1.md`: "CI for this branch: run 39421533539, all eight jobs green." |

## Result

Landed at hashes:1543e21 on 2026-10-20. Landed at hashes:aba7865 (remove CRC-32C) and hashes:1543e21 (2026-10-20).

- Review: `docs/reviews/cl-1-code.md` -- APPROVE WITH CONDITIONS.
- Catalog: cpp-libraries:b8664a0 merges `cl-1-crc32c-catalog`.
- Docs: `docs/BENCHMARKS.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
