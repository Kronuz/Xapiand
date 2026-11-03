> Status: CURRENT (2026-11-03) the developer's ledger for making local master clean under the Linux sanitizers; local branch `fix/linux-sanitizers`, not pushed.

# linux-sanitizers (build): master clean under ASan, UBSan and TSan on Linux

Branch `fix/linux-sanitizers` in `Xapiand-wt/linux-sanitizers`, from master `Xapiand:4f873c0e2`. Linux runs: the shared AWS lease (`strata/tools/vm/aws-linux.sh`, c7a.4xlarge, Ubuntu 24.04, kernel 7.0) with a git bundle; clang 20 + libc++ 20 and g++-13 (the CI compiler) with CI's sanitizer flags (`ci.yml`: `-DASAN=ON -DUBSAN=ON` / `-DTSAN=ON`, RelWithDebInfo, `-DLTO=OFF`; `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`, `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; UBSan is `-fno-sanitize-recover=all`). Builds use `-g1` to fit the disk; sanitizer semantics are unchanged.

## State

Items 1 to 5, the `test_node` segfault and a debug abort in the shutdown drain (both found on the way) are closed, each with a failing-before run, except item 1, which master already fixed (below). The `test_node` fix needs a push of the traceback library first (Next). Not finished: a full `ctest` per sanitizer on the final commits (Blocked on). The g++-13 hang in `test_pool` released_views is not touched and nothing here explains it; the one thing in common is the call site `DatabasePool::run_detached`, whose no-control fallback is the nested `sync_wait` the hang sits in (row 7).

## Findings and fixes

