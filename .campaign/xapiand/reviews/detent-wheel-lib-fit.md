# detent-wheel-lib review: Xapiand fit (A7, D71)

Commit: wheel:f5406791bbb2316f2a367769ed1e54c9142055e4
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Detent's A1 jobs as reported in wheel `docs/timer-wheel.md` (single row 261111-234624-f5cc8e,
  cross row 261111-233422-3a6c0b). Performance is not this review's question; I did not rerun them.
- The library's own tests as listed in `test/wheel_test.cc:1-11` (reentry, model, far, cross,
  drop, owner). The cross test arms unarmed entries only and never cancels or re-arms a
  remote-armed entry, and never runs past 64 producer threads, so it does not cover F1.

## What I checked myself

Probe job 261112-044021-5f777b, script
`.scratch/review-detent-wheel-lib-fit/run.sh` + `lag_probe.cc` (rerun:
`campaign job submit --level light --ref WHEEL=<wheel repo>@<commit> -- sh -c 'sh .../run.sh "$JOB_REF_WHEEL" "$JOB_OUT"'`).
It copies `wheel.hh`, inserts a pause hook between `arm_remote`'s CAS and its `fetch_or`
(wheel.hh:123-124; no other change), and forces the interleaving in F1. Result: the owner's
`cancel` of the handed-over entry is a no-op, and the next `expire` fires the cancelled entry
(exit 3); with the entry freed after the cancel, ASan reports heap-use-after-free in `drain()`
(wheel.hh:237 of the copy).

Every Xapiand master timer that would move onto the wheel, against the contract (one owner
arms, re-arms, cancels, expires; others `arm_remote` an unarmed entry only):

| Use | Arms from | Re-arms armed | Cancels from | Covered |
|---|---|---|---|---|
| `control::Periodic` (control_plane.h:143) | loop task, a fresh entry per tick | no | loop (frame resumes there: `task::event::set` posts, task.hh:1187-1188); teardown thread in phase 3 | yes; phase 3 needs `release()` (F2) |
| `control::Debounce` (control_plane.h:294) | loop task; `touch()` from any thread only moves `target` under a mutex and spawns | no (the watcher re-reads `target` and arms anew) | loop; teardown thread in phase 3 | yes; F2 |
| `reactor::PeriodicTimer` try-shutdown (manager.cc:184, 309, 708) | loop: `shutdown()` is posted through `shutdown_async` (manager.cc:612-614) | yes, `again()` on the loop | loop | yes (owner `arm` unlinks first); retiring it, a fire that re-arms itself is allowed |
| Index registry idle-close/commit (pool.cc:438-499) | `after()` any thread, then `asio::post` to the loop, armed there | no | loop (`fire` extracts, `close` posts `timers.clear()`) | yes; F3 for the posted clear |
| `task::deadline<engine::Host>` take_turn / walk gate (pool.cc:339, walk_gate.h:90) | loop (`Host::current()`) | no | loop; teardown thread in phase 3 | yes for Xapiand's host; F1 if task's "any thread" deadline (task.hh:1137) is built on `arm_remote` + posted cancel |
| `engine::SleepFor` (host.h:176, strata/shard.cc:494) | loop | no | loop; teardown | yes; F2 |
| http deadlines `task::deadline<task::asio_scheduler>` (Kronuz/http http_asio.h:366, 876; http_asio_tls.h:97) | loop coroutine | no (a new deadline per read) | loop; manager thread in `http_service->destroy_roots()` (manager.cc:1100) | yes; F2 |
| http deferred reply timer (http_asio.h:128, 143-148) | loop coroutine re-arms | yes, on the loop | producers post the cancel to the loop | yes |
| Kronuz/scheduler `Debouncer` (fsyncher, xapiand_fsyncher.h:73; wal.cc:108) | any thread, a fresh task per arm (debouncer.h:343) | no (busy touches move `target`; the task re-arms itself as a fresh one) | `clear()` flag, any thread | yes, with flag-cancel: the entry must stay alive until its deadline; F5 for `past_horizon` |
| Reactor lag probe (Kronuz/reactor reactor_lag.h:177) | loop | yes, loop | loop | yes |

No Xapiand use today re-arms an armed entry from another thread or cancels from another thread
while the loop runs; the only off-owner cancels are teardown's (F2). The contract is enough for
Xapiand once F1 is fixed, because F1 breaks the one cross-thread path the contract offers.

Expiry callbacks that arm or cancel other entries: allowed and tested (`reentry`, `model` arms
from fires); the due list is detached first (wheel.hh:139-161). An entry armed in a fire for a
passed deadline waits for the next `expire`, which `next()` puts at most 1 ms out.

