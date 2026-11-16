# prospect-f13-preflight: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-f13-preflight.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:b60af99 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/f13-preflight.md`: "K1: segments identical to main's (cmp, identity seed 1) at 100k (51 flushed segments, docid and reordering merges)" |
| A2 | `docs/status/f13-preflight.md`: "build pairs C/A 0.970, 0.962, 1.045, 1.075 (mean 1.012, noise about the effect's size)" |
| A3 | `docs/status/f13-preflight.md`: "Attribution: every class classified and ranked by expected gain" |
| A4 | `docs/status/f13-preflight.md`: "FQ3's level-1 positional offset built and timed: 0.988 phrase `COUNT`, 0.983 `TOP_1000`, for +253,760 bytes at 1M" |

## Result

Landed at prospect:b60af99 on 2026-10-20. Merged on strata main before the repository split; the commit is its twin in prospect's filtered history.

- In situ the new seeds take 14.1 cycles per posting of the 5M merge (2.1% of its CPU) against 67.7; merge peak RSS +99 MB.
- Two format-free prototypes measured: counted unions 2.1 to 2.75 times faster, intersection_union 1.14 to 1.22 (built in `prospect-query-wins`).
- Review: none.
- Docs: `docs/perf/format13-seeds-cost.md`, `docs/perf/format13-loss-attribution.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
