# task-lanes review: correctness

Commit: task:18eb51672e4f306aac508bd0444f6d367eac4291 (quest/xapiand/task-lanes, 18eb516)
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Build and suite at 18eb516: 261112-214929-dfac91. It runs `task_lanes_test` with every rule's test.
- Mutants at 18eb516, one per rule (261112-214931-341d4a through 261112-215008-8ab078): each test
  fails without the mechanism it covers. That covers lane-off order (`default-heavy`), equality
  (`eq-compares-lane`), the off-by-one at `light_slices` (`heavy-late`), the rearm (`rearm-stays-heavy`,
  `rearm-keeps-count`), the hop (`no-hop`) and the one-turn-per-pass drainer (`drain-runs-all`,
  `strict-priority`). I read `test/lanes_mutant.py`'s mutants against the tests; each mutant
  targets the line its test names.
- TSan suite at 18eb516: 261112-215012-f7d1c9 (`cross_thread_hop`: 8 posters x 10,000 cross-thread
  resumes, half heavy, each one counted to run exactly once).
- A3 A/B 261112-215014-07a8ed, GitHub CI 43374507023 and Detent's 275/275 (A4): accepted as stated.
  They are not correctness questions I reopened.

## What I checked myself

By reading `task.hh` and `task_asio.hh` at 18eb516, against the five named risks:

1. Lane bit. `vt_` is private. Every vtable read goes through the masking `vt()`
   (task.hh:209-237): `post`, `post_demoted`, `now`, `post_call`, `runs_here`, `roots`, `arm`
   and `cancel`. `heavy()` and `with_heavy()` are the only raw readers. `operator==` and
   `operator bool` read `self_` only (task.hh:238-239). Copies carry the bit: promise
   `continuation_scheduler`, event and lock waiters, `asio_park::sched_` and `spawn`.
   `static_assert`s pin vtable alignment and the 16-byte size. Where the bit drops, the doc says
   so: `schedule_on(of(s))` takes a caller-built reference ("Leaks are bounded"). A heavy
   `post_call` runs `fn` in a light scope. Its only user, the cancel emit (task_asio.hh:476-500),
   resumes through `asio_park::resume`, which posts a heavy reference (task_asio.hh:317), so
   the frame stays heavy.
2. Cross-thread posting. Only the loop touches `demoted_` and `draining_`. An off-loop
   `demote` posts a hop that re-enters `demote` on the loop (task_asio.hh:238-241) and rechecks
   `closed()`. `drain` pops before it resumes, so an entry cannot run twice. It reposts itself
   at the asio tail after each entry, so a light flood cannot starve the heavy lane
   (`heavy_progresses_under_flood` and the `strict-priority` mutant). Teardown drops pending
   hops the way it drops `post()`'s lambdas.
3. Budget. `spent_` is incremented before the comparison, so the `light_slices`-th spent slice
   posts heavy (`turns_heavy_at_light_slices` and `heavy-late`). `rearm_light(A)` with
   A < `light_slices` yields A more light slices, and `rearm_light(0)` makes the next spent slice
   heavy. A budget that never opts in takes `s.post_demoted(h)` exactly as on main. No scope
   becomes heavy without `light_slices` > 0 or an explicit `post_heavy`: the only heavy
   constructors are task.hh:1380 and the drainer. Its order is main's (`default_unchanged`,
   `default-heavy`).
4. The fast path. `current() == of(*this)` holds only inside a `running_on` of this scheduler.
   This library installs one only in its own `post`, `post_call` and `drain` handlers, and in
   `asio_park`'s inline resume, which first requires `runs_here()`. All of these run on the
   io_context's thread, so the fast path implies `running_in_this_thread()`. Nested schedulers
   and a scheduler built on one thread but run on another behave alike under both checks
   (probe below). Nothing in Xapiand, Detent, completion or consensus installs `running_on` with
   an asio_scheduler's own reference: Xapiand's uses are wrapper (Host) or pool references.
5. F12. The last `when_all_child` to decrement transfers straight to the parent
   (task.hh:1784). Inner `task` finals transfer whenever the scheduler matches, ignoring the lane.
   So the join continues in the scope of whoever decrements last, on every order. The parent's
   own decrement can be the last only when every child finished synchronously, and a child
   cannot turn heavy without suspending, so that case cannot strand a lane.

Probe job 261112-220308-cb453f (exit 0). Script:
`.scratch/review-task-lanes-correctness/probe.sh`, which builds `probe.cc` against the job
checkout's headers with asserts on. Its results:

- `when_all_light_last`: the heavy child finishes first and the light one last. The join is
  light, and the parent's next spent slice is heavy through the shared count. The rule holds on
  the order the suite does not test.
- `when_all_both_heavy`: the join is heavy.
- `rearm_above_light_slices`: with `light_slices` 2, `rearm_light(5)` gives `lHHHHH` (see N1).
- `default_budget_in_heavy_scope`: a default budget nested in a heavy scope posts heavy, and a
  default budget alone never turns heavy.
- `nested_schedulers`: inside A's scope, B is polled. B's checkpoint (fast path), a plain B
  handler's `A.post_demoted` (fast path, `current()` == A) and its `B.post_demoted` (slow
  path) all push synchronously, and each handle resumes once.
- `scheduler_on_other_thread`: the scheduler is built on main and run on another thread. Its
  checkpoints and a `post_heavy` from main each resume once, and `demoted()` returns to 0.

(Job 261112-220241-2774d3 failed on a probe bug, an io_context left stopped by `poll()`. The
probe was fixed and rerun as the job above. Not a finding.)

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | `rearm_light(allowance)` silently caps the allowance at `light_slices`. `rearm_light(5)` with `light_slices` 2 runs one light slice, the same as `rearm_light(2)`. The comment and docs/lanes.md say it "lets it run `allowance` more slices light". D82's allowance 4 under K 8 is unaffected. | probe `rearm_above_light_slices` (261112-220308-cb453f); task.hh:1365-1368 | State the cap ("at most `light_slices`") in the comment, docs/lanes.md and API.md. The alternative is to keep a separate remaining-light counter so allowances above K are honoured. The owner's call. |
| N2 | no | gap | F12 is tested only with the heavy child finishing last. The other order (a light last finisher leaves the join light, and the shared count turns the next spent slice heavy) is untested, though the code does it. | probe `when_all_light_last`; test/lanes_test.cc `when_all_join_lane` | Add the light-last order to `when_all_join_lane`. |
| N3 | no | nit | The fast path treats `current() == of(*this)` as proof of being on the loop. `running_on` is public, so a caller that installs this scheduler's reference on a foreign thread would push to `demoted_` without the hop. No current caller does. | task_asio.hh:238; grep of Xapiand, Detent, completion, consensus | Add a Debug-only `assert(!io_ \|\| io_->running_in_this_thread())` on the fast branch, or state the invariant beside `running_on`. |

## Not run

- No TSan build of my own. The race-relevant paths are those in 261112-215012-f7d1c9, and my
  probes add no new cross-thread interleaving beyond one off-thread `post_heavy`.
- No asio executor other than an io_context's, apart from what the suite's
  `unsupported_hop_asserts` covers. Xapiand's Host wrapper forwarding of `post_heavy` is
  http-lane's question.
- A3's numbers, the Linux, FreeBSD and arm64 platforms (CI's), and Detent's suite were not rerun.
