# flush-memory-bound: journal

State: landed
Quest: .campaign/xapiand/quests/flush-memory-bound.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:fb583d8 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/flush-memory-bound.md`: "Records go from 16 KiB to 64 KiB so a quarter of them (9.8 MB) clears every run's scratch twice over" |

## Result

Landed at strata:fb583d8 on 2026-10-26. Before fix, test failed 10 of 300 runs at 32 concurrent; 0 of 200 at 12 and 0 of 100 at 4. No review named in the ledger.

- Merge: Merge branch 'fix/flush-memory-bound'.
- `strata:f0ba032`: test fixture change and this ledger.
- After fix, 0 of 600 runs at 32 concurrent; both variants pass with leak detection.
- Removing `borrow_stored(lease)` makes the new test fail in both thread variants.
- No code change; CI run on push was not watched.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
