# x1-analysis: journal

State: landed
Quest: .campaign/xapiand/quests/x1-analysis.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:0cf555fac on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/dev/extraction-ledger.md`: "replaced by `analysis::term_generator`, `snowball::stemmer` and `analysis::stopper`" (row of 2026-10-20: analysis::term_generator, snowball::stemmer and analysis::stopper replace the default path; Kronuz/analysis a0825b1, Kronuz/snowball 570a0b7, Kronuz/unicode c93026c) |
| A2 | `docs/dev/extraction-ledger.md`: "`getGeneratorStopStrategy`/`getGeneratorStemStrategy`, the phf `SimpleStopper`" (the same row: the phf SimpleStopper and the stop and stem strategies replaced; German's duplicate stop word fixed) |
| A3 | `docs/dev/extraction-ledger.md`: "E2E docs suite on 335 requests: 570 of 570 assertions pass before and after" (the same row: docs E2E on 335 requests, 570 of 570 before and after, also under ASan and UBSan; bodies differ only in 2 volatile fields) and `docs/gates/phase0.5.md` (its base Xapiand:855f75281 is the parent of the first X1 commit) |

## Result

Landed at Xapiand:0cf555fac on 2026-10-20 (code Xapiand:fe9307227: 7 files, 186 insertions, 75 deletions). No review is named; the extraction ledger's proof is the record, and gate 0.5 (`gate0-5`) measured the seam from X1's parent.

- Docs E2E: 335 requests, 570 of 570 assertions before and after, under ASan and UBSan with no report in the changed code.
- `harness/terms_diff.py` over 130 configurations and `harness/vocab_diff.py` over 18 languages equal Xapian's except Danish, Italian and Norwegian apostrophe stems (a newer Snowball) and German stop words; a mutant of the stop mapping changes 168 documents.
- Xapiand grew by 105 lines (75 deleted, 180 of glue): the default path's Xapian term generator, stemmer and stopper became library calls.
- Docs: `docs/dev/extraction-ledger.md`, `docs/design/xapiand-on-strata.md`.

## Rulings I made

None beyond the quest file's Decisions (the extraction ledger is the record).
