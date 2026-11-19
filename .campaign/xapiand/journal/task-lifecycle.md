# task-lifecycle: journal

State: ready for review. Suite 951ddb and TSan 36a333 pass at 066a183; mutants 838b76, ac2bcb, 37fb0f, 73b92c fail as required; CI green.
Quest: .campaign/xapiand/quests/task-lifecycle.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1 built at task:066a183 (A1-A7). Syntax-checked clean (clang, -Wall -Wextra) under quiet.
Jobs queued, none run yet (light slots full):
- suite 261119-070103-951ddb, tsan 261119-070103-36a333 (`campaign verify`, --for A1-A6)
- fails-without at mutant commits (refs/mutants/task-lifecycle/* in the task repo):
  - 261119-070126-838b76 A1: stops run forward (81a3a6c) -> `task_lifecycle_test order` fails
  - 261119-070133-ac2bcb A1: no cancel at bound - grace (48c275e) -> `bound` fails
  - 261119-070140-37fb0f A4: sleep ignores its token (454b50e) -> `task_timers_test periodic` fails
  - 261119-070146-73b92c A5: start failure skips the unwind (4155fc4) -> `failure` fails

## Next

1. Read the six jobs; fix any red (each fix gets its own test and mutant job).
2. Push is the lead's: CI (A6's "green on every platform") runs on the quest branch push.
3. Paste `campaign brief`'s evidence table; A7 row: docs/design/service-shape.md (D132 names,
   D15, D16), README.md and docs/API.md list the API; docs/mechanisms/lifecycle.md.
4. Later step (not in A1-A7): `event::wait(token)` and the process step (`run_daemon`,
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

| Criterion | Evidence |
|---|---|
| A1 | 261119-070103-951ddb, 261119-070103-36a333; fails without: 261119-070126-838b76, 261119-070133-ac2bcb |
| A2 | 261119-070103-951ddb, 261119-070103-36a333 |
| A3 | 261119-070103-951ddb, 261119-070103-36a333 |
| A4 | 261119-070103-951ddb, 261119-070103-36a333; fails without: 261119-070140-37fb0f |
| A5 | 261119-070103-951ddb, 261119-070103-36a333; fails without: 261119-070146-73b92c |
| A6 | 261119-070103-951ddb, 261119-070103-36a333 |
| A7 | `docs/design/service-shape.md`: "task::lifecycle" |

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
