# task-lifecycle review: correctness

Commit: 066a183
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Reviewed task:066a183832a4 (`lifecycle.hh`, `task_timers.hh`, `task.hh`'s sleeps, `test/`,
`docs/`) against cpp-libraries:0579a1a `docs/server-composition.md` (steps 3, 7, 8, C10) and
`docs/design/service-shape.md` at the same commit.

## Evidence I relied on

- 261119-070103-951ddb (suite) and 261119-070103-36a333 (TSan) pass at 066a183: the order, bound,
  hurry, failure, nested (two levels), parent, tool and three-signal child-process tests.
- Mutants 838b76 (stops forward), ac2bcb (no cancel at bound minus grace), 37fb0f (sleep ignores
  its token) and 73b92c (start failure skips the unwind) fail as required. CI green per the lead.

## What I checked myself

Scripts in `.scratch/xapiand/review-task-lifecycle-correctness/` (`run.sh MODE`; probes build
against `$JOB_SRC` with the job's own FetchContent deps). A probe job exits 1 when it finds a defect.

- 261119-083042-a3d297 (`run.sh lifecycle`, inline_scheduler, fake clock): P1 stop during a start
  waiting on its token, P1b stop before start, P2 a throwing drain, P3 a start failure three levels
  deep (`failed == "1/1/d"`), P5a third-signal flushes through `components` nesting: all correct.
  P5b, P5c, P7, P8a, P8b: defects below. P6: informational.
- 261119-083132-31d2eb (`run.sh lifecycle-asan`): P7 under ASan, use-after-poison in
  `event::set()` (`task.hh:1699`) from the bound timer's completion.
- 261119-083046-8363f1 (`run.sh sleep 30000`): a sleep's fire against a cross-thread cancel on
  `blocking_scheduler`, the request timed around the calibrated fire: 69 `drive()` stalls in 30,000
  rounds (10,488 fired, 19,512 cancelled).
- 261119-083741-b98c22 (TSan) and 261119-083743-4d5bbd (ASan+UBSan), `run.sh end-* 3000`: the
  lifecycle ending while a hurry and a third-signal `run_flushes()` arrive from another thread at
  random offsets: 3,000 rounds each, no report, every round flushed both components. This answers
  "use after destruction when lifecycle ends while a hop is in flight": clean. (Jobs 083047-e63b6c
  and 083048-8d3767 were a probe bug, a stop before the starts, superseded by these.)
- 261119-083044-b2e868 (`run.sh asio`): service-shape's `main` as written, on `asio_scheduler`.
- By reading: the signal handler (`lifecycle.hh:328-335`) saves errno, loads an atomic int and calls
  `write(2)` on a non-blocking pipe: async-signal-safe. Handlers are installed before the thread
  starts and the pipe holds early signals, so a signal during start stops it (P1 is the same token
  path). `group::join`'s ready path locks the group's mutex in `~join_awaiter`, so a joiner on
  another thread cannot free the group while `finish()` still holds it.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A start failure, or a throwing `ready()`, never requests the token `start(stop)` was given. A component whose housekeeping runs on that token (server-composition step 8: `periodic` owned by the component, its drain joins the group) never drains, so the lifecycle hangs until something outside stops the source: an HTTP bind failure leaves the daemon running, not exiting with the failure. | 261119-083042-a3d297 P8a, P8b: still running at 5 s, 50 ticks, after `drain housekeeper`; ends only after `request_stop()`. `lifecycle.hh:408`, `:413-420` | Start the set on a token the lifecycle itself requests on a start or ready failure (a `cancel_source` linked to `source.stop()`), before the stop pass. Test: P8a and P8b in `lifecycle_test`, with a mutant that drops the request. |
| F2 | yes | defect | A `stop()` that throws (asio's throwing `acceptor.close()`, say) escapes `run_lifecycle`: the remaining `stop()`s, every drain and every flush are skipped, and the orphaned bound timer later sets `timer_ended` in the freed frame. The concept does not require `stop()` to be `noexcept`, and `components::stop` does not catch. | 261119-083042-a3d297 P7: log ends at `stop b`, lifecycle threw. 261119-083132-31d2eb: ASan use-after-poison at `task.hh:1699` via `run_then` of the timer. `lifecycle.hh:117-119`, `:426-430` | Either require `noexcept` stop in `component` (a static_assert), or catch per component in `components::stop`, keep stopping and report the first throw as `failure`; in both cases the timer is finished and joined on every exit from the stop pass. Test: a throwing `stop()` still runs every other stop, drain and flush and reports the throw, under ASan. |
| F3 | yes | defect | Lost wake in `sleep_awaiter`: a cross-thread cancel that has claimed the sleep (CAS to `cancelled`) but not yet posted its hop when the entry fires leaves `blocking_scheduler::drive()` with an empty queue and wheel, so it reports a stall with the frame still waiting. This is the race 412b2c2 fixed for deadlines (`cancel_state::settle`); the sleep keeps its own state word and does not settle. `periodic` and `debounce` inherit it. On asio it bites a loop without a work guard the same way. | 261119-083046-8363f1: 69 stalls in 30,000 rounds. `task.hh:1542-1563` | A fire that loses to a claimed cancel waits until the hop is queued (as `settle` does), or the sleep reuses the deadline's settle. Test: a forced interleaving like `blocking_test race-forced` (the callback held between claim and hop while the entry fires), with a mutant. |
| F4 | yes | gap | The nested-lifecycle pattern service-shape documents (`service-shape.md:172`: a component whose start spawns `lifecycle` over `stop_source inner(stop)`) loses the third signal's flushes: `run_flushes()` on the process's source reaches only the outer set, whose parent `flush()` does not reach the inner source's registrations, so the children die unflushed. `lifecycle_test`'s `parent_component` (`test/lifecycle_test.cc:192-214`) has the same hole. `components` nesting is correct (P5a). | 261119-083042-a3d297 P5b: third signal flushed `[flush parent, flush a]`, no `x`, `y` | Options: (a) keep the pattern: the parent component's `flush()` runs `inner.run_flushes()` (or its children's flushes), stated in the note, and the test checks it at a third signal; (b) drop the pattern and keep `components` as the one nesting mechanism (owner's call; narrows a documented use). (a) clears the block. |
| N1 | no | gap | Across a nested lifecycle, stops are not in reverse start order: an outer component started before the parent stops before the parent's children, since the inner lifecycle sees the stop through a hop. `lifecycle_test` asserts that order (`:225`). | 261119-083042-a3d297 P5c: `[stop parent, stop a, stop y, stop x]` | State the order in the note, or prefer `components` (F4 (b)). |
| N2 | no | defect | service-shape's `main` (`service-shape.md:162`) runs `io.run()` with no work guard, so it returns at once while the lifecycle waits on the stop: the daemon exits 0 at start, unflushed. | 261119-083044-b2e868: `io.run()` returned after 0 ms, lifecycle not ended, flush not run | A work guard reset in the completion callback (as `timers_test` does). |
| N3 | no | gap | Ruling D16 (flushes on the loop, not a D11 thread) departs from the note's unfinished path ("only flush(), on a thread (D11)"). Consequences: a nested lifecycle's flush blocks the shared loop while the parent's other drains run; abandoned work cannot move during the flush; a `flush()` that needs the loop deadlocks. | `lifecycle.hh:433-437`; `service-shape.md:260` | Owner's call: keep D16 and say `flush()` must never need the loop, or restore a D11 thread for the flush. |
| N4 | no | gap | Abandonment is cooperative: the lifecycle never stops waiting on its own, so a drain that ignores `b.abandon` hangs a programmatic or tool stop forever (only a third signal ends it), and `stopped` has no abandoned flag, so a drain that returns nothing at abandon reads as a clean stop. Matches the note's text. | 261119-083042-a3d297 P6: unfinished 0, failure none after abandon | Optional: `stopped::abandoned = abandon.requested()`; keep otherwise. |
| N5 | no | defect | `~stop_signals` closes the write end while a handler on another thread may have loaded the fd before `store(-1)`: that handler's `write` can land in a reused descriptor. | `lifecycle.hh:310-317`, `:330-332` | Keep the write end open for the process's life, or never reuse it. |
| N6 | no | gap | `run_flushes()` runs the flushes under the source's mutex: a `flush()` that calls `fail()` or `failure()` on that source self-deadlocks, and the third signal then never kills. | `lifecycle.hh:263-269` | Say so in the concept's comment, or run the flushes outside the lock with the registration held alive. |
| N7 | no | nit | `sleep_awaiter`'s cancel ignores `post_call` returning false (a scheduler without it): the cancel is lost with no assert, unlike the token-bound awaits (`task_asio.hh:482`). | `task.hh:1547` | Assert as the others do. |
| N8 | no | nit | `event::wait(token)` left the note's API (deferred in the journal), but the note's step 1 still lists it, and Xapiand's HTTP drain row awaits a D11 event on `b.abandon`. A `cancel_source` linked to `b.abandon`, requested by the thread, does that today. | `service-shape.md` "Checks and first steps" 1; manager.cc table | Align the step list. |
| N9 | no | nit | Naming: `task::components` and `stop_source` / `stop_signals : stop_source` match the note as revised and C10; no departure from server-composition step 7. `periodic` and `debounce` need only `task.hh` and run without a lifecycle (timers_test), as D139 asks. | `lifecycle.hh:90`, `task_timers.hh:39` | None. |

## Not run

- No probe on Linux or FreeBSD; F3's stall count is from this macOS host only.
- The three-signal child-process test under a wedged loop or in teardown (service-shape step 3) was
  not repeated; I relied on the suite's `signals` case.
- `debounce` under concurrent `touch` from several threads, and `group::join` with a token racing
  the last `finish()`, were read, not stressed.
- Detent's and Prism's components were not modelled; F1's case is server-composition step 8's
  shape, not a measured adopter.

## Re-check

Commit: 27028a0
Verdict: APPROVE
Date: 2026-11-19

A scoped re-check of fix round 1 (task:27028a038fb6), against Xapiand D140.

Evidence accepted: the suite (261119-090341-1360aa) and TSan (261119-090341-37aa0c) at 27028a0;
the probe jobs 0559d4 (lifecycle), ff6d94 (lifecycle, ASan+UBSan), 6cc987 (sleep race: 0 stalls
in 30,000 rounds, against 69 before), e27174 and d1a7c8 (lifecycle end races under TSan and
ASan), and 3d1640 (the note's `main`: `io.run()` waits 302 ms for the stop, then the lifecycle
ends flushed); the nine mutants in the journal. My own probe: 261119-091929-77b81e
(`run.sh inbox`).

1. **The inbox.** A cross-thread `post_call` goes into a locked vector with a release flag that
   `run_until` drains under the same lock (`task.hh:401-410`, `:469-476`), so nothing posted is
   lost while a driver keeps calling `run()`. `off_loop` joins its thread in `resume` and
   in its destructor, so the frame never outlives the thread's last touch
   (`lifecycle.hh:437-478`). Ordering against `post()`: a same-thread `post_call` stays FIFO with
   posts, but a cross-thread call is taken only once ready *and demoted* are both empty
   (`task.hh:447`). The class's rule is that demoted handles run only when nothing else is ready,
   so this breaks it: R1.
2. **The adapted probes keep their meaning.** P7 now waits for the flush thread and still requires
   no throw, every other stop, drain and flush, and an ASan-clean run of the bound timer. P5b now
   follows the note's revised pattern (the parent forwards `flush()` to `children`,
   `service-shape.md:172-176`). Since F4's fix is that pattern, P5b checks the right thing:
   whether a component written as the note says gets flushed at the third signal. The suite's
   `parent` case checks the same thing.
3. **Failure order.** The order is now start, ready, stop, drain, flush, which is chronological,
   and `drain_all` and `flush` keep their first throw in the order they ran
   (`lifecycle.hh:509-528`). The exception is `fail()`, which still ranks last (`:528`) even
   when it requested the stop and so came before every stop, drain and flush error. That is what
   the note says ("else fail()'s"), but a failing control task's cause is then reported as the
   drain or flush error it led to: R3. Otherwise the order is right.
4. **Findings:**
   - **Closed:**
     - F1: a start or ready failure now ends what ran on the start token (`lifecycle.hh:484`, `:500`; P8a, P8b).
     - F2: `components::stop` catches per component (`lifecycle.hh:122-132`), and `run_lifecycle` catches stop and drain and joins the timer (`:508-519`; P7 under ASan).
     - F3: the lost fire settles the token (`task.hh:1598`; 6cc987).
     - F4: the parent pattern forwards its flush (`service-shape.md:172-176`; P5b, suite `parent`).
     - N1: the order is stated (`service-shape.md:175`).
     - N2: work guard (`lifecycle.hh:553`; 3d1640).
     - N3: flushes run off the loop (`lifecycle.hh:523`, D16 revised).
     - N5: the destructor waits for handlers still writing (`lifecycle.hh:336`, `:359`).
     - N6: `run_flushes` drops the lock and `remove_flush` waits for idle (`lifecycle.hh:277-296`).
     - N7: assert (`task.hh:1581`).
   - **Open, non-blocking:**
     - N4: there is still no abandoned flag. Optional, the owner's call.
     - N8: the note's step 1 still lists `event::wait` (`service-shape.md:291`).

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | defect | A cross-thread `post_call` on `inline_scheduler` runs only after every demoted handle, including ones posted after it, so a demoted loop that keeps re-posting starves it: a flush thread's resume or a sleep's cancel hop waits for the loop to end, and never runs if the loop does not end. | 261119-091929-77b81e: posted at step 10 and run after step 10,000; a same-thread call ran at step 10. `task.hh:447` | Take the inbox when `ready_` is empty, before falling back to demoted. Test: the probe's ordering. |
| R2 | no | gap | When `off_loop` cannot start its thread (`std::system_error` from `await_suspend`, `lifecycle.hh:450`), the end-of-lifecycle flush never runs; it is only reported as `failure`. That breaks "every path ends with the flushes" under thread exhaustion. | By reading, `lifecycle.hh:448-458`, `:522-526` | On a failed spawn, run `f` inline on the loop. |
| R3 | no | gap | `fail()` ranks after every other error even when it caused the stop, so a failing control task's cause is replaced by the drain or flush throw it led to. | `lifecycle.hh:528` | Owner's call: keep it (the note's rule), or record `fail()` first when it arrived before the stop. |
| R4 | no | nit | `owner_` is read relaxed from other threads, so a thread that drove the scheduler earlier and now posts while another thread drives it can match its own stale id and push into `ready_` unlocked. | `task.hh:402` | Document one driving thread per `inline_scheduler`, or compare with acquire against a run counter. |

Not run in this re-check: Linux and FreeBSD, and R1 or R2 under a sanitizer. I did not stress
`off_loop` against teardown destroying a lifecycle frame mid-flush on asio.
