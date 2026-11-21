# task-inbox-starvation: journal

State: waiting on jobs 261120-233748-31a460 261120-233749-515981 261120-233753-2f3547 261120-233756-445d50 261120-233759-1389f7
Quest: .campaign/xapiand/quests/task-inbox-starvation.md (the lead's; never edit it)

## Doing

Nothing to build: this quest repeats task-inbox-first, which landed at task:b7e2449 (an ancestor
of the quest's base task:a4ba197). R1, R2, R4 and N8 are already fixed on task main. Re-proving
each criterion at a4ba197 with fresh jobs and two mutants rebuilt on it.

## Next

1. Lead: decide under "Blocked on"; meanwhile record the jobs' results in Evidence.

## Blocked on

The lead: the quest is a duplicate of task-inbox-first (landed task:b7e2449, 2026-11-19), so the
branch has no commits to land. Options:

1. Close it as satisfied by task-inbox-first, citing the jobs below as re-verification at a4ba197.
2. Land it as an empty quest on these jobs.

Recommendation: 1. No code change is needed or made.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | test `deadline_test inbox` at a4ba197: 261120-233750-f790e5 (passed); mutant r1 (task:a341688, `mutant/tis-r1`: inbox taken only when ready and demoted are empty) fails-without 261120-233753-2f3547. Bound: a call posted at step 10 runs at step 10 behind a demoted loop and at step 11 behind a ready loop (one repost ahead of it); before the fix, step 10,000 |
| A2 | test `lifecycle_test flush-inline` at a4ba197 (hook `test_hooks::off_loop_start` throws `std::system_error`): 261120-233756-445d50; mutant r2 (task:1b7eab1, `mutant/tis-r2`: rethrow instead of the inline run) fails-without 261120-233759-1389f7 |
| A3 | suite 261120-233748-31a460, tsan 261120-233749-515981 at a4ba197 |
| A4 | [artifact] R4 under "Rulings I made" below; `docs/design/service-shape.md` has no `event::wait` at a4ba197 (step 1, line 296, reads "task: sleeps, D10, D14") |
| A5 | CI at a4ba197 (the quest branch, pushed) |

Mutant commits live on local branches `mutant/tis-{r1,r2}` in task; they never land.

## Result

No change. task:a4ba197 already holds task-inbox-first's fixes (task:aa92196, b7e2449):

1. R1: `inline_scheduler::run_until` takes the inbox at every step (`task.hh:468`).
2. R2: `off_loop::await_suspend` catches `std::system_error`, runs `f` inline and does not
   suspend; `ran_inline()` holds the error (`lifecycle.hh:461-476`).
3. R4: `owner_` is written and read under `inbox_mutex_` (`task.hh:407`, `:463`).
4. N8: service-shape.md no longer names `event::wait`.

Ordering a consumer can observe (unchanged by this quest, in effect since task:aa92196, and only on
`inline_scheduler`; asio's executor is not involved): a cross-thread `post_call` runs at the next
step, behind the ready handles queued when it is taken and ahead of every demoted handle; a
same-thread `post_call` stays FIFO with `post()`. Before aa92196 a cross-thread call ran only once
ready and demoted were both empty.

## Rulings I made

- R4's ordering argument: `run_until` stores `owner_` under `inbox_mutex_` before its first step,
  and `post_call` compares it under the same mutex, so the lock orders every read after the latest
  driver's write; a thread that drove earlier sees the new owner's id and goes to the inbox. The
  draining thread never takes the lock: it matches the thread-local `draining_scheduler` and
  pushes to `ready_` directly. Debug builds assert one driver at a time (`driver_`,
  `task.hh:445-454`). Covered by `deadline_test owner` under TSan (in the tsan job above), and by
  task-inbox-first's r4 mutant (a TSan race, 261119-112655-c20d84).
- No test moved: the quest names `test/inbox_test.cc` and `test/task_test.cc`, which do not exist;
  the inbox tests live in `test/deadline_test.cc` (`inbox`, `owner`) and `test/lifecycle_test.cc`
  (`flush-inline`), and stay there.
