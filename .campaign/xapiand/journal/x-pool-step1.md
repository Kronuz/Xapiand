# x-pool-step1: journal

State: landed
Quest: .campaign/xapiand/quests/x-pool-step1.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:4f873c0e2 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x-pool.md`: "[x] `docs/design/x-pool.md`: decisions, gate rows, the rulings on F1 to F6 (revision 2, L2, XH8)." |
| A2 | `docs/status/phase7c-x-pool.md`: "[x] Step 1, the seam: `IndexPin` over today's `refs`, `lock_shard` awaited (`co_await lk_shard.pin()`)" |
| A3 | `docs/status/phase7c-x-pool.md`: "[x] Step 1 proof on Linux: see Proof." |

## Result

Landed at Xapiand:4f873c0e2 on 2026-11-02. macOS pool tests: 231 of 231; reactor requests: 342 of 342. No review named in the ledger.

- Merge: Merge branch 'phase7c/x-pool' (step 1: IndexPin seam, pin before view, resident chore pins).
- `Xapiand:0d4c7e629` adds `IndexPin`, `pin`, `pin_chore`, awaited `lock_shard`, view check and tests.
- `Xapiand:24c0e2f10` adds `lock_shard::pin()` and checker row; `Xapiand:158a3b14a` adds resident chore pin and mechanism notes.
- Linux TSan docs E2E GREEN; ASan+UBSan docs E2E GREEN; no result differs from master.
- Linux sanitizer runs expose pre-existing master failures in `BlockingScheduler`, pool tests and path-boundary checks.
- Steps 2-4 and wide-search integration remain open.
- Docs: `docs/design/x-pool.md`, `docs/mechanisms/index-registry-and-lease.md`, `docs/mechanisms/reader-views.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
