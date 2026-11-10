# x7-catalog-timer: journal

State: blocked on lead: no debouncer wheel can carry the catalog's timer (D56's mechanism)
Quest: .campaign/xapiand/quests/x7-catalog-timer.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Stopped before code: D56's mechanism does not exist as written (Blocked on 1).

## Next

1. On the lead's ruling for Blocked on 1, implement the chosen option in src/database/pool.{cc,h}, then A1-A3 jobs.

## Blocked on

1. **D56 names a wheel that cannot take the catalog's timer** (lead decides). Findings, at master:
   - The autocommit debouncer that had the `AC--` wheel is now `control::Debounce` (src/control_plane.h:184), tasks
     on the control loop; no thread. Section 2's "the debouncer wheel" has no thread behind it today.
   - The only debouncer wheel left is the FS fsyncher's `FS--` (src/xapiand_fsyncher.h:73), which section 2 retires
     in 7b (D8), and it is outside this quest's file set.
   - A scheduler wheel is typed by its one task type: `ThreadedScheduler<DebouncerTask<int, F, Tuple>>`, and
     `ScheduledTask::operator()` static_casts to that type (scheduler d57e8fc scheduler.h:170-179). A catalog `Fire`
     task cannot be added to it; the debouncer's API only calls `fsyncher_fsync(int, bool)`.
   - The x7-gate census at master: unnamed 16, W 16, FS 9 (8 workers + FS--), WL 8, LOG 1, main 1, DB-TIMER 1.
   Options:
   a. The control loop: the catalog's timer arms `engine::Host::start_timer` on the control loop's AsioHost (section 2's
      diagram already puts idle-close there; D1 too). Costs: the host is set after the pool is built and cleared
      before Stop 3, and test_pool runs without one, while the catalog's destructor waits for its armed timers; needs
      the control loop to own the timer for the pool's whole life (manager.cc, outside the file set) or a fallback,
      which is two mechanisms.
   b. One shared, type-erased wheel: Kronuz/scheduler gains a task type that runs a callable, the FS debouncer and
      the catalog share it (scheduler repo, xapiand_fsyncher.h). FS goes in 7b, so the catalog would move again.
   c. Keep DB-TIMER and amend section 2's list (D56 rejected this).
   Recommendation: (a), with the quest's file set widened to src/manager.cc and the pool's tests: the catalog timer
   becomes a control-loop timer like the autocommit, one timer mechanism on a thread that exists for good; the pool
   takes its timer host at construction (manager.cc:284 builds the pool before `_rx`, so the members reorder)
   and tests pass a test host. The pool is reset at manager.cc:1188, after the loop; that order needs checking.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <job id at the current code> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
