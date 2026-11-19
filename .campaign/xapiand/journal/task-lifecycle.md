# task-lifecycle: journal

State: working
Quest: .campaign/xapiand/quests/task-lifecycle.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1, A1-A7 built on task quest/xapiand/task-lifecycle: sleeps in task.hh, task_timers.hh
(group, periodic, debounce), lifecycle.hh (component, components, stop_source, stop_signals,
lifecycle), tests test/timers_test.cc and test/lifecycle_test.cc, docs. Syntax check, then the
suite and tsan through `campaign verify task-lifecycle`.

## Next

1. Run `campaign verify task-lifecycle` (suite, tsan) and fix what fails.
2. A mutant job for A1: drains in start order must fail test/lifecycle_test.cc order.
3. Later step (not in A1-A7): `event::wait(token)` and the process step (`run_daemon`,
   `ProcessLock`) moved from Detent, per server-composition's adoption order 3.

## Blocked on

Nothing.

## Decisions

- lead, 2026-11-19: `periodic` and `debounce` stay in task, in their own header
  (`task_timers.hh`), separate from lifecycle, built as coroutines over task's sleep on the
  loop's wheel; they do not depend on lifecycle, and lifecycle includes them only if a component
  needs them. Built so: `lifecycle.hh` includes only `task.hh`. `group` lives in
  `task_timers.hh` beside them, since `debounce` spawns its watchers into one.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- "Task's existing sleep" did not exist: `sleep_until`/`sleep_for` are new, in task.hh, as
  service-shape's API states. A cancel hops through `post_call` (D3); `sleep_until(never, t)`
  waits for the token alone, so the driver's wait on the stop needs no second primitive.
- The note's call shape is kept: `lifecycle(loop, stop, timing, ready, cs...)` is a function
  returning `task<stopped>` (served renamed). The nesting type is `task::components`, a set that
  is a component; the driver runs one internally, so the order logic exists once.
- The stop source's type is `task::stop_source` (the note leaves it to task); `stop_signals`
  stays as the signal source, deriving from it. A7's "stop_signals as a type is gone" read as:
  gone as the driver's parameter type.
- A start failure is reported as `stopped::failed`, the component's `name()` or position,
  nested as "1/c"; a nested set whose start threw still counts as started so its started
  children unwind.
- `drain_all` keeps lines and failure together so a nested drain's leftovers are never lost to
  its throw; `components::drain` (the concept form) rethrows.
- Flushes run on the loop at the end of the driver, not on a D11 thread (design D16): the
  fake-clock scheduler is single-threaded; `add_flush` returns a registration removed when the
  driver returns, so a late third signal never flushes a dead set.
- The process step (`run_daemon`, `ProcessLock`) is not in A1-A7; left for a later quest.
