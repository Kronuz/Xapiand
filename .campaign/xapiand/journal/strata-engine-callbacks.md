# strata-engine-callbacks: journal

State: landed
Quest: .campaign/xapiand/quests/strata-engine-callbacks.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:5c8b6651d on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/strata-engine-callbacks.md`: "[x] SYNONYM, XOR, MAX, ELITE_SET, SCALE_WEIGHT with Xapian's scoring (`test_operators`)." |
| A2 | `docs/status/strata-engine-callbacks.md`: "[x] WILDCARD and EDIT_DISTANCE expand over the dictionaries once per search (`test_expansions`)" |
| A3 | `docs/status/strata-engine-callbacks.md`: "[x] Posting sources scan the values plugin's column (`test_sources`)." |
| A4 | `docs/status/strata-engine-callbacks.md`: "[x] Key makers order by key, then weight, then docid, through `prospect::search`'s `key` hook (`test_sorter`)." |
| A5 | `docs/status/strata-engine-callbacks.md`: "[x] A spy sees every match with a lazy `SegmentDocView`" |
| A6 | `docs/status/strata-engine-callbacks.md`: "[x] Values read from the values plugin; term lists are not offered (R-P2)." |
| A7 | `docs/status/strata-engine-callbacks.md`: "[x] Tests shared with the Xapian engine: see Proof." |
| A8 | `docs/status/strata-engine-callbacks.md`: "[x] `harness/numeric_range_check.py` on a strata index: 797 ranges, 0 wrong." |
| A9 | `docs/status/strata-engine-callbacks.md`: "[x] `docs/mechanisms/strata-callbacks.md`." |

## Result

Landed at Xapiand:5c8b6651d on 2026-10-28. Release tests: `test_engine_callbacks` 942 checks, `test_engine_strata` 9568, `test_engine_search_strata` 231,298, `test_engine_search` 4423. Review (not named in the ledger): `docs/reviews/strata-engine-callbacks-code.md` -- APPROVE WITH CONDITIONS (Xapiand:0ccd64652).

- Merge: Merge branch 'phase7a/strata-engine-callbacks' (S3 review fixes: CR1, CR2, sort shapes, _terms cap, geo).
- Xapiand:3998a95b9 merge of master; Xapiand:95905a5f3 further master merge; Xapiand:74d8236e5 prospect repin; Xapiand:d43000e22 `_terms` over values and refusals.
- Final checks: 208 searches, 0 different; numeric-range check 797 ranges, 0 wrong.
- `terms_cap_check.py`: 345,000 documents, 200,000 distinct keywords and integers; top 10 counts within 1%.
- ASan+UBSan after review: callbacks 944, strata 9568, search-strata 231,298, search 4423 pass.
- Docs E2E on default engine: 576/576; strata engine remains outside the gate.
- Docs: `docs/mechanisms/strata-callbacks.md` named.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
