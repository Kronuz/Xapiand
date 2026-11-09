# seam-fields: journal

State: landed
Quest: .campaign/xapiand/quests/seam-fields.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:8e1183c1a on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/seam-fields.md`: "[x] X1 `engine::Document` and the query nodes carry a field; the Xapian engine joins (field, term) into today's term." |
| A2 | `docs/status/seam-fields.md`: "[x] X2 Every site of D12's counts migrated; `git grep 'prefixed('` is empty" |
| A3 | `docs/status/seam-fields.md`: "[x] X3 SEARCH dumps, `:info` hashes and the shard term dump equal master's on the Xapian engine." |
| A4 | `docs/status/seam-fields.md`: "[x] X4 Docs E2E 576 of 576." |
| A5 | `docs/status/seam-fields.md`: "[x] X5 The engine census unchanged (one explained counter, below)." |
| A6 | `docs/status/seam-fields.md`: "[x] X6 `harness/seam_check.sh` green (exit 0, files=7 lines=158, equal to master)." |
| A7 | `docs/status/seam-fields.md`: "[x] X7 The engine tests on both engines (ctest 25 of 25)." |
| A8 | `docs/status/seam-fields.md`: "[x] X8 ASan+UBSan on the engine tests." |
| A9 | `docs/status/seam-fields.md`: "[x] X9 A mutant that maps a field to the wrong prefix fails a test (ten mutants, all killed)." |
| A10 | `docs/status/seam-fields.md`: "[x] X10 C4: the interim mapping is `src/engine/strata/interim_terms.h`" |
| A11 | `docs/status/seam-fields.md`: "[x] X11 `docs/mechanisms/seam-fields.md`." |

## Result

Landed at Xapiand:8e1183c1a on 2026-10-28. ctest 25 of 25; `test_engine_fields` 1237 checks. Review: `docs/reviews/seam-fields-code.md` named; ledger says approve with conditions.

- Merge: Merge branch 'phase7a/seam-fields' (X6: fields at the engine seam).
- Xapiand:a919deb6f field-aware seam; Xapiand:65a3df348 `harness/fields_identity.py`.
- `fields_identity.py`: search, `:info`, and shard term dump SHA256 equal; analysis configs also equal.
- Docs E2E 576 of 576 on both engines; two documented body differences.
- ASan+UBSan engine tests pass; 0 ASan reports; mutants all killed.
- Merged with S2; code review records CR1 and CR3 fixed, CR2/CR4/CR5/CR6 for SF.
- Docs: `docs/mechanisms/seam-fields.md` named.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
