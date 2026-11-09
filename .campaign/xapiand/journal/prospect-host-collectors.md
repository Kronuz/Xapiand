# prospect-host-collectors: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-host-collectors.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:d8274b7 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/host-collectors.md`: "[x] H1 Identity: a search over several indexes equals a search over one merged index at the interleaved" |
| A2 | `docs/status/host-collectors.md`: "[x] H2 Docid interleave matches Xapian's: 3,696 oracle rows" |
| A3 | `docs/status/host-collectors.md`: "[x] H3 Property test over random shard splits (`identity()`, `visitor()`, `numbering()`: 20,000 round trips)." |
| A4 | `docs/status/host-collectors.md`: "[x] H4 The exhaustive visitor sees every match exactly once (`visitor()`: 145,089 matches over 40 rounds" |
| A5 | `docs/status/host-collectors.md`: "[x] H5 A mutant per claim, each caught (`test/shards_mutants.py`, 33 mutants" |
| A6 | `docs/status/host-collectors.md`: "[x] H6 ASan+UBSan and TSan, each in its own build directory with its own dependencies" |
| A7 | `docs/status/host-collectors.md`: "[x] H7 CI green: run 40629357689 on `prospect:83e8152`" |
| A8 | `docs/status/host-collectors.md`: "[x] H8 `docs/mechanisms/multi-index-search.md`." |
| A9 | `docs/status/host-collectors.md`: "[x] H9 Smoke A/B of single-index search (screen only, `bench/quick/ab.sh`" |

## Result

Landed at prospect:d8274b7 on 2026-10-27. collapse added later (prospect:0d7c43d).

- Review: none recorded (the ledger ends in review; S2 built on it).
- Docs: `docs/mechanisms/multi-index-search.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
