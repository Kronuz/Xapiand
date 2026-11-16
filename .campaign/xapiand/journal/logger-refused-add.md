# logger-refused-add: journal

State: landed
Quest: .campaign/xapiand/quests/logger-refused-add.md (the lead's; never edit it)

## Doing

Nothing: landed at logger:90094b5 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/fix-refused-add.md`: "a delay past the 24 h horizon) it undoes `scheduled` and `pending`, then by kind (owner's policy)" |
| A2 | `docs/status/fix-refused-add.md`: "`pending_count()` is exact across every refusal (0 before anything is scheduled)." |
| A3 | `docs/status/fix-refused-add.md`: "scheduler pinned to 2cbea95 (`schedule_result`); the policy goes by the line's kind" |

## Result

Landed at logger:90094b5 on 2026-10-20. Landed as logger:a48bce8 to logger:90094b5 (2026-10-20).

- Review: `review-wt/stash/docs/reviews/window-refusals-code.md` -- APPROVE WITH CONDITIONS (logger reviewed at 2c2a264; prepared by this audit).
- Docs: `CHANGELOG.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
