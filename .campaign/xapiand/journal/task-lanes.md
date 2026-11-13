# task-lanes: journal

State: waiting on jobs 261112-204846-725ea8 261112-204849-91803a 261112-204854-7e293b 261112-204858-0e6b9d 261112-204902-8b592b 261112-204905-205bd3 261112-204909-7b597e 261112-204914-1d26c6 261112-204918-7d0c0f 261112-204923-7f2258 261112-204928-6befdc 261112-204935-9ea300 261112-204942-de21a3 261112-204947-85fbaf
Quest: .campaign/xapiand/quests/task-lanes.md (the lead's; never edit it)

## Doing

Jobs at task:90b50f0 (pushed; CI on quest/** running). At task:5ae571a every job passed: build-test 261112-204125-ade849, all 11 mutants killed (261112-204139-c030da ... 261112-204222-565acd), TSan 261112-204230-c172ff. CI at 5ae571a failed on linux-arm64-gcc only: `heavy_sticky_across_chore` raced (an idle loop ran the hop and the drainer before the second wake was posted). task:90b50f0 holds the loop in a handler while the test posts both wakes (20/20 local runs green). The header code is unchanged since 0484a73.

## Next

1. Read the jobs at 90b50f0 (CI there is green).
2. A3: read `build-resume-ab/ab.json` from 261112-204947-85fbaf; record the median B/A ratio per measurement, honest magnitude.
3. When green: State "ready for review", sha for Detent (A4).

## Blocked on

Nothing. (The lead fixed the quest file's criteria header; resubmitting with `--for A1 [--fails-without]` reused the identical 90b50f0 jobs and tagged them, so nothing was cancelled.)

## CI

Kronuz/task CI at 90b50f0: run 43367833692, success on every platform (freebsd, linux gcc/clang x86_64 and arm64, asan-ubsan, tsan, macos arm64/x86_64). At 5ae571a, run 43367102633 failed on linux-arm64-gcc only (the test race fixed in 90b50f0).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | At 90b50f0: build-test 261112-204846-725ea8. Mutants (`python3 test/lanes_mutant.py NAME`, exits 0 only when its test fails on a CHECK, 2 on a build error): light-posts-demoted 261112-204849-91803a, drain-runs-all 261112-204854-7e293b, strict-priority 261112-204858-0e6b9d, post-ignores-lane 261112-204902-8b592b, no-hop 261112-204905-205bd3, default-heavy 261112-204909-7b597e, eq-compares-lane 261112-204914-1d26c6, rearm-stays-heavy 261112-204918-7d0c0f, rearm-keeps-count 261112-204923-7f2258 (F12), heavy-late 261112-204928-6befdc, inline-heavy 261112-204935-9ea300 |
| A2 | TSan suite 261112-204942-de21a3 (`cross_thread_hop`: 8 threads x 10,000 off-thread resumes); green at 5ae571a (261112-204230-c172ff) |
| A3 | resume-path ABBA vs task main, exclusive, 261112-204947-85fbaf (`test/resume_ab.py`) |
| A4 | [manual] Detent's lead, 11-12: Detent's integration job 261112-205304-da2074 passed (exit 0), Detent main's full suite with task pinned to quest/xapiand/task-lanes@90b50f0, 275 of 275 tests (test_local_trusted_paths skipped as on main) |
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
