# task-lanes: journal

State: waiting on jobs 261112-214929-dfac91 261112-214931-341d4a 261112-214934-bb3d65 261112-214937-b6d183 261112-214941-85d939 261112-214945-9995a1 261112-214949-5b89c5 261112-214952-0d0642 261112-214956-c1b908 261112-215000-7f3020 261112-215004-e2d24e 261112-215008-8ab078 261112-215012-f7d1c9 261112-215014-07a8ed
Quest: .campaign/xapiand/quests/task-lanes.md (the lead's; never edit it)

## Doing

Jobs at task:18eb516 (pushed). A3 at 90b50f0 (261112-204947-85fbaf): demote 1.031x main (8 pairs 1.022-1.049, +5 ns), yield 1.009, inline_await 1.017, child 1.005. Cause: `io_->running_in_this_thread()` on every demotion (asio's call-stack TLS walk). Local probe, strict, 4 ABBA blocks, demote median ns: main 128.44, 90b50f0 130.27, fast path 128.85, fast path + 8-byte packed entries 128.12. 18eb516 takes the fast path only: inside this scheduler's own scope (`current() == of(*this)`, a checkpoint's case) it is on the loop, so the io_context is not asked. The packed entry (lane in the handle's low bit) is the fallback if A3 still shows a cost. resume_ab.py now runs an A/A arm (main against a second build of main) beside B/A, per Detent's request.

## Next

1. Read A3 261112-215014-07a8ed: per path B/A beside the A/A band. If demote is outside the band, try the packed entry.
2. Read A1/A2 jobs; CI at 18eb516.
3. When green: "ready for review", sha for Detent (A4).

## Blocked on

Nothing. (The lead fixed the quest file's criteria header; resubmitting with `--for A1 [--fails-without]` reused the identical 90b50f0 jobs and tagged them, so nothing was cancelled.)

## CI

Kronuz/task CI at 90b50f0: run 43367833692, success on every platform (freebsd, linux gcc/clang x86_64 and arm64, asan-ubsan, tsan, macos arm64/x86_64). At 5ae571a, run 43367102633 failed on linux-arm64-gcc only (the test race fixed in 90b50f0).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | At 18eb516: build-test 261112-214929-dfac91. Mutants (`python3 test/lanes_mutant.py NAME`, exits 0 only when its test fails on a CHECK, 2 on a build error): light-posts-demoted 261112-214931-341d4a, drain-runs-all 261112-214934-bb3d65, strict-priority 261112-214937-b6d183, post-ignores-lane 261112-214941-85d939, no-hop 261112-214945-9995a1, default-heavy 261112-214949-5b89c5, eq-compares-lane 261112-214952-0d0642, rearm-stays-heavy 261112-214956-c1b908, rearm-keeps-count 261112-215000-7f3020 (F12), heavy-late 261112-215004-e2d24e, inline-heavy 261112-215008-8ab078 |
| A2 | TSan suite 261112-215012-f7d1c9 at 18eb516 (`cross_thread_hop`: 8 threads x 10,000 off-thread resumes) |
| A3 | resume-path ABBA vs task main with an A/A arm, exclusive, 261112-215014-07a8ed (`test/resume_ab.py`), medians head/main [A/A range]: yield 0.995 [0.977-1.023], demote 1.013 [0.981-1.016], inline_await 1.009 [0.975-1.032], child 0.985 [0.979-1.029]; every path inside its A/A band |
| A4 | [manual] Detent's lead, 11-12: Detent's integration jobs (da2074 at 90b50f0, b7fe6c at 18eb516; Detent's repository) passed, Detent main's full suite with task pinned to quest/xapiand/task-lanes, 275 of 275 tests (test_local_trusted_paths skipped as on main) |
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
