# stored-short-section: journal

State: landed
Quest: .campaign/xapiand/quests/stored-short-section.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:75716f0 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/stored-short-section.md`: "`test/format14_test.cc`: truncated stored sections of 0, 1, 2, 3, 4, 5 and 20 bytes" |
| A2 | `docs/status/stored-short-section.md`: "`fuzz/fuzz_stored.cc`: `data[1] & 4` cuts the chosen section short" |

## Result

Landed at strata:75716f0 on 2026-10-26. Fuzz replay of 599 + 25 inputs is clean; fuzz_stored ran 8 minutes, 115,516 runs, no report. Review: `docs/reviews/f14-step1-decoder.md` -- SR2.

- Merge: Merge branch 'fix/stored-short-section'.
- Main already contains the refusal at `strata:21ca549`; this branch adds coverage and fuzz reach.
- Constructor-check mutant causes 7 format-test failures and fuzz replay trap; threshold `< 4` mutant causes 3 failures.
- No format or reader change; CI dispatch remains open.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
