# logger-fork-reset: journal

State: waiting on jobs 261119-142949-6501eb 261119-142950-24d36d
Quest: .campaign/xapiand/quests/logger-fork-reset.md (the lead's; never edit it)

## Doing

A1 done: the fix passes, the mutant (logger:3ed8ec2, `mutant/fork-no-reset`) fails with "the
fork child blocked on the LOG thread's mutex (killed after 5 s)". Suite and TSan at logger:dc571a0
are queued behind other holds.

## Next

1. When suite 261119-142949-6501eb and tsan 261119-142950-24d36d pass: fill A2/A3 rows; State "ready for review".
2. CI on all platforms (A3) needs the quest branch pushed; the lead's prompt says not to push.

## Blocked on

Nothing. A3's CI half waits for the lead to push `quest/xapiand/logger-fork-reset`.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261119-142946-f38e36; fails without: 261119-142948-f94cd1 (hang, killed after 5 s) |
| A2 | 261119-142946-f38e36 (parent writes all 100 post-fork lines, pending 0); TSan pending: 261119-142950-24d36d |
| A3 | pending: suite 261119-142949-6501eb, tsan 261119-142950-24d36d; CI after push |

## Result

logger:dc571a0. `LogWorker::get()` registers a `pthread_atfork` child handler,
`after_fork_child`, that placement-constructs `mtx` and `cv` anew in the child. A child forked
while the LOG thread held `mtx` with `kSleeping` set no longer blocks in `wake_for` (its first
line) or `end()` (`finish()`). The child's lines are still queued and not written (unchanged).
How it works and the contract for a child that keeps running without exec: logger
`docs/fork.md`. Test: `test/exit.cc` `child-fork-held` (ctest `logger_exit_fork`), forced with
`logger_detail::sleep_hook` (a test-only atomic function pointer the LOG thread calls on its sleep
path with `mtx` held; one relaxed load per sleep). Mutant: `test/fork_mutant.sh`, and the branch
`mutant/fork-no-reset` for the `--fails-without` job.

Leaves open (docs/fork.md, "What it does not cover"): `shared_ring_mutex` and the once filter's
mutex have the same fork hazard and guard data, so they would need a prepare/parent/child lock,
not a reset; neither is held by the LOG thread across a sleep.

## Rulings I made

- Reset in the child, not a prepare lock. `mtx` guards no data (the state is atomics; it pairs
  with `cv` only), so a fresh one loses no invariant; `cv` must be reset anyway because the LOG
  thread waits on it at most forks, and glibc's condvar group counts can make a signal wait on a
  phantom waiter; a prepare lock makes every host fork wait on the LOG thread and deadlocks a fork
  from a thread holding `mtx`. The handler only stores the static initializers (no libc call), so
  it is async-signal-safe in practice.
- Registered at LOG thread creation (`get()`), before `run()`: before that no thread can hold `mtx`.
- The test lives in `test/exit.cc` (`fork` mode now runs `child-fork` and `child-fork-held`)
  because `CMakeLists.txt` is outside the quest's file set; no new ctest entry was needed.
- No doc change to ARCHITECTURE.md / AGENTS.md (outside the file set); docs/fork.md carries it.
  The lead may want a one-line pointer from ARCHITECTURE.md's exit paragraph.
- The child-fork-held child calls `finish(0)` and `_exit(0)`: `finish(0)` covers `end()`'s lock;
  `_exit` because the nudger is a joinable `std::thread` in the child.
