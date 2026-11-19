# task-lifecycle: journal

State: waiting on jobs 261119-090341-1360aa 261119-090341-37aa0c 261119-090347-251328 261119-090348-2190d4 261119-090350-b4378e 261119-090352-db744b 261119-090354-f4cfff 261119-090355-d51b76 261119-090357-d8be11 261119-090358-1a1d14 261119-090405-f5a3a0 261119-090407-ec258c 261119-090420-0559d4 261119-090421-ff6d94 261119-090422-6cc987 261119-090424-e27174 261119-090425-d1a7c8 261119-090426-3d1640
Quest: .campaign/xapiand/quests/task-lifecycle.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix round 1 (correctness review CHANGES at 066a183, D140) built at task:27028a0. Local Debug
build and ctest pass (11/11). Jobs at 27028a0:
- suite 261119-090341-1360aa, tsan 261119-090341-37aa0c (`campaign verify`)
- fails-without, mutants in refs/mutants/task-lifecycle/* (Debug, one case each):
  - F1 start-token-unrequested (c97c6c3): `lifecycle_test start-token` 261119-090347-251328
  - F2 stop-throw-escapes (72aa59d): `stop-throws` 261119-090348-2190d4
  - F3 sleep-fire-skips-settle (918156d): `timers_test sleep-forced` 261119-090350-b4378e
  - F4 parent-flush-not-forwarded (64f77a3): `parent` 261119-090352-db744b
  - N3 flush-on-loop (d032618): `flush-off-loop` 261119-090354-f4cfff
  - N2 no-work-guard (33829d4): `asio` 261119-090355-d51b76
  - N5 close-under-handler (b795cd6): `signal-close` 261119-090357-d8be11
  - N6 flushes-under-lock (85da541): `flush-fail` 261119-090358-1a1d14
  - N7 sleep-cancel-no-assert (c496c69): `sleep-no-post-call` must abort; fixed 261119-090405-f5a3a0,
    mutant 261119-090407-ec258c
- probes: lifecycle (adapted copy: P5b, P7, P8a, P8b) 261119-090420-0559d4; lifecycle-asan (P7)
  261119-090421-ff6d94; sleep 30000 261119-090422-6cc987; end-tsan 261119-090424-e27174;
  end-asan 261119-090425-d1a7c8; asio 261119-090426-3d1640. Reviewer's run.sh for sleep, end-*
  and asio; the adapted copy is task-wt .scratch/task-lifecycle/fix1/probes/ (see rulings).

## Next

1. Read the jobs; fill Evidence with the fix-round rows; State to ready for review.
2. Lead: `campaign accept task-lifecycle correctness --confirm F1=... F2=... F3=... F4=...` with the
   suite job; push for CI (A6).
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
- Fix round 1 (review F1-F4, N2, N3, N5-N7; D140):
  - F1: the components start on the lifecycle's own `cancel_source`, linked to the source's stop,
    requested at a start or `ready()` failure (and on every path before the stop pass).
  - F2: `components::stop` catches each throw and rethrows the first once all ran (flush's shape),
    not a `noexcept` requirement. `failure` is now chronological: start or ready, stop, drain,
    flush, then `fail()` (a flush error used to outrank a drain error).
  - F4: the parent holds its children as one `components` member and forwards `flush()` to it,
    not `inner.run_flushes()`: flush errors propagate and only started children flush.
  - N3: D140's "third-signal flush runs off the loop" read as every lifecycle-end flush on a D11
    thread (`detail::off_loop`) that resumes the lifecycle through `post_call`; the third signal
    already flushed on the signal thread. The loop needs `post_call` (static_assert).
    `inline_scheduler::post_call` from another thread goes to a locked inbox `run()` drains once
    its queues are empty; `post` (the resume path) is unchanged, so not benchmarked.
  - N2: `asio_scheduler::work_guard()`; `lifecycle` holds it when the loop declares one.
  - N5: the handler counts itself in before loading the fd; `~stop_signals` waits for zero
    before closing the pipe. N6: flushes in a `std::list`, run with no lock held; a removal waits
    for no run in progress. N7: an assert; its test is a Debug control case, like
    blocking_test's offthread-cancel.
  - Tests' and probes' fake-clock drive loops yield when nothing is queued or armed, and their
    limit rose to 1,000,000 simulated ms, so the flush thread runs. The reviewer's lifecycle
    probe assumes the old parent pattern and an on-loop flush (P5b, P7 read the log before the
    flush thread ends); the adapted copy forwards flush, waits for P7's result, and prints P5c
    as INFO (N1).

## Notes (D140: not fixed)

- N1: across a nested lifecycle, stops are not in reverse start order (the inner lifecycle sees
  the stop through a hop): `[stop parent, stop a, stop y, stop x]`. The note now says so.
- N4: abandonment is cooperative; a drain that ignores `b.abandon` hangs a programmatic stop,
  and `stopped` has no abandoned flag. Matches the note.
- N8: the note's "Checks and first steps" 1 still lists `event::wait`, which left the API
  (deferred above); Xapiand's HTTP drain row awaits a D11 event on `b.abandon`.
