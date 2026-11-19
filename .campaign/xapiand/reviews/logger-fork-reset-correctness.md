# logger-fork-reset review: correctness

Commit: logger:dc571a0
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Landing still needs A3 (suite 261119-142949-6501eb, TSan 261119-142950-24d36d, CI on Linux
gcc/clang/arm64, macOS arm64/x86_64 and FreeBSD); this review does not stand in for them.

## Evidence I relied on

- 261119-142946-f38e36 (macOS, dc571a0): `logger_exit_test fork` passes both `child-fork` and
  `child-fork-held`. The test checks values: the parent writes exactly `1 + nudges + 100` lines and
  `pending_count() == 0` after the fork (A2's parent side).
- 261119-142948-f94cd1 (mutant logger:3ed8ec2, the `pthread_atfork` call replaced by a no-op):
  fails for the right reason, "the fork child blocked on the LOG thread's mutex (killed after 5 s)".
- The CI matrix (cpp-libraries `.github/workflows/cmake.yml`, freebsd/tsan/sanitize default on)
  runs `logger_exit_fork` on glibc/libstdc++, macOS libc++ and FreeBSD libthr: it is the
  per-platform proof for risk 1, pending.

## What I checked myself

Reading settled every named risk; I submitted no probe job and kept no scripts.

1. **Byte reset per platform.** `std::mutex` and `std::condition_variable` take no attributes: on
   libc++ and libstdc++ both are built from `PTHREAD_MUTEX_INITIALIZER` / `PTHREAD_COND_INITIALIZER`,
   so a robust, error-check or non-static mutex cannot reach `LogWorker::mtx`/`cv`
   (logger.cc:961-962), and the reset writes exactly a fresh object's bytes. glibc: zero bytes; a
   non-robust mutex is on no robust list, so nothing else refers to the old state. macOS: the
   signature word goes back to `_PTHREAD_MUTEX_SIG_init`, initialized lazily on first lock; psynch
   kernel state is per process, so the child has none. FreeBSD: the objects are pointers reset to
   NULL; the child's first lock allocates a fresh one through libthr's static-init path (the parent's
   struct leaks, one per fork). POSIX leaves re-initializing an initialized mutex undefined; this is
   the established after-fork practice and the only option in a child handler. On libstdc++ the
   condition variable's constructor is out of line in libstdc++.so but only stores the initializer,
   so the CHANGELOG's "calls no libc function" holds.
2. **Other state the child reaches.** `worker_state` is frozen at the fork: the child's `wake_for`
   clears `kSleeping` and notifies a fresh `cv` with no waiter. `kClosed` is set only after
   `kStopping` (`close()` runs only after the stop or the abandon, which follows `end()`), and both
   producer paths refuse on `kStopping` before publishing (logger.h:790, logger.cc:1368), so the
   child never reaches the `taken()` spin on `final_done` (logger.cc:1351-1354). The exit-time
   stop returns on `getpid() != pid_` (logger.cc:1273). `RingRelease` at exit takes no lock
   (logger.cc:917-924). `finish(n)` goes through workpool's `Thread::join`, which waits on the
   promise's future with a timeout and never joins a `std::thread` (`run_thread` detaches, workpool
   thread.cc:94), so nothing joins a thread that is not there.
3. **Registration.** `pthread_atfork` is called inside the function-local static's initializer
   (logger.cc:1263-1265): once per process. `w->run()` at logger.cc:1266 is the only start; nothing
   restarts the LOG thread. Handlers are inherited across `fork`, so grandchildren reset too. The
   handler is registered before `run()`, so every fork after the LOG thread exists runs it. A fork
   before first use leaves the static uninitialized, and the child starts its own LOG thread, with
   its own `pid_`, on its first line. The one gap is a fork during that initialization (N4).
