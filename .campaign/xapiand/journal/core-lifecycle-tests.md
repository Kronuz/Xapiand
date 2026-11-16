# core-lifecycle-tests: journal

State: landed
Quest: .campaign/xapiand/quests/core-lifecycle-tests.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:8713d2b on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/core-lifecycle-tests.md`: "`b3c0ea9` six programs on `basic_index<document_layout>`" |
| A2 | `docs/status/core-lifecycle-tests.md`: "Mutants (each in a core header, test rebuilt, reverted after)" |
| A3 | `docs/status/core-lifecycle-tests.md`: "Local totals (macOS x86_64): 19 tests pass in RelWithDebInfo, under ASan+UBSan" |

## Result

Landed at strata:8713d2b on 2026-10-20. Local macOS x86_64 totals: 19 tests; Linux adds directory_tmpfs (20). No review named in the ledger.

- Merge: index_test: stop allocating a 4 GiB key.
- strata:b3c0ea9 ports lifecycle tests and helpers.
- strata:14cf51e updates live strata_engine references to prospect_engine.
- Six documented mutants fail their corresponding tests.
- The 4 GiB max-term test was dropped after peaking at 12.9 GB; current test RSS figures are recorded in the ledger.
- CI run id and final result were not recorded; landing remains open.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
