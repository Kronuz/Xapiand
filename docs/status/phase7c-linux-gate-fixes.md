> Status: CURRENT (2026-11-03) the developer's ledger for the Phase 7c Linux gate dry-run fixes and the two CI breaks of `ci/master-candidate`; local branch `phase7c/linux-gate-fixes`, not pushed.

# phase7c linux-gate-fixes (build): repair what the Linux dry run found

Branch `phase7c/linux-gate-fixes` in `Xapiand-wt/linux-gate-fixes`, rebased on master `Xapiand:88892b4cb`. Input: strata `docs/gates/phase7c-linux-dryrun.md` (strata:53be755). Linux runs: the shared AWS VM (`aws-linux.sh`, c7a.4xlarge, Ubuntu 24.04, kernel 7.0), g++-13.3, clang 18.1.3, clang 20.1.2. The first batch ran on the local QEMU VM because the lease failed with a service-control-policy deny on `RunInstances`; no result below comes from it.

## State

All six items are done and proved, except two things that are not mine to close: the `std::format` guard in the Kronuz libraries (the maintenance agent has it) and a gcc-13-only hang in two pool tests that exists on master (Blocked on). Sensitive area touched: `msgpack_load` takes untrusted bodies (the check only refuses; no reviewer escalation proposed, the lead decides).

## Findings of the dry run, and what became of each

| # | Finding | Result |
|---|---|---|
| 1 | `GET /` and `GET /:metrics` answer 400 | already fixed on master by `Xapiand:bfd3431ff`; confirmed live on the Release build (GET /, /:metrics and /:info answer 200) and `test_root_routes`; nothing changed |
| 2 | TSan: `pthread_cond_destroy` against `pthread_cond_signal` in `engine::BlockingScheduler` | fixed (`host.h`: `post`, the completion callback and `arm` signal under the lock) |
| 3 | `test_text` out-of-memory under ASan and TSan | the code, not the test: `msgpack_load` let a 6-byte body reserve 128 GiB; fixed there |
| 4 | clang 18 and clang 20 do not build the tree | clang 18: CMake picked libc++ 18, which lacks `__cpp_lib_format`, so the libraries drop `std::formatter<stringified>`; clang 20: RapidJSON v1.1.0; both fixed below |
| 5 | stale harness | fixed (below) |
| 6 | `--term-at` and a `strata_check` target missing | added |
| CI-1 | GCC 13 ICE at `schemas_lru.cc:176` | fixed |
| CI-2 | FreeBSD `format_arg_store.h:167` | not RapidJSON (its lines in the log are `std::iterator` deprecation warnings); same `stringified` guard as clang 18 + libc++ 18; library fix |
| new | `fuzz_body.cc:45` does not preprocess on GCC | fixed |

## Proof

Each fix: the test fails with the fix reverted, passes with it.

