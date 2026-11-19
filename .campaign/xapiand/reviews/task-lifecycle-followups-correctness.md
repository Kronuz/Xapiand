# task-lifecycle-followups review: correctness

Commit: task:8cf6793 (code at task:4c73dbb; 8cf6793 is docs only)
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261119-140132-6f55be and TSan check 261119-140132-d04ab6 at task:4c73dbb (8cf6793 changes
  only docs/API.md and docs/design/service-shape.md).
- ASan+UBSan 261119-140207-206045; TSan x50 of the concurrency tests 261119-140209-c56112.
- Mutants, each failing on its own CHECK per the journal: A1 9aee97, A2 816688, A3 8db73c, A4
  04f388; A5 bc8368 (flags), e85702 (throttle), 3b9118 (fire_pending's wait), 7d67fb (stop keeps
  pending), d2568f (fired once), a8816d (touch after stop), b19aab (throwing fire hand-off).

These cover each criterion's behavior and the two hook-pinned interleavings; what follows is what
they do not establish, settled by reading. No probe job was needed.

## What I checked myself

Read `git diff fb32493 8cf6793` (task.hh, lifecycle.hh, task_timers.hh, both tests, three docs),
the quest file, quest 1 and D5/D8/quest 10 of docs/design/lifecycle-adoption.md, the journal's
rulings, and the old debouncer at the Xapiand pin (CMakeLists.txt:552, scheduler:d57e8fc
debouncer.h).

1. **Stop keeps pending keys (ruling).** No leak: a stopped watch returns and leaves only a
   pending entry in `keys_` (task_timers.hh:314), freed by `fire_pending()` or the map's
   destructor; throttled entries are erased (same line). No double fire: `fire_pending()` erases
   each taken entry under `m_` (:255-260) and every watch rechecks `find(key)` plus `gen` before
   it fires (:311), so a sleeping watch, a post-fire throttle loop or `hand_off` (:363) finds
   it gone. A key mid-fire on the loop is already non-pending (:325), so it is not taken. No fire
   after `fire_pending()` returns when it is called after the stop: a watch decides `due` and
   `++firing_` under `m_` after checking `stop_` (:311-329), so either it saw the stop (no fire)
   or `fire_pending()` sees `firing_ > 0` and waits (:265-267). A touch racing the stop cannot slip
   in after the take: the stop request happens before the call, and the take's unlock orders any
   later touch's `stop_.requested()` check (:230) after it. `pending_` stays consistent: +1 only on
   non-pending to pending (:236-237), -1 at due (:326), reset with the take, and `hand_off` and the
   stop path erase only non-pending entries.
2. **fire_pending's wait (ruling).** From the loop outside a fire it cannot deadlock: `++firing_`
   (:327) to `fire_ended()` (:345, :349) is synchronous with no suspension, so on a single-threaded
   group scheduler `firing_` is 0 whenever any other code runs on that thread, including the
   lifecycle's inline-flush path (Q3), which runs `set.flush()` from the lifecycle frame. From
   inside a fire it self-deadlocks, as the comment (:249) states; see N1. In the fsyncher plan
   (quest 10) the fire only hands an fd to a WorkPool, so the wait is short.
