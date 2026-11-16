# prospect-query-wins: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-query-wins.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:e20499a on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/query-wins.md`: "Q1: dumps identical to main with and without a count (both presets, k 10, 100, 1,000) at" |
| A2 | `docs/status/query-wins.md`: "Q2: parity 419 of 419 in one segment, four, and reordered; worst error 3.87e-16." |
| A3 | `docs/status/query-wins.md`: "Q3: each row reverted fails search_test's tally check (4 of 4); five mutants caught" |
| A4 | `docs/status/query-wins.md`: "Q4: ASan and UBSan (Homebrew LLVM, `-O1`): search, exact, multisegment, engine_query, and" |
| A5 | `docs/status/query-wins.md`: "Q5: 1M reordered, 16 rounds, A/A first, both presets: no row lost." |

## Result

Landed at prospect:e20499a on 2026-10-20. Merged on strata main before the repository split; the commit is its twin in prospect's filtered history.

- CI run 39438785455 green on 8 jobs; run 39448276304 seven of eight when last read.
- Review: none recorded (the ledger asks the lead to review and merge).
- Docs: `docs/perf/query-wins.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
