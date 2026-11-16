# phase7c-linux-gate-fixes: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-linux-gate-fixes.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:ebfb8fdfe on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-linux-gate-fixes.md`: "All six items are done and proved, except two things that are not mine to close" |
| A2 | `docs/status/phase7c-linux-gate-fixes.md`: "GCC 13 ICE at `schemas_lru.cc:176`" |
| A3 | `docs/status/phase7c-linux-gate-fixes.md`: "same `stringified` guard as clang 18 + libc++ 18; library fix" |
| A4 | `docs/status/phase7c-linux-gate-fixes.md`: "`fuzz_body.cc:45` does not preprocess on GCC" |

## Result

Landed at Xapiand:ebfb8fdfe on 2026-11-02. BlockingScheduler TSan: 3 of 3 runs pass; TSan ctest 33 of 33, except two blocked tests. No review named in the ledger.

- Merge: Merge branch 'phase7c/linux-gate-fixes' (GCC ICE, BlockingScheduler race, msgpack_load bound, Linux toolchains, harness).
- `msgpack_load` ASan+UBSan ctest: 34 of 34; mutant array limit fails.
- clang 18 Release ctest: 34 of 34; clang 20 ASan+UBSan ctest: 34 of 34.
- macOS Release ctest: 35 of 35; docs E2E GREEN.
- Linux harness checks pass; mixed load exits 0 after 40.3 s, `model_equals_dump` true over 93,880 docs, 363 ids cut off, 0 failures.
- GCC-13 Release ctest: 32 of 34; `test_pool` and `test_blocking_guard` hang, reproduced on master.
- `Xapiand:8b4541e63` fixes BlockingScheduler; remaining master hang and TSan report need follow-up.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
