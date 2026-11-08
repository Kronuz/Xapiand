# x1-tests: journal

State: landed
Quest: .campaign/xapiand/quests/x1-tests.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:0ae3505c9 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/x1-tests.md`: "test_engine_strata, test_wal_replay, test_pool, test_offload_followups" |
| A2 | `docs/status/x1-tests.md`: "ctest: 25 of 25 passed." |

## Result

Landed at Xapiand:0ae3505c9 on 2026-10-29. ctest: 25 of 25 passed. No review named in the ledger.

- Merge: Merge branch 'phase7c/x1-delete-xapian' (Phase 7c X1: Xapian and the cluster deleted).
- Xapiand:1848ed81e, Xapiand:4f7c37fd2, Xapiand:0c6dfda16, Xapiand:a15e0b3f1, Xapiand:31b20ddd8.
- xapiand_fuzz_text: 55200 texts in 5 s, seed 1, no violation; xapiand_fuzz_body ran for 3 s.
- Three mutants were caught by query/callback, scan, and document/field tests.
- ENGINE_CENSUS test was not linked or run with census on; FUZZ-only libFuzzer targets were not built or run.
- Weakened lowering check and dropped Xapian-writer replay semantics remain for the lead to decide.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
