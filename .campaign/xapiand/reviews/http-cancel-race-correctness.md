# http-cancel-race review: correctness (concurrency)

Commit: http:473bbc32a4da7f9484c80b7fe7fe5fb16a8f27c0
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261112-035947-754791 (build-test, Release, macOS, at 473bbc3): exit 0, 13 of 13 ctest, `readahead` passed,
  so the new `[U]` window case passes, including its `took` precondition (the test's own wait ran
  first and took the byte, so the watch's handler really woke to `EAGAIN`).
- 261112-035948-060f09 (`--fails-without`, `sh test/watch_mutants.sh build-mutants cancel-race`): exit 0.
  The script (test/watch_mutants.sh:33-34) counts the mutant killed only when `FAIL: [U] a cancel between
  the wake and its handler ends the read with operation_aborted` is in its log; a hang hits `timeout 120`
  without that line and counts as survived (exit 1). The log shows that exact line, then
  "unmutated: readahead_test passes". This meets the standard: exit 0 only when the test fails on its own check.
- 261112-035951-01fc5f: all six watch mutants killed, unmutated passes (the `no-cancel` fragment was
  updated to the new `Cancel{&input_}` and still kills on "its requested token ends the wait").
- GitHub CI run 43213906039 at 473bbc3: success on linux-tsan, linux-asan-ubsan, linux x86_64 gcc and
  clang, linux arm64 gcc, freebsd-x86_64, macos-x86_64, macos-arm64. That covers epoll and kqueue for the
  test's same-pass FIFO assumption, and TSan for question 2.

## What I checked myself

Read-only; no probe job was needed. Scripts in `.scratch/review-http-cancel-race/`:
`http_asio_473.h` (the header at the commit, for the line numbers below) and `aa_ratio.py`
(recomputes the A/A column from 261109-150350-f39012's `runs/`; rerun with
`python3 -I aa_ratio.py ~/.scratch/campaign/jobs/261109-150350-f39012/a1/out/watch-ab/runs`).

1. The window on every path (http_asio.h at 473bbc3):
   - Cancel before the wait: `cancel_callback` runs `Cancel` inside the constructor when the token is
     already requested (task.hh:1067-1083), after `wait_for_read` (http_asio.h:527-528). The mark is set
     and `socket.cancel()` aborts the pending `async_wait`; asio posts the abort, it never runs it inline.
     If the wait had already completed and its handler was queued, `readable()` sees the mark (http_asio.h:617).
   - During the wait: `socket.cancel()` aborts it, `readable(operation_aborted)` then `completed()`. As before.
   - After `EAGAIN` re-arm: the re-armed wait is pending, so a later cancel aborts it. A cancel in the
     wake-to-handler window is caught by the mark. Everything runs on the loop, so no window remains between the check and `arm()`.
   - Cancel racing data or EOF: the handler completes with the data or EOF, as a read that completed
     before the cancel would. The callers handle both: `settle_read` maps only `operation_aborted` to
     `timed_out`, and the feed's `handler_done` path (http_asio.h:988-999) takes data or EOF safely.
   - Mark leak: `waiter_` is set only in `wait_for_read`, which clears `cancelled_` (http_asio.h:573).
     `readable()` honours the mark only with a waiter set (617). The one stale case is a cancel that fires
     after `completed()` posted the resume but before `await_resume` resets `cancel_`. It sets the mark
     with no waiter, and the next `Read` or `Drain` clears it before it can count. `~Read` deregisters
     `cancel_` (member destruction) after `forget()`. The test's follow-up reads (3000 bytes in order after the
     cancel) exercise reuse of the input.
2. The plain-bool ruling holds. The tokens reaching `ConnectionInput::read` are `stop`, a deadline
   linked to it, and `feed_stop` linked to it (http_asio.h:879, 929, 953, 988, 1047). `stop` is
   `SessionControl::stop` (http_asio.h:1112). That control's `abort()` (reactor.h:381-384 at ab69cf2) runs
   from `abort_all` through `on_loop_sync`, which runs on the loop or on the caller only when no thread
   runs the loop (reactor.h:243, 276). `track()` asserts the loop (reactor.h:232). Deadlines fire from an
   `asio::steady_timer` on the scheduler's executor (task_asio.hh:130-135), and the scheduler is the
   connection's own (`reactor->scheduler()`). `feed_stop.request()` (http_asio.h:1020) runs in `run_handler`
   after `co_await handler->handle(...)`, and a task's awaiter resumes on its own scheduler (task.hh:544-545),
   which is the loop. Linked sources request children on the requesting thread. `Request::cancel` (shutdown,
   disconnect) never reaches this read. The pre-existing `socket.cancel()` in the same callback already
   needs the loop, and CI's linux-tsan is green.
3. `asio_park::resume` (task_asio.hh:204-216 at bdafa5f) is the function `await_asio`'s `drop()` calls,
   so the nesting bound (`inline_resume_bound` = 4, the same thread_local depth) and the in-place rule are
   identical, not copied. A null scheduler goes to `resume_on`, which resumes in place, as the old code did.
   Lifetime: `h` and the scheduler are copied out (`std::exchange`) before the resume, nothing touches `this`
   after it, and the handler's `owner` shared_ptr keeps the input alive. The only change from the old copy is
   the added depth bound, which is stricter.
4. The N2 and N4 fixes: `aa_ratio.py` reproduces the new A/A column exactly (0.96-1.00, 0.93-1.05, 0.82-1.10,
   1.00-1.24, 0.96-1.03, 0.98-1.05). The run.log shows the first series rotating by round (a, aa, base, e1, a, ...),
   so ruling N4 out and leaving bench/watch_ab.sh unchanged is correct.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | no | gap | Pre-existing, outside A1's plain-TCP scope: on a TLS connection the same window still loses a held read's cancel. `kLazyWatch` is false for `ssl::stream`, so the watch is `ssl::stream::async_read_some` (http_asio.h:606), and the mark is consulted only in `readable()`. When the lowest-layer read completes with part of a record and the cancel runs before the `io_op` continuation, `socket.cancel()` finds nothing. The `io_op` checks only its own per-operation cancellation state and re-arms another lowest-layer read (asio ssl/detail/io.hpp:260-276). The comment at http_asio.h:524-525 and watch-peek.md step 2 state the guarantee without that limit. Reasoned from source, not probed. | http_asio.h:606, 617; asio io.hpp:260-276 | The owner's call, as a follow-up quest. Options: (a) bind a cancellation slot to the TLS watch read, emitted by `Cancel`, which the `io_op`'s `cancelled()` check then sees, as `task::await_asio` does for the direct path; (b) have `Cancel` also post a second `socket.cancel()` to the loop, which runs after the queued continuation re-armed; (c) keep it and limit the doc claim to plain TCP. |
| C2 | no | nit | The unchanged after/before ranges in watch-peek.md are inverted from the report's B/A, so three ends differ from per-round a/base in the last digit: 1.98 against 1.97, 1.11 against 1.12, 0.89 against 0.90. The verdicts are unaffected. | aa_ratio.py output | Optional: print the ranges from a/base directly. |

## Not run

- No TSan build of my own: question 2 was settled from the code, with CI linux-tsan green at 473bbc3.
- No TLS reproducer for C1: the claim rests on asio's `io.hpp`, not on a test.
- No new test on Linux epoll: I relied on CI for the test's ordering assumption there, since the
  build-test job ran on macOS only.
