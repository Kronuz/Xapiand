# strata-fields: journal

State: landed
Quest: .campaign/xapiand/quests/strata-fields.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:9fa890ecc on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/strata-fields.md`: "[x] SF1 fields declared through the field table, (field, term) postings: `Xapiand:` 46e3bb8df" |
| A2 | `docs/status/strata-fields.md`: "[x] SF2 `_id` routes through the key field; no host-built key term, no id term in `inverted`" |
| A3 | `docs/status/strata-fields.md`: "[x] SF3 / C2 `interim_terms.h` and `keys.h` deleted;" |
| A4 | `docs/status/strata-fields.md`: "[x] SF4 engine tests on both engines: ctest 28 of 28 (Release, no LTO)" |
| A5 | `docs/status/strata-fields.md`: "[x] SF5 `strata_callbacks_check.py`: 208 searches, 0 different; `numeric_range_check.py` on strata: 797 ranges, 0 wrong" |
| A6 | open: marked partial ([~]): strata E2E 507 of 576, 69 refusals in `docs/status/strata-fields.md` |
| A7 | `docs/status/strata-fields.md`: "TSan (`build-tsan`, own deps): fields, strata, search_strata 153616, callbacks, shard, replay: 0 warnings." |
| A8 | `docs/status/strata-fields.md`: "[x] SF8 mutants, each alone, `.scratch/mutants.py`" |
| A9 | open: marked partial ([~]): not at merge, no test in `docs/status/strata-fields.md` |
| A10 | open: marked partial ([~]): CR2's query-side marker awaits the analysis change in `docs/status/strata-fields.md` |
| A11 | `docs/status/strata-fields.md`: "[x] SF11 how-it-works note `docs/mechanisms/strata-fields.md`" |

## Result

Landed at Xapiand:9fa890ecc on 2026-10-28. ctest 28 of 28; `test_engine_fields` 1258; strata 9796; search-strata 231324; callbacks 946; text 13252. Review: `docs/reviews/strata-fields-code.md` named; ledger says approves with conditions.

- Merge: Merge branch 'phase7a/strata-fields' (SF: the strata engine on fields and keys).
- Xapiand:46e3bb8df field-aware engine; Xapiand:89e0b9f7b parsed-text stem field; Xapiand:3bacc26f1 key-helper test.
- Callbacks check 208 searches, 0 different; numeric range 797 ranges, 0 wrong.
- Docs E2E: Xapian 576 of 576; strata 507 of 576; 69 failures are recorded refusals.
- ASan+UBSan and TSan suites pass; TSan 0 warnings; mutants M1-M7 all killed.
- Open: measure `verify_fields` at open; no engine-level test for remote-protocol term path.
- Docs: `docs/mechanisms/strata-fields.md` named.
- Left open: A6, A9, A10.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
