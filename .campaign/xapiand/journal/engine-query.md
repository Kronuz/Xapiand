# engine-query: journal

State: landed
Quest: .campaign/xapiand/quests/engine-query.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:1ab0ce077 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/engine-query.md`: "`src/engine/query.{h,cc}`, `src/engine/callbacks.h`: `Query`, `Op`, `TextConfig`, `text::` flags" |
| A2 | `docs/status/engine-query.md`: "`src/engine/xapian/query.{h,cc}`: `to_xapian(Query)`." |
| A3 | `docs/status/engine-query.md`: "New `test_engine_query`: 22091 checks (leaves and operators against the Xapian calls" |
| A4 | `docs/status/engine-query.md`: "Migrated: `query_dsl.{h,cc}` (no `Xapian::` left), `multivalue/range.{h,cc}` and" |
| A5 | `docs/status/engine-query.md`: "`database/handler.{h,cc}` (`get_mset`, `get_rset`)" |
| A6 | `docs/status/engine-query.md`: "`server/remote_protocol_views.cc` (four registrations)" |
| A7 | `docs/status/engine-query.md`: "0 of 301 differ between base and final, error messages included." |
| A8 | `docs/status/engine-query.md`: "0 AddressSanitizer reports; the UBSan lines are the lz4 and rapidjson ones the base shows" |
| A9 | `docs/status/engine-query.md`: "Unit mutants (rebuild the test, run it; `.scratch/mutants.py`, output `.scratch/mutants.out`), each caught:" |
| A10 | `docs/status/engine-query.md`: "[x] `Multi_MultiValueKeyMaker` derives from `engine::KeyMaker` and reads a `DocView`" |
| A11 | `docs/status/engine-query.md`: "[x] `DatabaseHandler::get_mset` takes the sorter and the spy as engine types" |
| A12 | `docs/status/engine-query.md`: "[x] Behaviour unchanged: docs E2E, the query corpus extended with sorts, collapse and aggregations" |
| A13 | `docs/status/engine-query.md`: "[x] ASan+UBSan and revert checks. Proof 6 to 8." |

## Result

Landed at Xapiand:1ab0ce077 on 2026-10-26. Step 1: `test_engine_query` 22395 checks; 14 of 14 unit tests; 301-answer corpus identical. Review: `docs/reviews/engine-query-code.md` -- APPROVE WITH CONDITIONS; `docs/reviews/engine-query-2-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase0.5/engine-query-2'.
- Xapiand:801f16f5c -- query and callback bases; 868af2635 -- tests and serialization fix; db0be79ad -- review fixes.
- Xapiand:4bbdbbe9b -- sort keys and spies; dde867bc6 -- tests; 635bff800 -- remote registrations; f969aea56 -- close review conditions.
- Step 1: E2E 339 requests, 576 of 576 assertions; 2-node cluster GREEN.
- Step 2: `test_engine_sort` 28144 checks; 15 of 15 unit tests; query corpus 2007 answers identical.
- Step 2: 2-node cluster range GREEN, 30 checks; seam metric 46 / 889 to 38 / 704 files / lines.
- After C1-C3: 40808 checks; qdiff 2007 and qnf 104 answers identical; cluster GREEN.
- Open: TSan, Linux, FreeBSD, benchmarks, and libFuzzer not run; extreme-range sort behavior was a review follow-up.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
