# task-lanes: journal

State: waiting on jobs 261112-204125-ade849 261112-204139-c030da 261112-204142-15b07c 261112-204146-f78c5d 261112-204150-66030b 261112-204154-c0d456 261112-204158-99a813 261112-204202-d4e265 261112-204208-6e7e46 261112-204212-fa53c8 261112-204217-2d75e1 261112-204222-565acd 261112-204230-c172ff 261112-204235-528ca4
Quest: .campaign/xapiand/quests/task-lanes.md (the lead's; never edit it)

## Doing

Jobs at task:5ae571a (pushed; CI on quest/**). Local RelWithDebInfo build: full ctest green except lanes_test's first run (missing io.restart(), fixed in 5ae571a); lanes_test 5/5 green after.

## Next

1. Read the jobs; fix any mutant that survives (a stronger test) or anything TSan reports.
2. Check CI on quest/xapiand/task-lanes.
3. When green: State "ready for review", with the sha for Detent (A4).

## Blocked on

Lead: the quest file's A1 bullet is on the same line as "Three to five, at outcome level:", so `campaign` does not parse A1 (`--for A1` is refused). build-test and the 11 mutants are submitted untagged; once A1 parses, they need `--for A1 [--fails-without]` reruns (same commands, below).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261112-204125-ade849. Mutants (command `python3 test/lanes_mutant.py NAME`, exits 0 only when its test fails on a CHECK, 2 on a build error): light-posts-demoted 261112-204139-c030da, drain-runs-all 261112-204142-15b07c, strict-priority 261112-204146-f78c5d, post-ignores-lane 261112-204150-66030b, no-hop 261112-204154-c0d456, default-heavy 261112-204158-99a813, eq-compares-lane 261112-204202-d4e265, rearm-stays-heavy 261112-204208-6e7e46, rearm-keeps-count 261112-204212-fa53c8 (F12), heavy-late 261112-204217-2d75e1, inline-heavy 261112-204222-565acd |
| A2 | TSan suite 261112-204230-c172ff (`cross_thread_hop`: 8 threads x 10,000 off-thread resumes) |
| A3 | resume-path ABBA vs task main, exclusive, 261112-204235-528ca4 (`test/resume_ab.py`) |
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
