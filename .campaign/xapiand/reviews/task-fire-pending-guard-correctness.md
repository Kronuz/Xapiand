# task-fire-pending-guard review: correctness

Commit: task:f937bc2f513ff83e1cd810d528c79f0b55434c04
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-145232-a31610: Debug suite at f937bc2 (asserts on), including debounce-overlap and the
  debounce-reentry fork death test.
- 261119-145249-853290 and 261119-145234-48e3e7: TSan, and TSan x50 of the timers cases.
- 261119-145237-42104f (no assert) and the old-starvation mutant 811bb9 fail, so the reentry and
  overlap tests detect what they claim.

## What I checked myself

Probe 261119-150725-8607f3 (`.scratch/xapiand/review-task-fire-pending-guard-correctness/run.sh`,
probe.cc), built Debug and Release (`-DNDEBUG -Wall -Wextra -Werror`, clean):

- throw-then-other: A's loop fire throws; B's later fire on the same thread calls
  `A.fire_pending()`, which takes A's pending key without an assert. The mark is popped on the
  throw path.
- nested-true: A's fire runs a nested loop whose B fire calls `A.fire_pending()`. In Debug it
  aborts. In Release it hangs until the probe's SIGALRM, so this is a true positive: the call would
  wait for A's own fire.
- nested-same-ended: once A's fire has ended on the loop thread, C's fire there calls
  `A.fire_pending()` with no assert.

Reading task_timers.hh:254-272,301-307,367-387,415 for the ticket list:

- Every list field (`oldest_`, `newest_`, `tickets_`, `prev`, `next`, `ticket`) is read and written
  only under `m_`. Tickets are issued and appended at the tail under that same mutex, so the list
  stays in ticket order, and unlinking from the middle keeps that order.
- `fire_pending()` reads `last = tickets_` under the same lock as its take. Every fire whose
  `begin_fire` came before the take therefore has `ticket <= last` and is in the list.
- The predicate `!oldest_ || oldest_->ticket > last` turns true only when the oldest node is
  removed, which is the one case that notifies. Once true it stays true, because later tickets are
  larger. So it neither misses an earlier fire nor waits on a later one, and N2's starvation is
  gone apart from ordinary mutex fairness.
- On a throwing fire, `end_fire` unlinks the node and restores `here_` before `hand_off` and the
  rethrow. No `co_await` comes between `begin_fire` and `end_fire`, so the node never outlives the
  fire and `here_` cannot cross threads.
- `owner` and `outer` are read lock-free only by the thread that wrote them.

N3 contract text (lifecycle.md:53-64, API.md:111-113, task_timers.hh:250-253) is accurate:

- A touch after the take makes a new entry, which the loop may fire alongside `fire_pending()`.
- After the stop, a watch that passed its stop check before the take is a fire in progress, so the
  call waits for it. A watch that locks after the take sees the stop (the mutex orders it after
  `request()`) and does not fire.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | nit | The assert costs something in release: `begin_fire` and `end_fire` each write the `thread_local here_`, and each node carries 2 extra pointers, even under NDEBUG. The cost is not measured but trivial next to the mutex each fire already takes. Keeping it unconditional keeps one layout across NDEBUG and Debug translation units. | task_timers.hh:301-307,367-380,415 | None; if it is ever wanted, guard only the `here_` writes, never the struct fields. |
| R2 | no | nit | The assert catches same-thread reentry only. A fire that hands `fire_pending()` to another thread and blocks on it still deadlocks with no diagnostic. The text says "a fire's own" thread, so this matches the contract. | task_timers.hh:250,255 | None. |
| R3 | no | nit | If `end_fire`'s `lock_guard` threw (`std::mutex::lock` failing), the node would stay linked in a dead frame. The old counter would have hung in the same case. This is practically unreachable. | task_timers.hh:375-380 | None. |
| R4 | no | nit | The probe's output repeats lines because stdout is not flushed before `fork()` (cosmetic, in the probe, not the quest). | probe.cc `in_child` | Flush before `fork()` if the probe is reused. |

## Not run

- No CI and no other platform; the probe ran on macOS arm64 with Apple clang only.
- No measurement of R1's release cost.
- No nested reentry of the same debounce on one thread (A's fire stepping A's own loop); the chain
  logic is the same LIFO as the cross-debounce nesting the probe covered.
