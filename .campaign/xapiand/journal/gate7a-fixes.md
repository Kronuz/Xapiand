# gate7a-fixes: journal

State: landed
Quest: .campaign/xapiand/quests/gate7a-fixes.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:8fb9032a2 on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/gate7a-fixes.md`: "`stamp_version` in `src/engine/strata/shard.cc` writes the slot at the three write sites" |
| A2 | `docs/status/gate7a-fixes.md`: "`validate` ignores the operator of an append onto an empty accumulation (`search.cc`)" |
| A3 | `docs/status/gate7a-fixes.md`: "`engine::check(path)` in `src/engine/open.{h,cc}` picks the engine by `kind_of`" |
| A4 | `docs/status/gate7a-fixes.md`: "`stats` takes the min/max over the segments that hold documents" |
| A5 | `docs/status/gate7a-fixes.md`: "ELITE_SET deleted: reserved word, DSL, default operator, `Op::ELITE_SET`, strata group" |
| A6 | `docs/status/gate7a-fixes.md`: "`_terms` reads stored values only; a text field or a field without values answers 400" |
| A7 | `docs/status/gate7a-fixes.md`: "INFO of a document lists no terms on either engine" |

## Result

Landed at Xapiand:8fb9032a2 on 2026-10-28. Release ctest 28 of 28. No review named in the ledger.

- Merge: Merge branch 'phase7a/gate-fixes' (gate 7a: harnesses, report and fix round).
- Xapiand:acd2cd42d PHRASE and NEAR over text on strata.
- Xapiand:42016bba9 version slot, `:check`, and length bounds.
- Xapiand:0c7d4bb59 ELITE_SET removal; Xapiand:dc78c182c text `_terms` and INFO term-list removal.
- T6: Xapian 566 of 566; strata 562 of 566; four failures are one PHRASE-window request.
- T7: 6 of 335 differ; callbacks 210 searches, 0 different; numeric ranges 797, 0 wrong.
- Open: PHRASE with window longer than its terms; lead/owner ruling required.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
