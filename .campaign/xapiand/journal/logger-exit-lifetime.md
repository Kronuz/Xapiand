# logger-exit-lifetime: journal

State: waiting on jobs 261118-180838-6a28bf 261118-180840-23d801 261118-180842-978b44 261118-180844-44f49a 261118-180847-4a6030 261118-180849-59f354 261118-180851-36de6b 261118-180853-c68838 261118-180856-e0632e 261118-180858-55c995
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
- logger:b3cab44 test only (A5): test/exception.cc (ctest logger_exception), threads L_EXC an
  exception and destroy it after the LOG thread wrote the line; races under TSan without the fix.
- logger:004304e A5 fix: a record keeps the description (`std::optional<std::string> exc`), made by
  `describe_exception` on the logging thread (`describe_at_call`; a throwing hook gives
  "unknown exception"); the LOG thread never touches the exception. Pushed.
- logger:cc66e3a test/exception.cc holds a watchdog so each L_EXC is due a tick later: at
  b3cab44 the test passed under TSan with the bug present (175754-d9ffd0, 175800-a631ea exit 0),
  since a line written at once could be released after its thread dropped the handle, ordering
  the two through the record's shared_ptr. Pushed.
- Mutant (local branch mutant/a5-no-describe-at-call, logger:5660da5, never pushed): cc66e3a with
  logger:004304e reverted, for the A5 --fails-without jobs.
Job scripts (ignored scratch): .scratch/xapiand/logger-exit-lifetime/{llvm.sh,ab_build.sh,ab_run.sh}.

## Doing

A5 (lead, third message): Xapiand TSan run 44281033140 (test_node): vptr race, LOG thread's
default_describe_exception vs ~StorageCorruptWAL on the throwing thread. Reading: the record held
an std::exception_ptr (shared ownership, not a raw reference), so the destruction followed the LOG
thread's release; the order lives in libstdc++'s exception refcount, uninstrumented, so TSan saw
no happens-before. Fixed anyway by the lead's design (text captured at the call): logger:004304e.
Jobs at head logger:cc66e3a: A5 fails-without at the mutant 180838-6a28bf (LLVM 22),
180840-23d801 (AGENTS recipe); clean at the fix 180842-978b44, 180844-44f49a; suite 180849-59f354,
ASan 180851-36de6b, TSan (AGENTS recipe) 180853-c68838, producer A/B 180856-e0632e, drain A/B
180858-55c995; exit-flush experiment 180847-4a6030 (175050-f89fd4 died in its build: llvm.sh's
otool line, now optional).

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
  same script at main 175038-d98aaf FAILED the same way (logger_clock, logger_producer,
  logger_queue on the same static-init reports): pre-existing with that toolchain, not this quest.
  A3's TSan evidence is the AGENTS.md recipe instead (175836-b6440b at 004304e; 7ab381 at db35b6c
  was superseded). CI linux-tsan green at both fix commits.
- Bench 171637-3deb48: harness error, not a regression: logger_ab.py refuses an output dir that
  exists (left by 171612-9b1d05 in the shared slot). Fixed in .scratch ab_run.sh (rm -rf first);
  rerun 175040-2293f6 done: producer 6e7ab9e -> db35b6c, 8 rounds, median ratio [95%]: burst 1p
  0.952 [0.713, 1.003], back to back 1p 0.944 [0.828, 1.300], end-to-end 1p 0.958 [0.652, 1.212],
  burst 4p 1.010 [0.806, 1.075], back to back 4p 0.964 [0.731, 1.158], end-to-end 4p 1.002
  [0.930, 1.215]: every interval spans 1.
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
| A5 | pending: fails-without at the mutant logger:5660da5 180838-6a28bf, 180840-23d801; clean at logger:cc66e3a 180842-978b44, 180844-44f49a |
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
