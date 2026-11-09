# layers-refactor: journal

State: landed
Quest: .campaign/xapiand/quests/layers-refactor.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:3e3f8b9 on 2026-10-18 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/layers-refactor.md`: "Steps 0 to 4b: 9bae9c8 (0), 0707741 (1), 97d0ae6 (2), 731918d (3)" |
| A2 | `docs/status/layers-refactor.md`: "Step 7a (F13-R11, L6): d911396. `merge_segments(sources, id, sink, spill, const merge_options&)`" |
| A3 | `docs/status/layers-refactor.md`: "Revision 4 (the re-check of revision 3, APPROVE WITH CONDITIONS: R12-R19, C5-C8), one commit per finding" |
| A4 | `docs/status/layers-refactor.md`: "05d9aac (LC1, LC5-LC10). At 05d9aac, macOS x86_64 Release: ctest 34/34 (merge_oracle gone)" |

## Result

Landed at strata:3e3f8b9 on 2026-10-18. At 05d9aac, macOS x86_64 Release ctest 34/34. Review: `docs/reviews/layers-code.md` -- APPROVE WITH CONDITIONS; `docs/reviews/layers-design.md` -- APPROVE WITH CONDITIONS; `docs/reviews/reordering-design.md` -- verdicts P1-R4 (d), P1-R17; `docs/reviews/system-2026-10-17.md` -- verdict not stated.

- Merge: design, ledger: the landing review's text items (R20, R22, R25).
- strata:9bae9c8 through 1265123 deliver steps 0-4b; strata:a993b68 through 8080e84 deliver step 5.
- strata:6681f75 delivers step 6; strata:d911396 delivers step 7a; strata:a86610e delivers step 7b.
- strata:05d9aac folds code-review conditions LC1, LC5-LC10.
- 20k corpus's 15 states byte-identical to main.
- LC4 optional "one spill, two ways in" item is not done; CI green at final tip remains required.
- Docs: `docs/design/layers.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
