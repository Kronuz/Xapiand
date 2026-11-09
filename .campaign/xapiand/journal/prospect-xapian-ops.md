# prospect-xapian-ops: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-xapian-ops.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:7f1d915 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/xapian-ops.md`: "O1, oracle parity: 669 rows compared, counted and pruned (match count and top-50 weights to 1e-9)" |
| A2 | `docs/status/xapian-ops.md`: "O2, property tests: 250 random trees on 300 and 5,000 documents and 60 on 20,000" |
| A3 | `docs/status/xapian-ops.md`: "O3, mutants: 25 (`test/ops_mutants.py`), every operator and the source covered, all caught." |
| A4 | `docs/status/xapian-ops.md`: "O4, sanitizers on macOS x86_64, Homebrew LLVM 23 at -O1" |
| A5 | `docs/status/xapian-ops.md`; CI run 40616608446 green at prospect:e134c8e7f, read on GitHub on 2026-11-08 (the ledger was written while it ran) |

## Result

Landed at prospect:7f1d915 on 2026-10-27.

- Vectors: 600 documents, 711 queries, 1,423 rows.
- Review: `docs/reviews/xapian-ops-code.md` -- APPROVE WITH CONDITIONS.
- Docs: `docs/mechanisms/query-operators.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
