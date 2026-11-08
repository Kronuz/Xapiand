# prospect-phase7c-l4: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-phase7c-l4.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:c77d6eb on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l4.md`: "[x] S1 Identity: `sliced_test` compares the sliced search (slice 0 ns = every checkpoint, 3 us, one hour)" |
| A2 | `docs/status/phase7c-l4.md`: "[x] S2 Yields: at a 0 ns slice, reading every posting resumes >= 0.9 of the windows" |
| A3 | `docs/status/phase7c-l4.md`: "[x] S3 Cancellation: after 3 resumes the token is set; the search ends at the next checkpoint" |
| A4 | `docs/status/phase7c-l4.md`: "[x] S4 Mutants: 17 in `test/sliced_mutants.py`, one per suspension point" |
| A5 | `docs/status/phase7c-l4.md`: "[x] S5 Sanitizers on the head `prospect:5c29324` code: ASan+UBSan (Homebrew clang 23, -O1) clean" |
| A6 | `docs/status/phase7c-l4.md`: "[x] S6 No-yield overhead, `quiet strict` (QUIET_PAUSE), Intel Mac, Release" |
| A7 | `docs/status/phase7c-l4.md`: "[x] S7 `cpp-libraries/tools/preflight.sh` on the worktree: `mac-hardened` ok" |

## Result

Landed at prospect:c77d6eb on 2026-10-30. task pinned at task:9a85e89.

- First no-yield A/B +4% on multi-term queries; loops moved to plain locals (prospect:5c29324), +2%; residual cause open at landing.
- Review: none recorded in prospect (the ledger ends in review); `docs/reviews/phase7c-design.md` names no L4 finding.
- Docs: `docs/mechanisms/sliced-search.md`, `docs/perf/sliced-search-residual.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
