# detent task-wheel review: Xapiand fit (A7, D78)

Commit: task:c0cda2371a54a033c1960324deb474d3fcb4d28a
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- The diff `main..c0cda23` in Kronuz/task (task.hh:1013-1275, task_asio.hh:100-215,
  docs/mechanisms/deadlines.md, docs/API.md). Detent's own suite and CI: not rerun, not this lens.
- Xapiand master's timer code: src/engine/host.h, src/engine/host_asio.h, src/control_plane.h,
  src/database/pool.cc, src/walk_gate.h, src/manager.cc; its pins (CMakeLists.txt:765 task,
  :772 strata, :826 reactor, :827 http) read with `git -C` at those commits.

## What I checked myself

Probe job 261112-082133-e0a63a (exit 0, ASan+UBSan clean), script
`.scratch/review-detent-task-wheel-fit/run.sh` + `probe.cc` (rerun:
`campaign job submit --level light -- sh -c 'sh .../run.sh "$JOB_SRC" "$JOB_OUT"'`). It builds
Xapiand's own `engine/host_asio.h` (debug, so `Counted` too) against task.hh/task_asio.hh at
c0cda23 and wheel.hh at wheel:bfd72bc, and checks: `task::deadline<engine::Host>` still compiles
and fires through `AsioHost::arm`; a `deadline<asio_scheduler>` armed on the loop fires 651 us
late (within the 1 ms contract); a deadline still filed when the loop stops, destroyed off-loop
after `close()` (Xapiand's phase 3), then the scheduler destroyed (phase 4), is freed by the
wheel's `false` fire with no sanitizer report.

Which timers reach the new wheel. Only `task::asio_scheduler::start_timer` and
`inline_scheduler::start_timer` file on it; `engine::Host::start_timer` (host.h:109) is Xapiand's
own and goes to `AsioHost::arm` (host_asio.h:68, a raw `asio::steady_timer`) or
`BlockingScheduler::arm` (host.h:276). So:

| Use | Scheduler | On the wheel | Destroyed when | Outlives every deadline? |
|---|---|---|---|---|
| `control::Periodic` (control_plane.h:143) | `Host::start_timer` on `_rx->host` (AsioHost over the control reactor) | no | frame: phase 3 at manager.cc:1119-1120; `_rx` later | n/a; asio timer needs the io_context alive, it is |
| `control::Debounce` (control_plane.h:294) | same | no | same | n/a |
| `engine::SleepFor` (host.h:176) | `Host::current()`: AsioHost or BlockingScheduler | no | frame with its root; BlockingScheduler's frames end inside `run()` | n/a |
| Registry idle-close/commit timers (pool.cc:451-457, close() :483) | `Host::start_timer` on the control AsioHost, armed via the raw executor | no | `timers.clear()` posted; runs in Stop 3's `run_control` (manager.cc:1131) or dies with the io_context | n/a; the post-close firing it relies on is N1 |
| `task::deadline<engine::Host>` take_turn (pool.cc:339), walk gate (walk_gate.h:90) | `Host::start_timer`, timer type `unique_ptr<Host::Timer>` | no (state carries an unused 32 B entry, N2) | frame: phase 3 (http :1101-1102, control :1119-1120), hosts at :1201 | yes |
| http read/write deadlines (http:51d153a http_asio.h:366, :870) | `task::deadline<task::asio_scheduler>`, the reactor's scheduler | yes | session roots spawned on it (reactor:ab69cf2 reactor.h:414), destroyed at manager.cc:1102; scheduler at :1202 (`http_service.reset()`, scheduler is the Reactor's last member, reactor.h:314) | yes |
| TLS handshake (http_asio_tls.h:97) | same | yes | same root | yes |
| http deferred reply timer (http_asio.h:128) | raw `asio::steady_timer` | no | - | n/a |

Frames that outlive the loop: every http deadline sits in a session root, and phase 3 destroys
it on the manager thread with the scheduler alive; `drop_timer` sees `closed()` and leaves the
entry to the wheel (task_asio.hh:191), which phase 4 frees. The abandoned-stop path
(manager.cc:1087) never destroys the service: it `_Exit`s (:1188-1195). strata (pin 5d742b2) uses
`inline_scheduler` without deadlines; reactor (ab69cf2) arms none. I found no deadline that can
be destroyed after its scheduler, so no violation of D78.

Earlier fit findings: F2 (teardown must call `release()`) is met by `asio_scheduler::close()`
(task_asio.hh:111); F1 (the `arm_remote` race) is unreachable here since task never calls
`arm_remote` (off-loop filings are posted); F3 concerns AsioHost's asio timers and is unchanged.

Other behaviour Xapiand would notice: http deadlines fire up to 1 ms late, at slot ends (was the
OS timer's resolution); cancellation from any thread is now a posted unfile rather than an
off-thread `steady_timer` destructor (an Asio data race before, so this is stricter, not looser);
`run()` still returns once no deadline is filed (the wake is cancelled when the wheel empties).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | hazard | A wheel on a closed `asio_scheduler` never fires: `on_wake` is guarded by `!closed()` (task_asio.hh:206) and off-loop filings are dropped (:148). Xapiand runs the control io_context again after closing its scheduler (manager.cc:1119 close, :1131 `run_control` "the catalog's timers ... fire on the control loop"), and the registry depends on it (pool.cc:451 "the raw executor still runs once the loop's task scheduler has closed"). Today that path is a raw asio timer, so nothing breaks; moving `AsioHost::arm` or the registry timer onto `scheduler().wheel()` (which the earlier wheel fit review assumed) would hang Stop 3 until its deadline. | code reading | Keep `AsioHost::arm` on raw asio timers, or, when it moves, give the registry's post-close timers their own path; note the rule beside pool.cc:451. |
| N2 | no | nit | `task::deadline<engine::Host>` (take_turn, walk gate) now allocates a `timed_cancel_state` with a 32-byte wheel entry it never files, and always links even without a parent (task.hh:1046). It gets none of the wheel's allocation savings: each turn still pays AsioHost's `make_shared<State>` + `make_unique<Node>` + `make_unique<Armed>`. | task.hh:1013, 1046; host_asio.h:68-75 | None needed for this quest. If take_turn's deadline cost matters, give `AsioHost` the wheel path on the loop thread (subject to N1). |
| N3 | no | nit | API.md's "`sched` outlives it" reads as covering every `S`; for `deadline<engine::Host>` the binding lifetime is really the io_context (AsioHost's `~Armed` cancels a `steady_timer`) or the BlockingScheduler (`Armed` holds `owner_`). Xapiand meets both. | docs/API.md:40; host_asio.h:94-97; host.h:300-310 | Optional doc line: the scheduler's timer type decides what must outlive the deadline; for the task schedulers it is the scheduler. |

## Not run

- Detent's own tests, TSan, CI, and performance: not this lens.
- An end-to-end Xapiand build against this task commit (the pin is task:bdafa5f; other commits
  between it and this branch's `main` are outside this diff).
- Logger and Kronuz/scheduler timers: they do not use `task::deadline`.
