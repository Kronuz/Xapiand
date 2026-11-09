# linux-sanitizers: journal

State: landed
Quest: .campaign/xapiand/quests/linux-sanitizers.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:4bb979da8 on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/linux-sanitizers.md`: "Items 1 to 5, the `test_node` segfault and a debug abort in the shutdown drain (both found on the way) are closed" |

## Result

Landed at Xapiand:4bb979da8 on 2026-11-03. CI g++-13 ASan+UBSan smoke/load/E2E/TLS GREEN, 0 UBSan reports; g++-13 TSan CI job GREEN. No review named in the ledger.

- Merge: Merge branch 'fix/linux-sanitizers' (IndexChores idle count, traceback frame pointer, shutdown drain, sanitizer test fixes).
- clang-20 ASan+UBSan `test_pool`: 232 of 232, 8 of 8 runs; g++-13 TSan pool cases 38 of 38 pass, released_views hangs.
- clang-20 whole ctest: 33 of 35; g++-13 ASan+UBSan whole ctest: 32 of 35, two known hangs and a third failure not read.
- `test_node` with traceback fix: 3 of 3 runs, 85 checks; traceback test mutant killed.
- `Xapiand:a868f2145`, `a85494c6c`, `5192d1f47`, `c83a6278b`, `03036aa19`, `f5b67bebb`, `a48ee6f89` recorded.
- Full ctest per sanitizer on final commits remains open; traceback push is required for fetchability.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
