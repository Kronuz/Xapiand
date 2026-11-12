# detent service-shape review: Xapiand fit (design, A3)

Commit: task:5f079559844f91ad80e5a494f356bccc9d3725a5
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right for Xapiand: one control loop, signals as tokens, start/stop/drain in reverse,
every wait on the loop's wheel. Two things in the text would lose behaviour Xapiand's users have
today. Both are text fixes to the design, not a different shape.

## Evidence I relied on

- docs/design/service-shape.md at task:5f07955 (the design commit only; the code it reads is
  task:47d50f3, `task_asio.hh:56-62,108-160`).
- Xapiand master (Xapiand:000224f5a): src/manager.cc, src/main.cc, src/control_plane.h,
  src/database/pool.cc, src/database/wal.cc, src/database/shard.cc, src/server/search_views.cc,
  docs/mechanisms/control-plane.md, docs/mechanisms/index-registry-on-catalog.md, docs/design/phase7c.md D16.
- Xapiand's stop tests, which state the behaviour as tested today: tests/test_node.cc:934-963
  (an abandoned stop exits 75 and every acknowledged write is read back after a restart), :1134-1151
  (Stop 1 to Stop 5 in order), :1153-1165 (a second SIGTERM ends the waits, exit 75).
- strata:5d742b2 catalog.hh:20-30,498-505 (shutdown resumes once every slot is closed, so once every
  pin is released); http:51d153a http_asio.h:1189-1201 (`stop()` cancels in-flight requests).
- The earlier fit review, .campaign/xapiand/reviews/detent-task-wheel-fit.md (N1).

## What I checked myself

No probe job. Each finding is a code reading of an ordering Xapiand already documents and tests;
an executable model of the new shape would have to be the migration itself. Asio's default
`signal_set` flags read at 1.36.0 (`signal_set_service.ipp:325,359-360`).

### 1. The manager.cc table against manager.cc and pool.cc

The rows cover the timers, autocommit, housekeeping, node identity, wiring, metrics and signals.
These duties of today's stop are missing, and the first two are behaviour users see (B1, B2):

| Duty today | Where | In the design |
|---|---|---|
| HTTP's roots destroyed (and index chores cancelled) before the indexes close, so their pins are released | manager.cc:1090-1102; control-plane.md step 2; x-pool C24 | teardown after `serve`: too late (B1) |
| The control loop's roots destroyed before Stop 3 | manager.cc:1113-1121 | the loop stays live, so only roots a drain gave up on remain; part of B1's fix |
| WAL writers and fsync threads drained without a bound, also on the abandoned path, before `_Exit` | manager.cc:1152-1185, :1188-1195 | inside the one bound and cut by the hurry (B2) |
| The registry's detached control tasks counted and drained | manager.cc:1009-1014; pool.cc:670-700 | not named (G4) |
| The registry timer's `close()` posts its timers' cleanup to the loop | pool.cc:449-451, :477-488 | not in a frame, so D10's "phase 3 destroys its frame" does not cover them (G2) |
| HTTP's grace sized from the time left | manager.cc:1074-1076, :165-169; test_node.cc:968-977 | `drain(cancel_token)` carries no time (G1) |
| Admin `/_quit` and `/_flush?clients` call `try_shutdown`/`shutdown(0,0)` | search_views.cc:1551, :1534 | no row (G3) |
| `sig_handler`: SA_RESTART, stdin to /dev/null and the signal name on stderr, `exit(EX_SOFTWARE)` once deinited | main.cc:230-262, :296 | not named (G5) |
| The log flush before `_Exit(EX_TEMPFAIL)` | manager.cc:1193-1194 | `main` sketch logs and exits (G5) |

### 2. N1: does anything fire after the scheduler closes, and does Stop 3 hang?