| # | Finding | Root cause | Fix | Fails before / passes after |
|---|---|---|---|---|
| 1 | clang UBSan aborts on every JSON response at RapidJSON `stack.h:117` | RapidJSON v1.1.0 `Stack::Reserve` computes `stackTop_ + sizeof(T) * count` with both pointers null on the first push: null plus a non-zero offset. Upstream fixed it after the 2016 tag; the pin `24b5e7a` (Tencent master, 2024-12-22, still upstream's head: `compare` shows 0 commits ahead) compares `std::ptrdiff_t` sizes instead | none left to make: Xapiand:52235a037 (on master) moved the pin; no suppression, no patch of fetched source | standalone `Writer<StringBuffer>` under `-fsanitize=undefined -fno-sanitize-recover=all`: v1.1.0 + clang-18: `stack.h:117:13: runtime error: applying non-zero offset 16 to null pointer`; v1.1.0 + clang-20 does not compile (`document.h:319`); pin + clang-18, clang-20, g++-13: clean. v1.1.0 + g++-13 is clean too: **CI's UBSan job (gcc-13) never hit it**; gcc has no null-offset check |
| 1b | clang 18 UBSan also flags LZ4 (`lz4.c`, v1.8.1 pin) | null plus zero offset in `LZ4_compress_fast_continue` and friends | not changed (see Decisions) | standalone: clang-18 `lz4.c:549:44: applying zero offset to null pointer`; clang-20 and g++-13 clean |
| 2 | intermittent `IndexChores ended with work outstanding` abort at teardown | a chore resumes its awaiting task from the worker (`ChoreAwaiter::invoke` posts the resume), and the worker then leaves the item. An owner that drops its `IndexChores` at once checks `group.idle()` while the worker is still inside the item: not idle, abort. `IndexChores` now counts chores whose awaiter has not resumed (`awaited_`, up and down in the awaiter) and the destructor checks that; joining the group still waits for the worker | `pool.cc`, `pool.h` | `test_a_chore_is_over_when_its_task_resumes` (a host whose post lingers 150 ms after resuming the task on another thread): reverted fix, 3 of 3 runs, `IndexChores ended with work outstanding: a chore is queued or running`; with the fix, clang-20 ASan+UBSan asserts on, 8 of 8 `test_pool` runs 232 checks pass. Mutant: delete the `fetch_add` of the awaiter: `test_pool` aborts (rc 134) |
| 3 | `test_a_registry_that_ends_with_work_outstanding_aborts` fails 5 checks under ASan | the test's child does `freopen(log, "w", stderr)`; on Linux that leaves stderr fully buffered and `abort()` does not flush, so the "ended with work outstanding" message never reached the log (strace: the child aborts with no `write`). Not ASan-specific | `setvbuf(stderr, nullptr, _IONBF, 0)` in `in_child`; the `chores` scenario held its chore through the group, which the new count does not see, so it holds it through `run()` | before: 3 of 3 runs, 5 of 231 checks fail (empty logs); after: passes |
| 4 | `path_boundary_check.sh`: 14 failures on Linux | not the server. The check puts its data directory under `${TMPDIR:-.}`; macOS sets `TMPDIR` to an absolute path, a Linux shell has none, so `D` was relative and the check's "absolute" spellings (`"$D/out"`) were relative index paths inside the data directory, which the server rightly serves. `realpath` and normalization are the same on both | `D` is resolved to an absolute path | Linux, `TMPDIR` unset: before 14 FAIL, after 0 FAIL / 143 ok; before with an absolute `TMPDIR` on Linux: 0 FAIL (so no Linux difference in the boundary); macOS with `env -u TMPDIR`: the same 14 before, 0 after |
| 5 | g++-13 TSan flags `test_pool.cc:183` | a real report, one level up from a false positive: waiters of one failed commit share one exception object; each thread read `error.kind` in its catch, and the last to leave frees it through a refcount in libstdc++ that TSan does not instrument (`free` by thread T406 against the read by T405, report below). The order exists at run time (the refcount is an acq_rel atomic), TSan cannot see it. A standalone program with the same two-threads-one-exception pattern did **not** reproduce the report, so this is shown by the fix, not by a stand-alone repro | `Running` keeps the `exception_ptr` and `kind()` reads it after the join | before: `ThreadSanitizer: data race ... Write of size 8 ... by thread T406: #0 free ... #1 operator() test_pool.cc:183`, rc 66; after: no report, 38 of 38 `test_pool` cases pass on their own under TSan (the 39th is the released_views hang) |
| 6 | (from the coordinator) `test_node` segfaults on g++-13 Release | `traceback::collect_callstack_sig_handler` (SIGUSR2, all-thread dump) reads the interrupted thread's `rbp` as a frame pointer and dereferences it. Code built without frame pointers keeps anything in `rbp`: gdb at the fault, `rbp = 0x7fffffffffffffff` in a futex wait inside an LTO-inlined `WorkPool::park`. The value is above the stack, so the existing `frame < stack` check passes | traceback library (separate repo, local branch `fix/handler-frame-pointer`, commit traceback:64cc105): the frame is read only when aligned and `mincore` says both words are mapped | traceback `traceback_hooks_test` (a registered thread calls the handler with a context whose `rbp` is junk), g++-13 Release: before rc 139 (SEGV), after rc 0; mutant (mincore result ignored): rc 139. Xapiand `test_node`, g++-13 Release with LTO (the default): before rc 139, "the child ended with signal 11" in 4 tests; after 3 of 3 runs rc 0, 85 checks. The hang masked it; it fails with LTO on (CI's build-test job builds the default) and passes with `-DLTO=OFF` |

| 7 | `test_node` shutdown tests abort under load on clang-20 ASan (`ctest -j6`: 2 of 76 checks fail, "the child ended with signal 6"; alone 5 of 5 runs pass) | the shutdown clears the pool's control and drains the control loop on its own thread (`XapiandManager::drain_control`); a debounced commit that fires in the drain calls `DatabasePool::run_detached` with no control left, whose fallback `sync_wait` then runs on a scheduler thread: debug builds abort ("engine::sync_wait blocks on database work on a task scheduler's thread"; release builds log it once). The message was lost by the same `freopen` buffering as row 3, in test_node's children too | `run_detached` marks that fallback as allowed to block (it runs on the thread that starts it, by design: Xapiand:fcbeb8ebc); `setvbuf` in test_node's children | `test_a_detached_task_may_block_once_the_control_is_gone` (a task on an inline host calls `run_detached` with no control): reverted fix, abort with that message; with the fix `test_pool` 233 of 233 checks |

The traceback test file also had its asserts around calls with side effects (`pthread_create`, `sigaction`), so its default Release build never started the worker and hung; it checks with its own macro now (traceback:ec91452).

