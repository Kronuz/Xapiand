# prospect-word-sum-memory: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-word-sum-memory.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:9de01a1 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/word-sum-memory.md`: "Bytes per iterator: `sizeof` 4,424 -> 3,400; malloc block 4,608 -> 3,584." |
| A2 | `docs/status/word-sum-memory.md`: "byte-identical to main on 100k merged, 100k flushed (4 segments), 100k reordered, 1M reordered" |
| A3 | `docs/status/word-sum-memory.md`: "Neutral: all inside the A/A band, buckets by query length also flat (1.001 to 1.010)." |
| A4 | `docs/status/word-sum-memory.md`: "the 14 other fill mutants are still caught (15 of 15, `.scratch/mutants.log`)." |
| A5 | open: Next; Intel Mac only in `docs/status/word-sum-memory.md` |

## Result

Landed at prospect:9de01a1 on 2026-10-26.

- Heap per scorer 18.5 KB to 16.0 KB (-2.5 MB per 1,000 in flight); peak RSS -0.3%.
- Review: the count-union-bits review's CR3 (`docs/reviews/count-union-bits.md` -- APPROVE WITH CONDITIONS).
- Docs: `docs/perf/count-union-bits.md`.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
