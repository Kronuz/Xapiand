# logger-exit-lifetime: journal

State: waiting on jobs 261118-175050-f89fd4 261118-175038-d98aaf 261118-175039-7ab381 261118-175040-2293f6
Quest: .campaign/xapiand/quests/logger-exit-lifetime.md (the lead's; never edit it)

Code: logger repo, worktree /Users/kronuz/code/Kronuz/logger-wt/xapiand/logger-exit-lifetime,
branch quest/xapiand/logger-exit-lifetime (pushed for CI).
- logger:d031751 test/exit.cc: `logger_exit_test once` (ctest logger_exit_once), test only: must fail.
- logger:9a7c6b6 the fix + docs/exit-lifetime.md + ARCHITECTURE.md paragraph.
- logger:61edbc6 test only: `logger_exit_test late` (ctest logger_exit_late), a static destroyed
  after logger.cc's statics logs; its lines must reach stderr. Must fail.
- logger:db35b6c the teardown fence (msg from the lead, below): `Teardown` defined right after
  config/hooks/handlers sets `torn_down`; emit and log load it first and write raw to stderr.
  Pushed; CI run 44435549806 green on all 9 platforms, logger_exit_late passed on each.
Job scripts (ignored scratch): .scratch/xapiand/logger-exit-lifetime/{llvm.sh,ab_build.sh,ab_run.sh}.

## Doing

Lead (2026-11-18, second message): read the three failures, attribute, fix what's mine, rerun.
Done (passed): late fails-without 171555-da551f, 171557-af5502; late+once pass 171600-54abd2;
suite 171608-1a6cec at db35b6c; ASan logger_queue alone at main 171634-64599a (12.3 s).
Failures read:
- ASan 171603-985f5d (db35b6c) and 170152-8be416 (9a7c6b6): logger_queue "exit flush" only,
  CHECK n == 4*800+400 (test/queue.cc:757), lines lost at exit. In the suite ctest runs 4 tests at
  once (logger_queue took 29-35 s vs 12 s alone). Hypothesis: the 200 ms exit join times out under
  contention, at main as at the fix. Experiment 175050-f89fd4 (.scratch exitflush.sh): the exit
  child 20x per arm, main vs fix, alone and beside 3 ASan logger_timestamp_test loops, interleaved.
- TSan 171605-c73004: data races reported on function-local static init (`mach_clock()::clock`,
  `tick_stamps()::tick`), code this quest did not touch; logger_clock, logger_producer, logger_queue
  fail on the warnings. Likely the LLVM 22 libc++abi build (guard not seen by TSan). Attribution:
  same script at main 175038-d98aaf; the AGENTS.md recipe at the fix 175039-7ab381. CI linux-tsan
  green at both fix commits.
- Bench 171637-3deb48: harness error, not a regression: logger_ab.py refuses an output dir that
  exists (left by 171612-9b1d05 in the shared slot). Fixed in .scratch ab_run.sh (rm -rf first);
  rerun 175040-2293f6.
Bench results so far (strict beside light holds, host load high: wide intervals):
- 171612-9b1d05 producer, 9a7c6b6 -> db35b6c (the teardown load alone), 8 rounds, median ratio
  [95% interval]: burst 1p 1.001 [0.831, 1.317], back to back 1p 1.032 [0.834, 1.235], end-to-end 1p
  1.067 [0.654, 1.761], burst 4p 1.007 [0.896, 1.078], back to back 4p 1.014 [0.874, 1.095],
  end-to-end 4p 0.967 [0.886, 1.047]. Every interval spans 1.
- 171614-f0f5a1 drain (emit per line), 6e7ab9e -> db35b6c, 15 paired rounds: 1491.5 -> 1412.2 ns
  per line, median ratio 0.962 (range 0.823..1.101).

## Next

1. Read the jobs; if 171634-64599a also fails at main, the exit-flush loss is pre-existing (report
   it; the join timeout is the cause to check); else investigate in my change.
2. Paste `campaign brief`'s evidence table; bench numbers to Result.
3. Mark ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | docs/exit-lifetime.md in the logger repo (logger:9a7c6b6, teardown fence logger:db35b6c) |
| A2 | 261118-170114-f2778f (--fails-without at logger:d031751, LLVM 22 libc++): child aborts `system_error: mutex lock failed: Invalid argument`, status 6; 261118-170117-86f821 (same, Apple clang): the same abort; 261118-170120-b58cbb: passes at logger:9a7c6b6 (LLVM 22 libc++) |
| A3 | pending at db35b6c: 171603-985f5d (ASan), 171605-c73004 (TSan); at 9a7c6b6 ASan 170152-8be416 failed logger_queue exit flush (attribution 171634-64599a); CI linux-asan-ubsan and linux-tsan green at 9a7c6b6 (run 44433808350) |
| A4 | pending: suite 171608-1a6cec at db35b6c; CI green on all platforms at logger:9a7c6b6 (run 44433808350) and logger:db35b6c (run 44435549806) (main's last red, run 43887623328, was a macos-arm64 runner never acquired) |

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
- Host-owned `Logging::config/hooks/handlers` keep their destructors (lead's decision). The fence
  is `Teardown`, defined right after them in logger.cc (destroyed just before them, by the in-file
  rule), setting a relaxed `torn_down`; `Logging::emit` and `Logging::log` (print_msg's path,
  which also iterates handlers) check it first. After it a line is its text alone to stderr. The
  macros still read config.log_level/max_pending there (ints in never-freed static storage).
- `logger_exit_test late` relies on the test file's statics being made before logger.cc's (link
  order); the child exits 97 if that does not hold, so it never passes vacuously.
- The test fills freed memory in the children (MallocScribble, MALLOC_PERTURB_) so a destroyed
  static reads as garbage on any libc++/libstdc++; the LLVM 22 libc++ build reproduces the
  reported `mutex lock failed`.