4. **docs/fork.md's contract for a child that does not exec.** Accurate in what it states: lines
   are queued and not written, the child's exit does not wait, `finish(0)` returns at once,
   `finish(n)` times out false. "A severe one is written inline" also holds: `LOG` makes priorities
   above `LOG_ERR` synchronous (logger.h:952, logger.h:970). It omits the queue's unbounded growth
   (N2) and two of the mutexes an inline write can block on (N1).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | "What it does not cover" names two mutexes but misses workpool's `thread_names_mutex`, which `hooked_thread_name` takes for each line written with `with_threads`, and handlers' own locks. It also implies the `once` filter is a producer's lock, but the LOG thread holds it while it writes a `once` line. A child forked during such a write blocks on its first inline line (synchronous severe, or refused). | docs/fork.md:45-50; logger.cc:1907-1909, logger.cc:1922; workpool thread.cc:150 | Name both locks and handler locks, scoped to inline writes in the child. |
| N2 | no | gap | A child that keeps running without exec queues every async line (ERR and below) forever. `ring_room` doubles the buffer without bound and nothing drains it, so a long-lived prefork worker grows memory without limit and loses those lines. The doc says "queued and never written" but not the growth. | logger.cc:1543-1556; docs/fork.md:36-38 | Owner's call: (a) keep, and add a sentence on the growth; (b) also set `kStopping` in the child handler so every line in the child is refused and, when due, written inline (logger.cc:1853-1858; refused routine deferred lines are then lost); (c) start a LOG thread in the child on first use (threads after a fork). |
| N3 | no | gap | `child-fork-held` holds only the mutex: the hook runs before `cv.wait`, so the `cv` reset has no failing test. A mutant that keeps the mutex reset and drops the `cv` reset survives. The pre-existing `child-fork` forks with the LOG thread idle in `cv.wait` and passed before this fix, so on current libcs the `cv` reset is defense in depth. The doc's reason 2 (glibc's per-group counts "a signal can wait on") is version-dependent: glibc 2.41 removed `__g_refs`. | logger.cc:1461-1470; test/exit.cc:282-298; docs/fork.md:22-24 | Keep the reset; soften reason 2 to a defensive reset, or add a hook inside the wait predicate to force it. |
| N4 | no | nit | Pre-existing: a fork while another thread is inside `get()`'s first-use initialization leaves the static's guard pending in the child, whose first line blocks in `__cxa_guard_acquire`. The window is the LOG thread's creation. | logger.cc:1255-1268 | Note it in docs/fork.md. |
| N5 | no | nit | `pthread_atfork`'s return value is ignored. On ENOMEM the reset is silently not registered. | logger.cc:1265 | Check the result, or accept and say so. |
| N6 | no | nit | `finish(n>0)` in a child waits on the promise's shared state. On libc++ its own mutex and `cv` are not reset, so a child forked while a parent thread was inside `Logging::join`/`finish`'s wait can block. The doc's "waits 2n*100 ms and returns false" assumes no parent joiner in flight. | workpool thread.hh:151-159; docs/fork.md:39 | Add the caveat to docs/fork.md. |
| N7 | no | nit | TSan cannot speak for the child. After a multithreaded fork, compiler-rt's `ForkChildAfter` ignores all synchronization and suppresses reports in the child, so the TSan gate covers the parent side (A2) only. | compiler-rt tsan `ForkChildAfter` | None; know what the gate proves. |
| N8 | no | nit | docs/fork.md is linked from nowhere. ARCHITECTURE.md mentions the fork child but not the new doc. | ARCHITECTURE.md:127 | Link it. |

## Not run

- No probe jobs: no Linux, glibc-version or FreeBSD runs of my own. Those platforms' proof is CI's
  `logger_exit_fork`, pending at review time, as are suite 6501eb and TSan 24d36d.
- I did not mutate the `cv` reset alone (N3), and I did not measure the queue's growth in a
  long-lived child (N2).
- I did not review the rest of `test/exit.cc` or the other logger tests beyond the fork modes.
