# prospect-fields: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-fields.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:3aeabbb on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/fields.md`: "[x] F1. `fielddict.hh`: D10 varint, builder, reader, cursor." |
| A2 | open: partial ([~]): sparse-kw 1.128 and 1.084 missed targets 1.11 and 1.08 in `docs/status/fields.md` |
| A3 | `docs/status/fields.md`: "[x] F3. Field parts, stage 1 (`fields_test` rejections); mutants D (key field), E (reference by number) die." |
| A4 | `docs/status/fields.md`: "[x] F4. Writer, tuple buffer, merge, reader, check, digest on (field, bytes)" |
| A5 | open: partial ([~]) in the ledger in `docs/status/fields.md` |
| A6 | `docs/status/fields.md`: "[x] F6. D7: the key field routes to `keys` (`find_term`, mutant G dies)" |
| A7 | `docs/status/fields.md`: "[x] F7. D9/D14: `term_table`, `snapshot_df`, `multi_snapshot::df` by name" |
| A8 | `docs/status/fields.md`: "[x] F8. T3b on Xapiand's bank (1,000 documents, 49 fields, Q and V wdf in the length): 240 term/OR/AND queries" |
| A9 | open: not built (D4) in `docs/status/fields.md` |
| A10 | open: not run in `docs/status/fields.md` |
| A11 | `docs/status/fields.md`: "[x] F11. Strata bump 0x8000000C (`strata:32b6800` repins `flush_memory`, `merge_stream`, `format14`" |
| A12 | `docs/status/fields.md`: "[x] F12. ASan+UBSan (`build-asan`) and TSan (`build-tsan`) in separate dirs with their own deps" |

## Result

Landed at prospect:3aeabbb on 2026-10-28.

- Bugs the proof found: the empty term's 64-bit shift (UBSan), reorder grouping by bytes only, an out-of-order dictionary the fuzz oracle found.
- Review: `docs/reviews/fields-security.md` -- REJECT until SR1 closes; re-check: F2 may land, SR1 to SR3 closed with reverted-fix failures, SR4 and SR5 open as hand-offs (termdict; strata field table).
- Docs: `docs/mechanisms/fields.md`.
- Left open: A2, A5, A9, A10.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
