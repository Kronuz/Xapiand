# prospect-f13-query: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-f13-query.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:45a710e on 2026-10-19 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/f13-query.md`: "M16 (F10 at A3@128), the commit after cd98766: each group's level 0 becomes one record of" |
| A2 | `docs/status/f13-query.md`: "M23 (F11, F10's check), the commit after 88f23ee: FOR ordinal blocks store gap - 1" |
| A3 | `docs/status/f13-query.md`: "No variant cleared every row: M22 out of the writer, 03c79cc. Code 2 is reserved" |
| A4 | `docs/status/f13-query.md`: "M20 (F13 at two pairs per k), the commit after 7d91c9f: a term with skip blocks and wdfs" |
| A5 | `docs/status/f13-query.md`: "M3t (format-free), the commit after 07bf37d: in a reordered segment" |
| A6 | `docs/status/f13-query.md`: "The kept set against main then loses no row in four sessions (21, 33, 38, 38 wins)." |
| A7 | `docs/status/f13-query.md`: "Revert checks (.scratch/revert/mutate.py, each mutant built and run)" |
| A8 | open: waiting when the ledger stopped (the gate VM rows are `prospect-f13-gate`) in `docs/status/f13-query.md` |

## Result

Landed at prospect:45a710e on 2026-10-19. Landed on strata main before the split as the format 13 query items (review commit strata:8110f1d); prospect:45a710e.

- Review: `docs/reviews/f13-query-code.md` -- APPROVE WITH CONDITIONS.
- Docs: `docs/perf/format13-query-items.md` holds the numbers.
- Docs: `docs/perf/format13-query-items.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
