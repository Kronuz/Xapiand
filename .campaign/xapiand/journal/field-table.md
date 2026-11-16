# field-table: journal

State: landed
Quest: .campaign/xapiand/quests/field-table.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:c4ea4ce on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/field-table.md`: "Ticked. `core/fields.hh` (table, chain, codec); manifest `fields`/`next_fields`, version 0x80000006" |
| A2 | `docs/status/field-table.md`: "Ticked. Stage 1 in `evaluate()`; every rule has a killed mutant" |
| A3 | `docs/status/field-table.md`; CI green at strata:c4ea4cefbb65 (run 40686925350, 8 of 8 checks), read on GitHub on 2026-11-08 (the ledger was written while it ran) |
| A4 | `docs/status/field-table.md`: "Ticked. `bench/field_table` (release, Intel Mac, faultfs): 2,000 fields one per commit write 36,903 B" |
| A5 | `docs/status/field-table.md`: "Ticked. `docs/mechanisms/field-table.md`. `key_prefix` and the segments are untouched." |

## Result

Landed at strata:c4ea4ce on 2026-10-28. Release suite: 57 tests pass. Reviews: `docs/reviews/field-table-check.md` -- APPROVE WITH CONDITIONS (durability and decoder-safety check, 2026-10-28, strata:347a7da); `docs/reviews/fields-and-keys-design.md` (Re-check 1).

- Merge: Merge branch 'phase8/field-table' (F1: the field table).
- strata:f3b7594 and strata:4f3c1e7 deliver implementation and proof; strata:219df7e merges main.
- T8: 2,000 fields write 36,903 B versus 26,909 B final file (1.37x); 100,000 write 1,988,903 B versus 1,488,910 B (1.34x).
- Resident table is 171 B per field at 2,000 and 167 B at 100,000.
- Decoder fuzz: 11.2M runs in 8 minutes, no finding; 203 corpus entries added.
- CI was pushed but not awaited. The decoder and append-only file check the ledger left next is `docs/reviews/field-table-check.md` (APPROVE WITH CONDITIONS).
- Docs: `docs/design/fields-and-keys.md`, `docs/mechanisms/field-table.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
