# stash-claim-hint-d64: journal

State: landed
Quest: .campaign/xapiand/quests/stash-claim-hint-d64.md (the lead's; never edit it)

## Doing

Nothing: landed at stash:3bfc455 on 2026-11-01 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | open: missed: M4 Pro 0.898 / 1.018, i9 0.972 / 0.963 in `docs/status/claim-hint-d64.md` |
| A2 | open: the landed fallback was not timed in a strict hold, and `CHANGELOG.md` (2026-11-01) lists stash rows below 0.97 beyond their A/A spread (structures A-spread t=8 p99 0.799 on the M4 Pro) |
| A3 | `docs/status/claim-hint-d64.md`: "`test_claim_walk`: no hint access in the first 32 slots, n - 32 reads, n / 4 - 8 publishes" |
| A4 | `docs/status/claim-hint-d64.md`: "TSan `test.cc`, `concurrent.cc` CI args x3, depth 64 x3: 0 reports" |

## Result

Landed at stash:3bfc455 on 2026-11-01. The fallback landed as stash:4187183; the ledger's last commit stash:3bfc455 (2026-11-01).

- Gate file `campaign/gate/stash.md` ("Rows < 0.97") of the cpp-libraries gate.
- Review: none.
- Docs: `CHANGELOG.md`.
- Left open: A1, A2.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
