# task-phase7c-l1: Task primitives for Phase 7c (L1)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at task:9a85e89 (2026-10-29); its ledger `docs/status/phase7c-l1.md` in Kronuz/task is the record. The quest's repository is `../task`. Its machine state is in `state.json`: `campaign status task-phase7c-l1`.

## Outcome

Kronuz/task gains the primitives Phase 7c's step L1 names: `event`, `async_mutex` with `lock_until` and `lock_for`, `await_asio`, the per-resume slice, `demoted()`, and destructor assertions on event, mutex and `asio_scheduler`, each tested and each claim killed by a mutant.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `event`, `async_mutex` with `lock_until`/`lock_for`, `await_asio`, per-resume slice, `demoted()`, destructor asserts in `task.hh` and `task_asio.hh`: `docs/status/phase7c-l1.md`
- A2 [artifact] Tests for every primitive: `test/sync_test.cc` and `test/asio_test.cc` (slice, event, mutex, lock_until orders, asserts, demoted, await_asio, real timers, 4-reactor contention): `docs/status/phase7c-l1.md`
- A3 [artifact] A mutant per claim, killed (m1-m10; m7 survived the first tests and died after case B gained `ms.ready() == 0`): `docs/status/phase7c-l1.md`
- A4 [artifact] ASan+UBSan and TSan builds clean with Homebrew LLVM 23 on all three test binaries: `docs/status/phase7c-l1.md`
- A5 [artifact] R8: `test_lock_until` cases A and B run ASan-clean; the frame-owned block reports use-after-poison in `unlock()` (m1): `docs/status/phase7c-l1.md`
- A6 [artifact] `cpp-libraries/tools/preflight.sh`: mac-hardened, mac-asan-ubsan, linux and freebsd ok: `docs/status/phase7c-l1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Design: Xapiand `docs/design/phase7c.md` D2, D3, D5, D14, D16 and section 5 L1; R2 closed and R8 fixed here, C7 untouched (`docs/reviews/phase7c-design.md`).
- Every queued waiter, timed or not, is one pool-allocated refcounted `lock_waiter`; settling is one compare-exchange (ledger, Design points).
- Risk high: concurrency primitives every 7c consumer awaits on (the auditor's mapping).

## May land early

None.
