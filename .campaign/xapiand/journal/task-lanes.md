# task-lanes: journal

State: working
Quest: .campaign/xapiand/quests/task-lanes.md (the lead's; never edit it)

## Doing

Building task:0484a73 locally (first compile), then the jobs.

## Next

1. Local build and lanes_test green; fix what fails.
2. Jobs: build-test (A1), 11 `--fails-without` mutants (A1), TSan light (A2), resume A/B exclusive vs main (A3).
3. Push quest/xapiand/task-lanes; CI on quest/**.
4. "ready for review" with job ids and the sha for Detent (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending: Detent's lead names the job at the sha |
| A5 | `docs/lanes.md` (task) |

## Result

Pending.

## Rulings I made

- R1 `post_heavy(h)` is a new optional scheduler member (vtable entry, falls back to `post_demoted`, then `post`) instead of a lane argument on `post_demoted`: wrappers (Xapiand's Host) forward it like `post_demoted`, and one that does not still keeps today's order. `asio_scheduler::post_demoted` and `post_heavy` share one queue whose entries carry the lane.
- R2 `rearm_light(allowance = all)`: the scope runs `allowance` more slices light; 0 makes the next spent slice heavy (not "0 = full K" as F13 suggested; explicit beats a sentinel). `rearm_light(4)` means what Xapiand's D82 says.
- R3 F12 settled by stating the rule, not changing `when_all`: a join continues in the last child's lane (as any completion does, and as D2 g already wants for `co_await child`), and a rearm covers what starts after it. Test `when_all_join_lane` shows a parent-side rearm starts every chunk light, and that a rearm inside the sibling read leaves the next chunk heavy. A `when_all` reset alone would not fix it: the heavy child keeps spending the shared count after the sibling's rearm. For the Xapiand half (lead): `rearm_light(REQUEST_STREAM_SLICES)` belongs in RESTORE's loop before the `when_all`, not in `Request::read_chunk`.
- R4 `demoted()` counts an off-thread entry when the hop reaches the loop (`cross_thread_hop` relies on it to see the hop deterministically).
- R5 The lane tests live in their own binary, `test/lanes_test.cc` (`task_lanes_test NAME` runs one), so a mutant job runs only its test. AGENTS.md's repo map is outside the file set: the lead may add the line.
- R6 The io_ null case: a Debug assertion fires for an off-thread call made from another scheduler's scope (a bare thread with no scope cannot be told apart from the loop there).