## Sanitizer runs

| Config | Result |
|---|---|
| g++-13 ASan+UBSan, CI's job exactly (asserts off): smoke, load, docs E2E, TLS | GREEN, 0 UBSan reports |
| g++-13 TSan, CI's job exactly: smoke, load, docs E2E, TLS | GREEN |
| clang-20 + libc++ 20 ASan+UBSan, asserts on: smoke, load, E2E, TLS (before the fixes) | GREEN |
| clang-20 ASan+UBSan, asserts on, `test_pool` | 232 of 232 checks, 8 of 8 runs |
| g++-13 TSan, `test_pool` per case | 38 of 38 pass; `test_released_views_are_reclaimed_by_the_housekeeping` hangs (the g++-13 hang) |
| clang-20 ASan+UBSan, asserts on, whole ctest, `-j6`, at Xapiand:03036aa19 | 33 of 35 pass; `test_node` (row 7) and `test_engine_search_strata` fail. The second is `CHECK(elapsed < 800 ms)` at `test_engine_search_strata.cc:1406` under six parallel ASan tests; not re-run alone, so it is not shown to be only load |
| g++-13 ASan+UBSan (CI flags, asserts off, so the registry tests are compiled out), whole ctest `-j6`, same commit | 32 of 35 pass: `test_blocking_guard` times out and `test_pool` fails (both the g++-13 hang, not mine); the third failure was not read before the AWS session expired |
| macOS (Apple clang, RelWithDebInfo, asserts on), `test_pool` and `test_node`, at Xapiand:03036aa19 with the local traceback | 232 of 232 and 85 of 85 |

## Decisions to review

1. **LZ4 v1.8.1 is flagged by clang-18 UBSan (null plus zero offset); clang-20 and gcc do not flag it.** Not changed: LZ4 is third-party at an old tag, CI is gcc-13 and the Linux gate is clang-20. Moving the pin to a current LZ4 changes the xxhash that Xapiand takes from its `lib/` directory; recommend leaving it unless clang-18 becomes a gate.
2. **The traceback fix is a commit in the traceback repository, not pushed.** `CMakeLists.txt` pins `64cc1052d4fb99fcf523d6ca1b4ad54b99ac63e0`; a build before that commit is pushed cannot fetch it. Push traceback `fix/handler-frame-pointer` (or merge it to its main), then this branch builds as is.

## Done

Xapiand, branch `fix/linux-sanitizers`:

1. Xapiand:a868f2145 path_boundary_check: absolute data directory (row 4)
2. Xapiand:a85494c6c test_pool: unbuffered stderr in the at-end children (row 3)
3. Xapiand:5192d1f47 IndexChores counts chores by their resume, with its test (row 2)
4. Xapiand:c83a6278b `Running` reads its error after the join (row 5)
5. Xapiand:03036aa19 CMake pin of traceback at traceback:64cc105 (row 6; not fetchable until traceback is pushed)
6. Xapiand:f5b67bebb test_node: unbuffered stderr in its children (row 7)
7. Xapiand:a48ee6f89 `run_detached` may block once the control is gone, with its test (row 7)

traceback, branch `fix/handler-frame-pointer` (worktree `traceback-wt/linux-sanitizers`, from the pinned traceback:7f3d7ba): traceback:ec91452 the hooks test checks with its own macro and tests a junk frame register; traceback:64cc105 the handler reads the frame only when mapped.

## Next

1. The lead pushes traceback:64cc105 (its `fix/handler-frame-pointer`), then reviews and merges this branch locally.
2. Whoever has AWS access: rerun the whole ctest per sanitizer at the final commit (`ctest -j4`; g++-13 ASan+UBSan and TSan with CI's flags, clang-20 ASan+UBSan with asserts on), read the third g++-13 ASan failure, and rerun `test_engine_search_strata` alone.

## Blocked on

Nothing needs a decision. The AWS session expired (`aws login` needed) while the whole-ctest sweep ran, and the lease was recreated once under me (the first lease reached its four hours), so the sweep above is at Xapiand:03036aa19, before rows 7's two commits, and was not repeated. `aws-linux.sh` then fell back to the local VM; I started nothing there.
