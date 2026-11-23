# task-timers-followups review: correctness

Commit: task:340746a (code at task:2012f28; 340746a adds docs only)
Verdict: CHANGES
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Debug timers_test 261120-234149-e25be0 at 2012f28: every subtest, both reentry cases abort
  (`Abort trap: 6`), held 0 early watches, held-twice waited.
- suite 261120-234150-5f88f9, tsan 261120-234151-7a5529, TSan x50 of the five concurrent subtests
  261120-234631-cf37c8, debounce-mid-fire x50 at 8 on 4 cores 261120-234629-234a67 (A3: the test
  waits on events, no timing margin).
- Mutants (Q4): each builds, then fails on its CHECK, and each mutates what it names (diffed
  against 2012f28): no-assert 261120-234620-0cc4fe (timers_test.cc:617, the loop case hangs to
  SIGALRM), wait-all 261120-234623-f45356 (:487), erase-taken 261120-234625-726b55 (:531, 2 at
  once), no-ticket 261120-234627-d28a8b (:561, 2 at once).

## What I checked myself

Scripts in `.scratch/xapiand/review-task-timers-followups-correctness/` (home checkout); rerun
with `campaign job submit --quest task-timers-followups --level light --fresh -- sh $D/run.sh
$D/probe.cc tsan 3` (and `debug 4`), `sh $D/deadlock.sh $D/deadlock.cc`, `sh $D/stress-only.sh
$D/probe.cc --commit <mutant>`.

- Every path that reads or erases `keys_` (touch, fire_pending's take, watch's gen/stop/throttle
  checks, hand_off's two forms), by reading: a held entry has gen 0 and only hand_off(key, 0) at
  its last holder changes that or erases it; a stale watch, a throwing loop fire's hand_off(key,
  G) and the stop path all miss it on the gen check; `pending_` stays consistent. No path fires a
  key twice at once, loses a touch, or spawns a watch after a stop.
- probe.cc deterministic cases, Debug and TSan (261121-003813-2f06ef, 261121-003816-fd80d3), all
  pass: stop-held (touch mid a held fire, then stop: no watch, pending kept, next call fires it
  with its flags), throttle-held (throttled entry taken: one off-loop fire, the throttled watch
  ends), touch-in-fire (a call's fire touches its own key: the loop fires it once after), throw-held
  (a throwing call fire with both keys re-touched: both fire, rethrown, both handed off),
  loop-throw (a throttled loop fire throws while a call holds its re-touched key: hand_off(key, G)
  misses, the call fires it, no overlap).
- probe.cc stress against a model: asio loop, 3 touchers and 2 fire_pending() threads on 2 keys,
  throttle 0/300 us, throwing fires, stop mid-run or quiescence without stop; checks no key fires
  twice at once, every successful touch fired after it (and without a stop, nothing pending after
  quiescence), no loop fire after stop + flush, group joins. 40 Debug + 30 TSan runs pass
  (~1000 loop and ~1000 off-loop fires, ~7000 touches mid an off-loop fire per run, calls that
  waited reported). Sensitivity: it fails "TWICE" on every config at mutants erase-taken
  (261121-003840-1f9d60) and no-ticket (261121-003842-2468e4).
- Q3: the diff leaves watch() and touch() byte-identical but comments; hand_off's new branch needs
  gen 0, which a loop watch never has. Loop fires, debounce targets and throttle are unchanged for
  callers that never call fire_pending(); the suite at 2012f28 confirms.
- Q2: deadlock.cc, two calls on two non-loop threads (261121-003326-2d5f3e at 340746a,
  261121-003327-363699 at task main a4ba197): both cases deadlock at 340746a and return at main.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A call now waits for every earlier call's fires, which adds deadlocks beyond the documented loop-thread one, with no loop-thread call: (1) "another debounce's fire may call it": d1's call fires a key whose fire calls d2.fire_pending() while d2's call on another thread fires a key whose fire calls d1.fire_pending(); (2) a caller holding a mutex across fire_pending() that an earlier call's fire takes, even with nothing pending. Both returned before this quest. The docs and header state only the loop-thread rule, and still allow (1). | task_timers.hh:279, :251; docs/API.md:118; docs/mechanisms/lifecycle.md:67-69; jobs 261121-003326-2d5f3e (DEADLOCK x2) vs 261121-003327-363699 (both return) | Owner's call between: (a) keep the code and state the general rule in the header, API.md and lifecycle.md: a call waits for earlier calls' fires, so its caller must not hold anything such a fire waits for (the loop thread, a lock, a fire_pending() of another debounce whose call's fire calls back), and drop or qualify "another debounce's fire may call it"; or (b) narrow the call-to-call wait to calls holding a key this call took (the double-fire case A2 needs), which keeps the old allowance but gives up "earlier calls' fires" for keys it did not take; add deadlock.cc's two cases as tests that return. Either way the stated contract must match the code. |
| F2 | no | gap | The concurrency proof standard asks for a stress against a model with two or more workers per object under TSan; the suite has deterministic tests and TSan repeats, no stress. probe.cc's stress covers it at this commit and catches both double-fire mutants. | probe.cc `stress`; 261121-003816-fd80d3, 261121-003840-1f9d60 | Optionally adopt the stress into timers_test (reporting coverage, failing only on model violations). |
| F3 | no | nit | If `group_.spawn` throws (allocation) inside the hand-off loop, the remaining taken keys keep `held > 0` forever (no loop watch until another fire_pending()), and the fires' first exception is replaced. | task_timers.hh:292 | Catch per hand-off, or document it with the existing allocation-failure stance. |
| F4 | no | nit | API.md says a touch of a taken key "fires on the loop after it"; it may instead be taken by a later call, or kept for the next call after a stop. | docs/API.md:117-118 | "fires after it, on the loop or in a later call". |
| F5 | no | nit | Pre-existing: an off-loop fire sets no throttle and a throttled, re-touched key fire_pending() takes fires at once, so "with `throttle`, a key fires at most once per `throttle`" holds only for loop fires. | docs/API.md:115; journal ruling | State it for loop fires. |

## Not run

- No platform other than macOS arm64 (CI's A5 covers the rest); no g++ TSan.
- The probes run on asio_scheduler and manual schedulers only, not on Xapiand's real loop or the
  lifecycle (no caller of fire_pending() exists in Xapiand yet).
- Probe jobs 261121-003052-c8594d, -003216-ce436c, -003643-b3946f and -003053-d19ac6/-003229-05ec1e
  ran an earlier probe.cc whose per-thread generator was never constructed on worker threads (one
  key, no sleeps); they passed but are superseded by the v4 jobs above.
