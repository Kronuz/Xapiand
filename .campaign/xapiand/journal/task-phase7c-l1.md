# task-phase7c-l1: journal

State: landed
Quest: .campaign/xapiand/quests/task-phase7c-l1.md (the lead's; never edit it)

## Doing

Nothing: landed at task:9a85e89 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l1.md`: "[x] `event`, `async_mutex` with `lock_until`/`lock_for`, `await_asio`, per-resume slice, `demoted()`" |
| A2 | `docs/status/phase7c-l1.md`: "[x] tests for every primitive: `test/sync_test.cc` (slice, event, mutex, lock_until orders, asserts)" |
| A3 | `docs/status/phase7c-l1.md`: "[x] a mutant per claim, killed (below)" |
| A4 | `docs/status/phase7c-l1.md`: "[x] ASan+UBSan and TSan builds clean, Homebrew LLVM 23 (`/usr/local/opt/llvm`): all three test binaries" |
| A5 | `docs/status/phase7c-l1.md`: "[x] R8: `test_lock_until` cases A (timeout first) and B (grant first, timer completion queued) run ASan-clean" |
| A6 | `docs/status/phase7c-l1.md`: "[x] `cpp-libraries/tools/preflight.sh`: mac-hardened, mac-asan-ubsan, linux, freebsd all ok" |

## Result

Landed at task:9a85e89 on 2026-10-29. Implementation and tests task:60b386c, docs and CI trigger task:e8349d8, late-timer test task:9887305, then task:9a85e89 (completer join, saturated `lock_for`).

- Mutants m1-m10 killed; preflight green on mac, linux and freebsd (one FreeBSD rerun for a missing `<csignal>`).
- Review: `docs/reviews/phase7c-l1-code.md` -- APPROVE WITH CONDITIONS. The ledger still reads in review: it was not updated after landing.
- Branch CI: not recorded in the ledger (the lead watched it).
- Docs: `docs/mechanisms/async-mutex.md`, `ARCHITECTURE.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