Not through the wheel. In this shape the registry drains on the live control loop, before
`serve` returns. The waits it relies on today through `run_control(0ms)` (manager.cc:1077, :1131)
then fire normally, and the abandoned path `_Exit`s without closing anything. A
`Timer::close()` runs every pending callback inline (pool.cc:477-485), and those callbacks only
post a chore, so none needs the closed wheel. The one leftover is G2: the registry's armed entries
(the catalog's 1 Hz sweep is always armed) live in the Timer object, not in a frame, and today's
close posts their cleanup to a loop that will not run again.

Stop 3 does wait, but for pins, not for timers (B1). The design runs `teardown(http, registry, loop)`
(phases 2 and 3) only after `serve`, so after the registry's drain. Requests left suspended on the
stopped HTTP loops (on a turn, a chore or a body read) keep their shard uses until phase 3
destroys them. The catalog's shutdown resumes only once every slot is closed, and a slot does not
close while pinned. So the registry's drain waits out the whole bound, and the node exits 75 with
its indexes uncommitted. A stop under load would usually end that way, where today it ends
cleanly. With B2 unfixed, it would also drop the WAL.

### 3. Q2, one bound from the first signal: what clients see

For an idle node, nothing changes: today the first signal finds `http_clients == 0` and stops at
once (manager.cc:739-750). For a busy node these change. The ones that narrow behaviour are
marked; the owner decides those:

1. **Narrows.** New connections are refused at the first signal. Today, while any request is in
   flight, the acceptors keep accepting for up to 8 x 5 s = 40 s (manager.cc:692, :708; D16 says
   "the acceptors still accept, up to about 45 s").
2. **Narrows.** New requests on open keep-alive connections are refused at the first signal; today
   they are served for the same window.
3. **Narrows.** In-flight reads are cancelled at the first signal (`stop()` aborts every request;
   reads end at their next checkpoint, http_asio.h:1190). Today a read shorter than the remaining
   window completes normally, since `abort_all` waits until `join()`.
4. **Narrows.** A second signal hurries everything. Today, before `join()`, a second signal 200 to
   1000 ms after the first only ends the wait for clients: the full stop still runs and the indexes
   commit. A repeat within 200 ms is ignored as a duplicate (manager.cc:645-685). In the shape, any
   second SIGINT/SIGTERM (a supervisor that forwards the signal plus a process-group kill, for
   example) abandons the drains and exits 75 with the indexes uncommitted. During `join()` the
   second signal behaves the same as today.
5. **New: a third signal kills** (D6, SIG_DFL). Today no SIGINT/SIGTERM ends the process during the
   stop. A third signal that lands before the WAL drains loses acknowledged writes (B2's queue)
   and the log.
6. **Shorter:** the stop takes at most 35 s, where today it can take about 40 s, then 35 s more,
   plus an unbounded WAL/fsync drain. Admitted writes keep D16's arithmetic, since HTTP drains
   first, with about 35 s left.
7. **Acknowledged writes:** unchanged only if B2 is fixed; otherwise lost on any bound or hurry
   while the async WAL queue holds records.

### 4. Is the shape worse than today anywhere?

No, as far as code reading shows. I measured nothing: the wheel's lateness is already measured,
0.5 to 2.2 ms (task deadlines.md; 651 us in our earlier probe). Xapiand's timers run at 1 min
(housekeeping), 1 Hz (the sweep), the debounce timings, and turn deadlines of `DB_TIMEOUT` (30 s).
Only `drain_readers`' first 1 ms pause (engine/strata/shard.cc:491-494) moves to 1 to 2 ms, and it
backs off to 50 ms anyway. Allocation per arm goes from 3 (`make_shared<State>`,
`make_unique<Node>`, `make_unique<Armed>`, host_asio.h:68-75; plus the registry map entry and
`std::function`) to 0 for sleeps. The D11 threads exist only at stop, 2 to 3 short-lived ones,
where today's `shutdown_wait` already spawns one.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B1 | yes | defect | Teardown phases 2-3 run only after `serve`, so HTTP's suspended request roots keep their pins through the registry's drain. The catalog's shutdown cannot finish, so Stop 3 runs to the 35 s bound and the node exits 75 with indexes uncommitted, where today it exits 0. `task_asio.hh:56-62` also says phases 2-3 never run on a loop thread, which is where `drain` runs. | manager.cc:1090-1102, :1113-1121; control-plane.md step 2-3; index-registry-on-catalog.md:60-66; strata catalog.hh:498-499; tests/test_node.cc:1134 | State that a service's drain ends with its own schedulers' phases 2-3 once its loops are joined. For HTTP: D11's thread runs `stop()`, `IndexChores::cancel_all`, `close()`, `destroy_roots()`, then sets the event. Allow phases 2-3 on that thread. Migration test: a request suspended on a turn at stop, and the stop exits 0 in order (keep test_the_stop_runs_in_its_stated_order). |
| B2 | yes | defect | The WAL writers and fsync threads sit inside the one bound, and the hurry cuts them ("each remaining drain ... waits for nothing", then `_Exit`). Every WAL write is async (`DB_SYNCHRONOUS_WAL` is never set; wal.cc:1111-1115), so an acknowledged write can still be queued. Today Stops 4-5 have no bound, run even on the abandoned path and after a second signal, and the `_Exit` relies on them ("What was acknowledged is in the log"). | manager.cc:1152-1185, :1188-1195; shutdown_wait.h:28-31; tests/test_node.cc:934-963; design "Stop and drain order" 3-4 | Make the WAL drain a step that neither the bound nor the hurry cuts: the registry's drain, and the unfinished path in `main`, flush the WAL writers (then fsync) before reporting and `_Exit`. Name that in Decisions and Limits. Migration test: a second signal while acknowledged writes are queued, and a restart reads them all back. |
| G1 | no | gap | `drain(cancel_token bound)` passes no time, while HTTP's grace is `(left - 5 s) / 2` so the reserve stays for the registry. | manager.cc:1074-1076, :165-169; test_node.cc:968-977 | Pass the bound's time point (or let `deadline` expose it), or give HTTP its grace from the bound at construction. |
| G2 | no | gap | The registry's wheel entries (the catalog's sweep is always armed) live in `DatabasePool::Timer`, not in a frame. Today's `close()` posts their cleanup to a loop that will not run again, and destroys the Timer only when the io_context goes, after the scheduler. | pool.cc:449-451, :477-488; design Timers "A timer still armed at teardown is cancelled when phase 3 destroys its frame" | `Timer::close()` cancels its entries on the teardown thread (D10) and is destroyed before the scheduler. Reword the pool.cc:451 rule. |
| G3 | no | gap | No row for the admin `/_quit` (`try_shutdown(true)`) and `/_flush?clients` (`shutdown(0,0)`). Without a prior signal both are no-ops today, since `shutdown_impl` acts only when `asap != 0` (manager.cc:703-707). | search_views.cc:1534, :1551; manager.h:201-205 | Owner's call: delete them (remove, not refuse), or map them to `request_stop()`, which would give admin a remote stop it does not have today. |
| G4 | no | gap | The registry's detached control tasks (`counted`/`Uncount`) are not named. `drain_control` waits for them today. | pool.cc:680-700; manager.cc:1009-1014 | The registry's drain joins them (they become its `group`). |
| G5 | no | nit | `stop_signals` loses parts of `sig_handler`: SA_RESTART (Asio's default `dont_care` sets `sa_flags = 0`), the stdin redirect and the signal name on stderr, and `exit(EX_SOFTWARE)` once deinited. The `main` sketch also does not flush the log before `_Exit` (`Logging::finish/join` today). | main.cc:230-262, :296; asio signal_set_service.ipp:359-360; manager.cc:1193-1194 | Pass `signal_set::flags::restart`. List the kept or dropped handler duties. Add the log flush to the `main` sketch. |
| G6 | no | nit | D12 ends a `periodic` when a tick throws. `control::Periodic` logs the throw and keeps ticking, so a single `schemas->cleanup()` throw would stop the schema cache's cleanup until restart. | control_plane.h:100-101, :148-153 | The schema tick catches what it tolerates, or D12 notes Xapiand's case. |
| G7 | no | nit | "Its stop token" for the autocommit debounce is ambiguous. If it were `signals.stop()`, every write during HTTP's drain would commit inline (`touch` false, shard.cc:1438-1439). Today that starts only after Stop 1. | design manager.cc table, Autocommit row; control_plane.h:199-206 | Say the autocommit service's own source, requested by its `stop()`. |
| Q2 | no | owner | One bound from the first signal narrows what a busy node serves while it stops (section 3, items 1-5): refused connections and keep-alive requests, cancelled in-flight reads, a second signal that abandons commits, and a third that kills. | manager.cc:692, :708, :645-685, :739-750; D16 phase7c.md:387-389 | Options: accept, as recommended, since stopping accepting at once is standard and 35 s fits orchestrators better than about 75 s. Or keep a short accept window before `stop()`. Or keep D16's 200 ms duplicate filter and the second signal's "end the wait only" step before `join()`, and drop the third-signal kill until the WAL is flushed. |

## Not run

- No build or test of any kind: the design has no code, and Xapiand's stop tests were not rerun.
- Detent's and Prism's rows, Q1, and the task API's own correctness: not this lens.
- The cost of `engine::Host` forwarding `arm`/`cancel` as virtuals, and of `BlockingScheduler`
  owning a wheel (today any thread can arm it, host.h:276-285; the wheel's `arm` is owner-thread
  only): for the migration quest.
