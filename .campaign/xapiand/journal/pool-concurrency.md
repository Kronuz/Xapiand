# pool-concurrency: journal

State: landed
Quest: .campaign/xapiand/quests/pool-concurrency.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:2466dac5c on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/pool-concurrency.md`: "[x] K1 Reproduced on master (below, 2026-10-25): UAF in round 1 of `uafhunt.sh` (4 of 4 runs)" |
| A2 | `docs/status/pool-concurrency.md`: "[x] K2 Each fix has a failing-before test; revert checks and mutants in the table below." |
| A3 | `docs/status/pool-concurrency.md`: "[x] K3 UAF hunt clean for 30 rounds (master: round 1, so 30 times); lost wake-up: deterministic" |
| A4 | `docs/status/pool-concurrency.md`: "[x] K4 TSan (Homebrew LLVM 22): `test_pool` 3 runs, 0 warnings, 12 checks; docs E2E GREEN on" |
| A5 | `docs/status/pool-concurrency.md`: "[x] K5 Two bullets under `[Unreleased]` / Fixed." |

## Result

Landed at Xapiand:2466dac5c on 2026-10-26. UAF hunt: 30 rounds, 0 ASan reports; master reproduced in round 1. Review: `docs/reviews/pool-concurrency-code.md` -- APPROVE WITH CONDITIONS; re-check CR1 and CR2 CLOSED.

- Merge: Merge branch 'fix/pool-concurrency'.
- Xapiand:cd5f7b44e -- tests; 28f6b88ff -- pool fixes; df87f4e1c -- reopen fix.
- Xapiand:b8a286309 -- changelog and ledger; 041187739 -- clear wake-up tests.
- Lost-wakeup hammer: master 6 of 30 rounds; fix 0 of 30; mutant M8 7 of 30.
- Reopen counts: master 121 writable / 286 readable hits; fix 11 / 25.
- TSan: 3 runs, 0 warnings, 12 checks; CR1/CR2 re-check CLOSED.
- Open: shutdown `clear()` with concurrent references; committer endpoint lifetime; clear wake-up mutants lack a deterministic seam.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
