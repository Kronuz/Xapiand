# sortable-step1: journal

State: landed
Quest: .campaign/xapiand/quests/sortable-step1.md (the lead's; never edit it)

## Doing

Nothing: landed at sortable:33ed972 on 2026-11-01 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/sortable.md`: "1. `sortable.hh` (D2-D5, D7), constexpr-evaluable; tests, fixtures from `tools/model.py`" |
| A2 | `docs/status/sortable.md`: "50.9M values, 0 failures. T6: all vectors match. T4 exhaustive: 83.9M short inputs, 0 failures." |
| A3 | `docs/status/sortable.md`: "3. T5 with R1: `test/size.cc` includes sub-1/16 s sets (7.93 and 7.94 B against 7.99 B)" |
| A4 | `docs/status/sortable.md`: "4. C4 and the D2/D3 mutants: `tools/mutants.py`, 38 mutants, 0 unexpected." |
| A5 | `docs/status/sortable.md`: "5. Sanitizers in separate build dirs: ASan+UBSan and TSan, 4 of 4 tests pass." |
| A6 | `docs/status/sortable.md`: "6. Fuzz: `fuzz/decode_fuzz.cc`, 4 workers x 10 minutes, 279M runs, 0 findings." |
| A7 | `docs/status/sortable.md`: "7. C1: the 20 call forms compile and the 7 others do not, as `static_assert`s in `test/test.cc`." |

## Result

Landed at sortable:33ed972 on 2026-11-01. Published on sortable main; the ledger's last commit sortable:33ed972 (2026-11-01).

- Review: none yet (decoder review is `sortable-proof`'s).
- Docs: `docs/mechanisms/sortable.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
