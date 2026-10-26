# pool-concurrency (CR8)

Branch `fix/pool-concurrency` from Xapiand master `c625977e7`, worktree
`Xapiand-wt/pool-concurrency`. Source: `docs/reviews/engine-core.md` CR8. Concurrency tier: code
review before the lead merges. Commits stay local.

## State

- State: in progress (reproducing on master).
- Next: ASan build of master, reproduce the three bugs, then fix.
- Blocked on: nothing.

## Done criteria

- [ ] K1 Each bug reproduced on master (command and output), root cause with file:line.
- [ ] K2 Each fix has a test or repro that fails before and passes after (revert check).
- [ ] K3 UAF repro clean under ASan for 3x the iterations it took on master; lost wake-up has a
  deterministic or stress test; reopen succeeds in one pass.
- [ ] K4 TSan (Homebrew LLVM) on the pool tests; docs E2E passes as on master.
- [ ] K5 Changelog bullet under `[Unreleased]` if user-visible.

## Log

### 2026-10-25

- Read CR8. Copied `uafhunt.sh`, `hammer.sh`, `stallcount.sh`, `node.sh` and `uaf-8987.log` from
  the main checkout's `.scratch/engine-core-review/` into `.scratch/`.
- Added `tests/test_pool.cc` (deterministic wake-up tests and a concurrent-cleanup stress test) and a
  `PoolTestAccess` friend in `src/database/pool.h`.
