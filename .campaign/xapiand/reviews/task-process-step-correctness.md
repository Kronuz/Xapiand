# task-process-step review: correctness

Commit: e68d46f7a170 (task:e68d46f)
Verdict: APPROVE (first pass); see Re-check
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. F1 is a real defect that the
port inherits from Detent; it is not blocking because the quest's criteria and Detent's behavior
are met, but the fix is two lines and a test, and cheaper here than after process-and-node adopts it.

## Evidence I relied on

- 15cba5 and suite 202326 (A1, A2): pass at d02ca88; e68d46f differs from it only in AGENTS.md and
  docs/API.md (`git diff d02ca88 e68d46f --stat`), so the code under test is the code reviewed.
- Mutants 853d1c, 098fe2, e2e328, fe16f4, cd52b7 each fail their test, so the lock, timeout, error
  and fd-flag assertions bite.

## What I checked myself

- Port fidelity: I extracted Detent:276bc58d `daemon.cc` (`locked_pid` through `run_daemon`) and
  diffed it against `process.hh` after normalizing the namespace, `Ready` to `daemon_ready`, `inline`
  and `process_detail::` prefixes: no other line differs (`diff -w`, zero semantic hunks). Detent's
  daemon.cc/.hh have not changed since 276bc58d (`git log` shows it as the last touch). Nothing is
  dropped: `setsid`, the `O_NOFOLLOW|O_CLOEXEC` opens, `high_fd`, the EINTR loops (poll, read,
  the ready write), the `kill(SIGTERM)` timeout path, the 4096-byte message cap and the log checks
  are all present. Detent never set a umask, ignored or blocked signals, or closed inherited fds,
  and neither does the port.
- Lock semantics, read from the code: `flock(LOCK_EX|LOCK_NB)` on an open file description. The
  pidfile fd is `O_CLOEXEC`, so an exec'd child never holds the lock; across a plain `fork` the
  description is shared (probe P2 below). A dead holder releases it at exit, so a stale file is
  recognised by taking the lock, with no pid liveness check and no `kill(pid, 0)` race. The PID is
  written only after the lock is held, and the refused path leaves the file untouched.
- Probe `.scratch/xapiand/review-task-process-step-correctness/{run.sh,probe.cc}`, job fb7334
  (exit 0, log `~/.scratch/campaign/jobs/261119-135843-fb7334/a1/log`):
  - P1: the daemon forks and execs `sleep 6`, then exits 5 before `ready()`. The parent reports
    "daemon startup timed out" after 3001 ms, not "daemon exited without reporting readiness".
  - P2: a `ProcessLock` taken in the parent before `run_daemon` detaches. After the parent closes
    its lock, `locked_pid` still returns the parent's PID: the child holds the lock through the
    shared description and the file names a process that is gone.
- Portability, read only: `flock`, `pread`, `poll`, `F_DUPFD_CLOEXEC`, `O_NOFOLLOW`, `O_CLOEXEC`,
  `setsid` and `std::from_chars` (int) exist on Linux (glibc, musl), macOS and FreeBSD. `pipe2` is
  not used, so no fallback is needed there (but see F1). `std::string::starts_with` needs C++20, which
  the project already requires. Nothing platform-specific lacks a fallback.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | The startup pipe's two fds are not `FD_CLOEXEC` (`pipe` has no flag, `high_fd` only sets it when it has to dup below 3). A subprocess that `run` execs before `ready()` inherits the write end, so the parent never sees EOF: if the daemon then throws or exits, the parent waits out the whole timeout (30 s default) and reports "daemon startup timed out", discarding the "E..." message it already read. Same in Detent. | probe P1, job fb7334; `process.hh` `pipe` and `high_fd` | Set `FD_CLOEXEC` on both ends after `pipe()` (`fcntl(F_SETFD)`, portable, no `pipe2`), plus a test with an exec'd child like P1. Or keep as is and document that startup must not exec before `ready()`. |
