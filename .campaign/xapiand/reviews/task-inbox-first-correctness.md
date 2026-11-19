# task-inbox-first review: correctness

Commit: task:aa92196dc9d5c15fe6395b9b92bbe288ffe28608 (aa92196)
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The code closes R1, R2, R4 and N8 as the quest file says. The two blocking rows are doc-only:
two documents still describe the old order and the old off-loop rule.

## Evidence I relied on

- Suite 261119-112703-f5c785 and TSan 261119-112704-fbb549 at aa92196 (A5's local half; CI was
  still running at review time).
- Inbox probe 261119-112658-04ea69: the cross-thread call runs at step 10 (A1).
- Mutants: r1 261119-112653-358a42, r2 b9569c, r4 c20d84 (TSan race). Each fails without its fix.
- The owner test passes 20 times under TSan: 261119-112657-ca44c1 (A3).

## What I checked myself

Scripts are in `.scratch/xapiand/review-task-inbox-first-correctness/` (`run.sh deadlock`,
`run.sh bench`).

1. **Inbox cost and fairness.** Each step now does one acquire load of `inbox_waiting_`
   (`task.hh:456`, `:481`). It takes the lock only when the flag is set. Measured on macOS arm64
   (strict job, beside four light holds), probe 261119-113513-6dd707 gives these best-of-7
   ns/step figures:

   | Path | Old | New |
   |---|---|---|
   | Ready | 10.3, 11.1, 9.7 | 12.5, 12.9, and one noisy 32.9 |
   | Demoted | 11.2, 11.7, 12.0 | 11.3, 10.7, and one noisy 34.0 |

   Ready handles cannot starve. `take_inbox` appends the inbox behind what is already ready,
   which is FIFO. Sustained cross-thread posting keeps demoted handles waiting, exactly as
   sustained same-thread posting does. That is the class's rule ("demoted handles only when
   nothing else is ready"), so it is not a regression.
2. **The owner handoff**, by reading:
   - `owner_` is written only under `inbox_mutex_` (`task.hh:450-453`, set at `:452`), and `post_call` reads it
     under that lock (`:407-408`).
   - A past owner A posts while a new driver B starts. If B has not yet taken the lock, A's
     push to `ready_` happens under the lock, and B's lock acquire orders it before B's first
     step. If B has, A's call goes to the inbox. The same holds for a post during run's exit:
     `owner_` stays the exiting driver, and everyone else uses the inbox.
   - The `draining_scheduler == this` fast path (`:403`) is a thread-local check, so it holds
     only on the thread that is draining.
   - Two `run()` calls at once: not enforced (F4).
3. **The inline flush fallback.**
   - Reentrancy is safe. `f` runs inside `await_suspend` (`lifecycle.hh:459-461`), and the
     waiting handle is never queued, so nothing resumes the lifecycle frame early. `return false`
     resumes it, and `run()` catches every throw into `error_`. When the start fails, `thread_`
     stays unjoinable, so neither `~off_loop` nor `resume` touches it.
   - A deadlock is possible. Probe 261119-113512-cbfb48 uses a `flush()` that queues a call on
     the loop and waits up to 2 s for it, which is lifecycle_test's `loop_flush`. With the thread,
     the call runs. With a failed start, it never runs ("would wait forever"), and `failure` is
     not set. See F2 and F3.
   - The test hook compiles out. `TASK_TEST_HOOK` is `((void)0)` and the hook variable does not
     exist unless `TASK_TEST_HOOKS` is defined (`task.hh:734-750`). Only test TUs define it
     (`test/*.cc`), and no header or CMake target does.
4. **Doc edits.**
   - The edited lines in API.md, lifecycle.md, deadlines.md and service-shape.md step 1 match the
     code.
   - Two places were missed: F1 and F2.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | ARCHITECTURE.md still states R1's order: a cross-thread `post_call` "waits in a locked inbox that `run()` moves into the ready queue once it is empty". The code now moves the inbox at every step, behind the ready handles and before any demoted handle. | `ARCHITECTURE.md:37-38` against `task.hh:455-470` | Reword the sentence to match API.md and lifecycle.md. Check: no doc outside `docs/reviews/` says "once it is empty" of the inbox. |
| F2 | yes | gap | service-shape D16 still says flushes run "at the end of every path on a thread of their own". Its rejected alternative names the hazard: "a `flush()` that needs the loop would deadlock". The fallback now runs flushes on the loop, and lifecycle.md:72 says "every path still ends with them" without that caveat. The deadlock is real. | Probe cbfb48: with the start failing, the loop call never runs while the flush waits. `service-shape.md:204,267`; `docs/mechanisms/lifecycle.md:72-73` | State the fallback and its limit in D16, step 5 and lifecycle.md: when no thread starts, `flush()` runs on the loop and must not wait on it. Check: an artifact read of the three places. |
| F3 | no | gap | The fallback is silent. A failed thread start no longer reaches `stopped::failure` (before this commit it did), so a shutdown that ran flushes on the loop under thread exhaustion looks clean. | `lifecycle.hh:459-461`; `test/lifecycle_test.cc` `flush_inline` checks `!r->failure`; probe cbfb48 "failure: none" | Owner's call. Options: keep it silent (the flushes did run); record the `system_error` as `failure` when nothing else failed; or log it. |
| F4 | no | gap | The one-driver-at-a-time rule is documented (API.md, `task.hh:399`) but not enforced. Two concurrent `run()` calls each set `owner_` and then race on `ready_` and `demoted_` with no check. | `task.hh:443-470` | Optional: a debug-only assert in `run_until`, such as an atomic driver id that allows same-thread nesting. |
| F5 | no | nit | The ready path costs about 2 ns more per step (about 20%) with an empty inbox. The demoted path does not change. The data is noisy: one strict run beside light holds, macOS arm64 only. `inline_scheduler` serves tests and simulation. | Probe 6dd707 (table above) | Optional: a relaxed load in the loop plus an acquire fence inside `take_inbox`'s taken path. Re-measure exclusive if a simulator's throughput matters. |
| F6 | no | nit | A `post_call` that lands as `run()` exits stays in the inbox until the next `run()`, and `queued()` does not count it. A caller that loops on `queued()` can therefore stop with a call pending. This predates the commit. The lifecycle `drive` helpers loop on the result, so they are unaffected. | `task.hh:474` | Optional: count the inbox in `queued()`, or document it. |

## Not run

- Linux, FreeBSD and x86_64. The bench ran only on macOS arm64, and not as an exclusive hold.
- The inline fallback on `asio_scheduler`: covered by reading only, since the same `await_suspend`
  serves both schedulers.
- I did not rerun the suite, TSan, the mutants or the owner test. Their jobs are listed above.
- CI was running at review time.
