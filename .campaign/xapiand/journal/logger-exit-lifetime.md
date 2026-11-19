# logger-exit-lifetime: journal

State: waiting on jobs 261118-170152-8be416 261118-170154-c3fcd5 261118-170156-26ae58 261118-170205-2112bc 261118-170207-3c6344
Quest: .campaign/xapiand/quests/logger-exit-lifetime.md (the lead's; never edit it)

Code: logger repo, worktree /Users/kronuz/code/Kronuz/logger-wt/xapiand/logger-exit-lifetime,
branch quest/xapiand/logger-exit-lifetime (pushed for CI).
- logger:d031751 test/exit.cc: `logger_exit_test once` (ctest logger_exit_once), test only: must fail.
- logger:9a7c6b6 the fix + docs/exit-lifetime.md + ARCHITECTURE.md paragraph.
Job scripts (ignored scratch): .scratch/xapiand/logger-exit-lifetime/{llvm.sh,ab_build.sh,ab_run.sh}.

## Doing

Waiting on: A3 ASan (170152-8be416) and TSan (170154-c3fcd5) suites under LLVM 22, A4 suite
(170156-26ae58), bench A/B strict: producer (170205-2112bc), LOG-thread drain (170207-3c6344).
Done: A2 (below); CI run 44433808350 on logger:9a7c6b6 green on all 9 platforms, and
logger_exit_once passed in each (linux asan-ubsan, tsan, gcc x86_64/arm64, clang, freebsd, macos x86_64/arm64).

## Next

1. Read the jobs; paste `campaign brief`'s evidence table; add the bench numbers to Result.
2. Mark ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | docs/exit-lifetime.md in the logger repo (logger:9a7c6b6) |
| A2 | 261118-170114-f2778f (--fails-without at logger:d031751, LLVM 22 libc++): child aborts `system_error: mutex lock failed: Invalid argument`, status 6; 261118-170117-86f821 (same, Apple clang): the same abort; 261118-170120-b58cbb: passes at logger:9a7c6b6 (LLVM 22 libc++) |
| A3 | pending: 261118-170152-8be416 (ASan), 261118-170154-c3fcd5 (TSan); CI linux-asan-ubsan and linux-tsan green (run 44433808350) |
| A4 | pending: suite 261118-170156-26ae58; CI run 44433808350 green on all platforms at logger:9a7c6b6 (main's last red, run 43887623328, was a macos-arm64 runner never acquired) |

## Result

<pending>

## Rulings I made

- Design rule (docs/exit-lifetime.md): the exit-time stop (`Finisher` in `LogWorker::get()`) is
  made at the first queued line; what the logger makes after it is destroyed first. So everything
  the logger owns on the emit/drain/inline path is constexpr, trivially destructible, or held in
  `Immortal<T>` (a union whose destructor does not destroy its member). No reordering.
- `Immortal<T>` (constant-initializable, so `shared_ring_mutex` stays constant-initialized) over
  `*new T`: one mechanism for function-local and namespace-scope objects, and a namespace-scope
  `*new` reference would be null before logger.cc's dynamic init.
- Beyond the once filter, also fixed by the same rule: `shared_ring_mutex` (Immortal), palette,
  tints and greys (constexpr term-color escapes as string_view; same bytes, the transcript golden
  checks), the gradient's function-local empty string (gone), `local_tm`'s thread_local cache
  (trivially destructible, TZ in a 64-byte array; longer TZ values are not cached).
- Host-owned `Logging::config/hooks/handlers` stay ordinary statics: made before the stop,
  destroyed after it, so out of the LOG thread's reach. Making them immortal (references to
  Immortal storage) would also cover inline lines logged after their destruction but stops the
  handlers' destructors running at exit (user-visible; test/exit.cc encodes it): left open in the
  doc, not done.
- The test fills freed memory in the children (MallocScribble, MALLOC_PERTURB_) so a destroyed
  static reads as garbage on any libc++/libstdc++; the LLVM 22 libc++ build reproduces the
  reported `mutex lock failed`.