| F2 | no | gap | `ProcessLock` must be constructed inside `run` (after the fork). Built before `run_daemon`, the detached child keeps the lock through the shared description while the file names the parent's PID. The header says "Fork before constructing any threaded component" but not this; no test pins either order. | probe P2, job fb7334 | One line in the header and mechanism doc: construct the lock in `run`. Optionally a test of the two-step (`run_daemon` then `ProcessLock` in `run`) order. |
| F3 | no | gap | `locked_pid` on a stale file takes the exclusive lock for an instant. A `ProcessLock` in another process starting in that window sees `EWOULDBLOCK` and reports "held" though nothing runs. `nullopt` also does not mean "lockable": a held lock whose PID is mid-write (after `ftruncate`, before `pwrite`) returns `nullopt`. Same in Detent. This is Q5 (tools on a live directory), which binds process-and-node. | `process.hh` `locked_pid`, `ProcessLock::ProcessLock` | Nothing here. Callers must treat `nullopt` as "unknown" and take the lock to decide; process-and-node's status tools should not probe in a loop against a starting daemon. |
| F4 | no | nit | `static_cast<int>(remaining)` wraps for timeouts above about 24.8 days (negative `poll` timeout means wait forever), and `steady_clock::now() + seconds::max()` overflows. Unreachable with a 30 s default. | `process.hh` `run_daemon` poll loop | Clamp `remaining` to `INT_MAX`. |
| F5 | no | nit | The timeout path sends SIGTERM but does not `waitpid`, so the child is a zombie until the parent exits, and a child that ignores SIGTERM keeps running. A non-`std::exception` throw in `run` terminates the child. All as in Detent. | `process.hh` timeout branch | None needed for a CLI that exits right after; note for long-lived parents. |
| F6 | no | nit | The new test does not assert that the pidfile fd is closed on exec (only the mutant cd52b7 covers the flag). The `lock` test has no cross-fork assertion. | `test/process_test.cc` | Optional. |

## Not run

- Nothing on FreeBSD or Linux: the CI matrix covers those and I only read for portability.
- No NFS or other network filesystem behavior of `flock`.
- I did not rerun the suite or the mutants.
- process-and-node's use of these primitives (Q4, Q5) is outside this quest.

## Re-check

Commit: ef353dd
Verdict: CHANGES

Scope: `git diff e68d46f ef353dd`. F1 (`FD_CLOEXEC` on both pipe ends, with the `exec-child` test), F2
(header and doc line, `lock-in-run` test), F4 (clamp of the deadline and of the poll timeout) and F6
(`lock-cloexec` test) read correct. The new `waitpid` at the timeout (F5) introduced a defect.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | yes | defect | The timeout branch now does `kill(pid, SIGTERM)` then an unbounded blocking `waitpid`. A child that ignores, blocks or only flags SIGTERM (a lifecycle `stop_signals` handler installed in `run` before `ready()` does exactly this, and a hung startup never honors the flag) keeps the parent waiting for as long as the child lives, so the startup timeout no longer bounds the parent. Before the change the parent threw at the timeout. Probe: the child ignores SIGTERM and sleeps 8 s with a 1 s timeout; the parent returns after 8003 ms, and would block forever on a truly hung child. | job ca077f at ef353dd (`.scratch/xapiand/review-task-process-step-correctness/{run2.sh,probe2.cc}`) | After SIGTERM, wait a bounded grace (poll `waitpid(WNOHANG)` for about 1-2 s), then `SIGKILL` and `waitpid`; add a test with a SIGTERM-ignoring child, and a mutant that removes the SIGKILL. |

F5 as filed (zombie left behind) is otherwise resolved by the reap. I did not rerun the pass job, the
suite or the mutants (d1a5e2, 362a8a, e5c800, b42af2, af456c); I accepted them for F1, F6 and the reap.
