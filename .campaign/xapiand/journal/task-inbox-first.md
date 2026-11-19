# task-inbox-first: journal

State: waiting on jobs 261119-114206-73a222 261119-114200-848297 261119-114200-363a83
Quest: .campaign/xapiand/quests/task-inbox-first.md (the lead's; never edit it)

## Doing

Done at task:aa92196: every job passed, and each mutant failed at its own check (r1 at
deadline_test.cc:393 during.at, r2 at lifecycle_test.cc:387 !r->failure, r4 a TSan data race at
task.hh:451 against post_call :404). Not pushed (lead's instruction).

## Review fold-in (reviews/task-inbox-first-correctness.md, CHANGES at aa92196)

Fix commit task:b7e2449.

- F1: ARCHITECTURE.md:37-39 gives the per-step inbox order; no doc outside docs/reviews/ says
  "once it is empty" of the inbox.
- F2: service-shape.md step 5 and D16, and docs/mechanisms/lifecycle.md, say the flushes run
  inline on the loop when off_loop's thread cannot start, so a flush() must never wait on the
  loop. Docs only: the reviewer's deadlock.cc shows the hazard but cannot assert the rule
  without waiting, so no test.
- F3: owner's call (silent fallback); left as is.
- F4: Debug assert in run_until (atomic driver id, same-thread nesting allowed). Test
  `deadline_test two-drivers` (child process, expects SIGABRT; deadline_test undefines NDEBUG,
  being its own program). Mutant f4 (task:85819b1) fails-without 261119-114206-73a222.
- F5 (note): ready path about 2 ns/step slower with an empty inbox (reviewer's noisy strict
  bench, macOS arm64); inline_scheduler serves tests and simulation, so not pursued.
- F6 (note): a post_call landing as run() exits waits in the inbox for the next run(), and
  queued() does not count it; predates this quest; lifecycle drive loops are unaffected.
- Suite 261119-114200-848297, tsan 261119-114200-363a83 at b7e2449.

## Next

1. The lead pushes the branch for CI (A5) and launches the correctness reviewer.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | test `deadline_test inbox` (suite); mutant r1 (task:8052dc0, old order) fails-without 261119-112652-358a42; reviewer's `run.sh inbox` at head 261119-112658-04ea69 |
| A2 | test `lifecycle_test flush-inline` (suite); mutant r2 (task:3758c9d, rethrow instead of inline) fails-without 261119-112653-b9569c |
| A3 | test `deadline_test owner` (tsan check); mutant r4 (task:a30b245, relaxed owner check) under TSan fails-without 261119-112655-c20d84; 20 TSan runs at head 261119-112657-ca44c1 |
| A4 | [artifact] `docs/design/service-shape.md` step 1 reads "task: sleeps, D10, D14" (task:aa92196) |
| A5 | suite 261119-112703-f5c785, tsan 261119-112704-fbb549 (both pass); CI after the lead pushes |

Mutant commits live on local branches `mutant/task-inbox-first-{r1,r2,r4}` in task; they never land.

## Result

task:aa92196 changes `task.hh`, `lifecycle.hh`, two tests and four docs:

1. R1: `inline_scheduler::run_until` takes the inbox at every step, so a cross-thread
   `post_call` runs behind the ready handles queued when it is seen and ahead of every demoted
   one. Bound: within one step (demoted loop: at the step it was posted; ready loop: one step
   later), against step 10,000 before.
2. R4: `owner_` is a plain `std::thread::id` set by `run_until` and read by `post_call` under
   `inbox_mutex_`; the draining thread posts straight to `ready_` via `draining_scheduler`.
3. R2: `off_loop::await_suspend` returns bool; on `std::system_error` from the thread start it
   runs `f` inline and does not suspend. Test hook `test_hooks::off_loop_start`.
4. N8: service-shape.md step 1 drops `event::wait`.

Docs: `docs/API.md`, `docs/mechanisms/lifecycle.md`, `docs/mechanisms/deadlines.md` (test list).

## Rulings I made

- R1: the inbox is taken at every step, not only when `ready_` is empty, so a ready loop that
  keeps re-posting cannot starve it either. Cost: one acquire load per step on a test/simulation
  scheduler.
- R4: one thread drives at a time, the last to call `run()` owns it; the owner check moved under
  the inbox lock rather than a run counter, since the lock already exists and the draining
  thread skips it.
- R2: the fallback catches only `std::system_error` (what `std::thread` throws); a flush that
  needs the loop would block inline, which is the only option without a thread.
- A3's test makes the old code race deterministically: the first thread posts, then signals the
  driver through a relaxed flag, so nothing orders the push before the driver's read.