| Fix | Test | Before (fix reverted) | After |
|---|---|---|---|
| BlockingScheduler signal | `test_blocking_scheduler` (500 rounds: a thread's `sync_wait` returns and the thread exits while another thread sets the event) | TSan, g++-13: 3 of 3 runs rc 66, `data race ... pthread_cond_destroy ... host.h:214`; also `test_blocking_guard` (when it still held the test) and `test_node` | 3 of 3 pass; TSan ctest 33 of 33 (minus the two tests under Blocked on) incl. `test_node`, `test_engine_host`, `test_text` |
| `msgpack_load` bound | `test_text` `test_body_depth_per_format`: the two oversize headers are `ClientError` ("more items" / "more entries"), a body that fits loads | ASan+UBSan, clang 20: `AddressSanitizer: out of memory: allocator is trying to allocate 0x2000000008 bytes`; macOS Release: uncaught `msgpack::insufficient_bytes` | pass (ASan+UBSan ctest 34 of 34) |
| mutant: array limit `size / 2` instead of `size` | same test | macOS: aborts, `MessagePack array has more items than the body has bytes` on the body that fits | reverted |
| ICE | compiling `schemas_lru.cc` with g++-13.3 | `internal compiler error: in build_special_member_call, at cp/call.cc:11096` | compiles |
| `fuzz_body.cc` | g++-13 build of `xapiand_fuzz_body` | `missing binary operator before token "("` | builds |
| clang 18 | configure and build with `libc++-18-dev` installed | `fs.cc`: `the supplied type is not formattable` (the only failing TU in the tree) | CMake logs `libc++ does not advertise std::format (needs libc++ 19): using libstdc++`; Release ctest 34 of 34 |
| clang 20 + libc++ 20, ASan+UBSan | whole ctest, RapidJSON at 24b5e7a, no local edits | v1.1.0 does not build (dry run) | 34 of 34, no UBSan abort |
| Release ctest, macOS | all 35 tests | n/a | 35 of 35 (build-rel, Apple clang, RapidJSON 24b5e7a); docs E2E with newman 6.2.3 GREEN |

Release ctest, g++-13 (the CI compiler): 32 of 34; `test_pool` and `test_blocking_guard` hang (Blocked on).

Harness rows run on Linux (g++-13 Release, python3 only at `/usr/bin`, fd limit 1024 at the start): docs E2E plus HTTPS (`run_all.sh e2e tls`, newman 6.2.3) GREEN; `engine_mixed_load.py --secs 60 --term-at 30`: SIGTERM at 30 s, exit 0 after 40.3 s, restart, `model_equals_dump` true over 93,880 docs, 363 ids cut off by the signal, 0 failures, 0 sanitizer reports; `lua_bounds.py` PASS (including the foreign-script phase with `--processors 1`); `cli_roundtrip_check.sh`, `stored_stream.sh`, `restore_stream_check.sh` ok; `engine_crash_check.sh --reactors 2` PASS with the `strata_check` target (`ninja strata_check`); `profile_light.sh` and `profile_bulk.sh` run with perf (cpu-clock, DWARF).

## Decisions to review

1. **Linux builds against libstdc++ unless libc++ defines `__cpp_lib_format` (CMake).** Recommendation: keep it. gcc-13 (CI) is libstdc++, so the gate and CI then use one standard library, and an installed `libc++-18-dev` no longer changes the build. Once the maintenance agent widens the guards in stringified, strings and located-exception, libc++ 17/18 builds too; the check could then go, but `strings.hh` still turns `strings::format` into a passthrough on a libc++ without the macro, so re-verify on libc++ 18 before removing it. It does not touch macOS or FreeBSD.
2. **RapidJSON pinned at `24b5e7a` (Tencent master, 2024-12-22), the commit Kronuz/msgpack already uses, instead of tag v1.1.0.** Recommendation: keep. v1.1.0 (2016) does not compile with clang 20 and aborts under UBSan; upstream never released the fixes, so a commit pin is the only way to take them, and one pin for msgpack and Xapiand removes the first-declaration-wins split. The behaviours Xapiand relies on are set by `RAPIDJSON_PARSE_DEFAULT_FLAGS`; E2E is green on Linux with it. Not a vendored patch.

## Next

1. The lead merges, pushes, and reruns CI on the candidate branch.
2. The maintenance agent's pins (stringified, strings, located-exception, fs) land on this branch: after them, build once with clang 18 + `libc++-18-dev` and FreeBSD.
3. The `msgpack` library's `unpacker` (streaming, used by `RESTORE`) still takes `0xffffffff` array and map limits and reserves up front; the body-size bound only covers `msgpack_load`. A bound there needs the library (it knows the buffer), so it is named, not changed.

## Blocked on

1. **gcc-13 only: `test_pool` (`test_released_views_are_reclaimed_by_the_housekeeping`, its own watchdog says "hung") and `test_blocking_guard` hang.** Master with only the ICE fix reproduces both (Release, g++-13.3); clang 18 and clang 20 pass both. At the hang, the main thread sits in a nested `sync_wait` inside `DatabasePool::housekeeping_pass` (`pool.cc:764`, `run_detached` of the per-entry lambda) with every pool worker idle; the CI step "Unit tests (ctest)" will fail on it. Not mine to decide whether it is a compiler bug in a coroutine lambda or a real deadlock; needs the author of the pool.
2. TSan on gcc reports `test_pool.cc:183` (a shared `exception_ptr` freed by the last of two threads; libstdc++'s refcount is not instrumented) in `test_a_failed_commit_reaches_only_the_joined_waiters`; likely a false positive needing a suppression, not examined further.
3. TSan on kernel 7.0 needs `sysctl vm.mmap_rnd_bits=28` (the gcc-13 runtime aborts at start otherwise); perf needs `kernel.perf_event_paranoid=-1` or root.
