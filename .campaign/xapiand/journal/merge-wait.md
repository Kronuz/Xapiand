# merge-wait: journal

State: landed
Quest: .campaign/xapiand/quests/merge-wait.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:6e13416 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/merge-wait.md`: "The wait is deleted; `merge_memory_wait` is removed from the options (and from the merge's environment)." |
| A2 | `docs/status/merge-wait.md`: "Revert check: main's `index.hh` under the new tests fails `budget_below_inputs` and `memory_held`" |
| A3 | `docs/status/merge-wait.md`: "Suites, all 19 tests: RelWithDebInfo pass (28 s); TSan (Homebrew LLVM 23, -O1) pass; ASan+UBSan" |

## Result

Landed at strata:6e13416 on 2026-10-20. The merge previously slept 5 s per round when source-file-byte estimates exceeded budget capacity. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/phase4/merge-wait'.
- At a 16 KiB budget over about 72 KB of inputs, old code slept 500 times inline and 395 times on pool.
- With the fix: 0 sleeps, well under 1 s.
- All 19 tests pass in RelWithDebInfo, TSan, and ASan+UBSan.
- fillrandom 2x settle median: 15.28 s to 0.25 s, 62x shorter.
- ycsb_f 2x settle median: 9.79 s to 0.35 s, 28x shorter; fits control unchanged.
- CI run 39516877171 was in progress on eight jobs when recorded.
- Docs: `docs/design/phase-3.md`, `docs/design/reordering.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
