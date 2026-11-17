# test_pool: one null-pc crash under ASan+UBSan on Linux CI

Two CI runs of "Unit tests under ASan + UBSan" crashed `test_pool` with SEGV at pc 0 and passed on rerun:
43066693214 (after `test_the_pool_waits_for_a_pin_released_during_its_end`) and 43977806319 (after
`test_the_id_counter_persists_replays_and_advances` printed "issued 14 for f"; the next case is
`test_the_turns_end_publishes_a_finished_merge`). Both sit where a test destroys a pool whose executor, timer
and merge threads may still run.

## Reproduction attempt (macOS x86_64, clang/libc++, master bf215d67a)

Build: Debug -O1 with `-fsanitize=address,undefined -fno-sanitize-recover=all -D ASSERTS=ON -DLTO=OFF`.
Loop: `.scratch/test-pool-flake/loop.sh`, 4 light jobs at once (261116-195117-303b7c, -195119-764b19,
-195122-d8f275, -195125-7de5fc), `halt_on_error=1:abort_on_error=1`, 20 minutes each.

Result: 120 runs, 0 failures. No shuffle exists (the runner takes only a name filter), so order was fixed.

## What differs from CI

1. Linux gcc-13 and libstdc++ (`_GLIBCXX_ASSERTIONS`) against clang and libc++: another thread-start
   and destructor ordering.
2. A 4-core runner at full load against 4 parallel loops on 16 CPUs; the crash needs a narrow window.
3. Linux-only code paths in the pool's executor and WAL writer threads.
4. macOS UBSan aborts on a lz4 zero-offset-to-null (`lz4.c:1069`) that gcc does not flag; the local build
   compiles lz4 with `-fno-sanitize=pointer-overflow,nonnull-attribute` to get past it (a gcc/clang
   difference, not the flake).

## Next

A Linux repro needs a Linux host (a qemu VM or the CI job in a loop with the ASan stack kept:
`ASAN_OPTIONS=abort_on_error=1` plus the `LastTest.log` upload already present). A pc-0 crash after a pool
teardown points at a callback run through a dangling function pointer; candidates are
`DatabasePool::Timer`'s armed callbacks (src/database/pool.cc:414-490) and the host's `timer` capture of
`timer_.get()` at pool.cc:554, which holds a raw pointer. Not proven.