3. **Throttle beats force (ruling).** Matches the old debouncer: `throttle_locked` sets
   `status.target = now + throttle_time` (debouncer.h:251-253), and a touch on a throttler computes
   `min(now + debounce_timeout, max_wakeup)` (:303-312) then raises it to `status->target`
   (:321-323), so the throttle's end wins over the force. New: `max(e.target, min(now + quiet,
   e.force))` (task_timers.hh:239). The force is re-drawn on that touch in both. See N7 for the
   one config difference.
4. **Q1 `cause()`.** `failure_` and `caused_` are written once, together, under `m_`
   (lifecycle.hh:245-250), and `cause()` reads both under `m_`, so two concurrent `fail()`s and a
   signal linearize at the first `fail()`'s lock: its `caused_` is whether the source's stop was
   already requested then, and neither field changes afterwards. `run_lifecycle` reads `cause()`
   only after the source's stop is visible (lifecycle.hh:528), and `fail()` stores before
   `request_stop()`, so the causing failure is always present and any later `fail()` sees the stop
   and gets `caused_ = false`. Stable. See N4, N5 for edges.
5. **Q2 `abandoned`.** Set only from `abandon.requested()` after the timer ended (:549), and the
   timer requests `abandon` only when `sleep_for(grace, finish)` runs out (:445), which `finish`
   (requested right after `drain_all` returns, :547) prevents; `drain_all` resumes the lifecycle by
   `task<>` completion, not a post, so no loop turn lets the timer fire between the last drain and
   `finish`. Never set on a clean path; set on every path whose drains outlive the grace, with or
   without a hurry.
6. **Q3 `flushed_inline`.** `off_loop` records the error only in the `std::system_error` catch
   that runs `f` inline (:471-474); the thread path leaves it null, and the lifecycle copies it
   after the await on every path (:557). It is the only inline path.
7. **F6 `queued()`.** No caller outside tests: no `inline_scheduler` use in Xapiand's src or tests,
   nothing in task.hh calls it; in task's tests it gates a yield (test/lifecycle_test.cc:122) and a
   `while (queued()) run()` (test/test.cc:627), which terminates because every `run_until` step
   takes the inbox (task.hh:468). Count assertions in sync_test and deadline_test pass in the suite.
   Idle detection only becomes more accurate.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | `fire_pending()` called from a fire hangs forever with no diagnostic; the rule lives only in a comment. | task_timers.hh:249,265-267 | A Debug assert from a thread-local "in a fire" marker set around `fire_(key, flags)`. |
| N2 | no | nit | The wait is for `firing_ == 0`, not for the fires in progress at the call; before a stop, back-to-back loop fires can win `m_` after each `notify_all` and starve the caller. Every planned caller runs after the stop, where `firing_` only falls. | task_timers.hh:267,353-355 | A fire generation counter, if a pre-stop caller appears. |
| N3 | no | nit | Without a stop, a touch after the take can make the loop fire the same key while `fire_pending()` fires it on its own thread; the doc's ordering covers only earlier touches. | task_timers.hh:252-267 | Document it beside `fire_pending()`. |
| N4 | no | nit | A start that throws because the causing `fail()` stopped it puts that start error ahead of the cause, as the documented ranking says. | lifecycle.hh:512-517,528 | None, or rank a stop-induced start throw below `cause()`. |
| N5 | no | nit | With a parent-linked source, the parent's request reaches `stop_` by callback, so a `fail()` in that window counts as the cause. Harmless: it is a real failure. | lifecycle.hh:248; task.hh:1379-1384 | None. |
| N6 | no | nit | `queued()` stays `noexcept` while it now locks `inbox_mutex_`; a lock failure terminates. | task.hh:487-494 | None, or drop `noexcept`. |
| N7 | no | nit | The old debouncer ignored a throttle no longer than the quiet (debouncer.h:244-247); the new one keeps the entry for any `throttle > 0`. Timing is the same when `min_force >= quiet`, which the old asserts required (:85-89); only entry lifetime differs. | task_timers.hh:328-332 | None, or assert the old order. |
| N8 | no | nit | `debounce_mid_fire` holds the loop's fire at most 300 ms for the other thread to reach the hook; on a loaded host it can fail spuriously (fails, never hangs). TSan x50 was clean. | test/timers_test.cc:378 | Wait for the hook without a deadline, or a longer one. |

## Not run

- No probe jobs: reading settled every named risk. No other platform, no Debug-asserts build, no
  CI (the branch is not pushed).
- The consumer contract (fsyncher-on-task A1: every fd fsynced before `flush()` returns) is quest
  10's; this review only checks that `fire_pending()` gives it a sound base.
- `off_loop` catching only `std::system_error` (a `bad_alloc` from `std::thread` skips the
  flushes) is older than this quest and outside it.
- The branch is behind main by 6 files (`campaign status`); the merge is the lead's.