Wheel shutdown with pending inbox items: the destructor drains and fires `(e, false)` for armed
and queued entries (wheel.hh:68-79, test `drop`), correct when every producer has returned from
`arm_remote`; a producer still inside it is F1's window.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Owner `cancel`/`arm` of an entry another thread just armed is not exact once stripes are shared. Only the push that finds its stripe empty sets the stripe bit, after its CAS (wheel.hh:123-124); a second thread on the same stripe pushes behind it, sets no bit and returns. If the first is still between CAS and `fetch_or`, the owner's `queued()` reads 0 (wheel.hh:96, 109, 218), so `cancel` is a no-op and the entry is later filed and fired; if the caller freed it, `drain` reads freed memory. Stripes are shared once 64 threads, process-wide, have ever called `arm_remote` (wheel.hh:209-213: one static counter, `% 64`), which a many-core Xapiand host with its pools reaches. This contradicts docs/timer-wheel.md:61-62 ("re-armed or cancelled exactly"), which is what task's any-thread `task::deadline` (task.hh:1137; Detent async-drivers D: "a deadline keeps task's contract ... on the wheel") must rely on for a deadline built off the loop and cancelled through a post. The same lag also leaves a handed-over entry unfiled while `next()` says `never`. | probe 261112-044021-5f777b (cancelled entry fires; ASan heap-use-after-free in `drain`) | Options: (a) `arm_remote` marks the entry queued before its CAS (a sentinel distinct from `armed()`), and the owner's arm/cancel of a marked entry drains every stripe head regardless of bits; (b) every push sets its bit (one more RMW per remote arm; rerun the cross row); (c) narrow the contract: a remote-armed entry is never cancelled or re-armed by the owner, it stays alive until it fires (flag-cancel), and docs/timer-wheel.md:61-62 goes. Test: deterministic, two threads sharing a stripe and a test hook between CAS and bit, as the probe does; the cancelled entry must not fire. |
| F2 | no | gap | Teardown cancels entries off the owner thread: phase 3 `destroy_roots()` runs on the manager thread (manager.cc:1100, 1119-1120; task_asio.hh:52-56) and destroys frames holding armed timers (control_plane.h:143, 294; pool.cc:339; walk_gate.h:90; host.h:176). The contract covers it only through `release()`, called after the loop joins and before `destroy_roots`, with every `arm_remote` producer returned. The docs say "once the loop is stopped and closed" but not "producers stopped". | code reading | Xapiand change: the wheel adapter behind `AsioHost::arm` (host_asio.h:65-92), or task's `asio_scheduler::close()`, calls `release()`; the library docs add that producers must have returned before `release()`/destruction. |
| F3 | no | gap | `DatabasePool::Timer::close()` posts `timers.clear()` to the control executor (pool.cc:483), which may not run again; entries can then outlive the wheel. The wheel's destructor unlinks them and fires `(e, false)` (wheel.hh:68-79), so a later destructor of the entry's owner must not call `cancel` on the dead wheel. | code reading | Xapiand change: the adapter's `fire(e, false)` marks its node dead so `~Armed` skips the cancel. Asio's timer has the same ordering hazard today, so not introduced by the wheel. |
| F4 | no | nit | A fire may destroy its own entry: Xapiand does (pool.cc:465 extracts and destroys the timer after its callback returns; a `task::deadline` request that resumes inline destroys its deadline). `expire` never touches an entry after its fire (wheel.hh:156-161), so it is safe, but docs/timer-wheel.md and the header say only "cancel or re-arm ... itself included". | wheel.hh:156-161 | Say "cancel, re-arm or destroy" and add the destroy case to `reentry`. |
| F5 | no | nit | No horizon: once Kronuz/scheduler moves onto the wheel, `past_horizon` and `DEBOUNCE_OR_RUN_PAST_HORIZON` (debounce_or_run.h:35-40, 55-56) and the Debouncer's horizon branches go dead. | debounce_or_run.h:35-56 | Xapiand change at migration: delete them with the scheduler's `within_horizon` (remove, not refuse). |
| F6 | no | nit | A `fire(e, false)` in the destructor that re-arms its own entry loops forever (wheel.hh:73-78 re-reads the bitmaps). No Xapiand fire re-arms on `false`. | wheel.hh:73-78 | Document that a `false` fire must not arm. |

## Not run

- Performance (A1 numbers, the pending cross-thread P=1 rerun): not this lens.
- TSan or the library's own suite, fuzz and CI: not rerun; I read the tests' scope only.
- Whether a stress run without the hook ever hits F1's window: the probe forces it; the window is
  two instructions, so a stress test would be timing-dependent.
- Kronuz/task's planned wheel adapter and Kronuz/scheduler's port: not written yet, judged only
  by their current contracts (task.hh:1113-1146, debouncer.h:272-354).
- Logger timers (Kronuz/logger): not traced.
