# logger-exit-lifetime: journal

State: exit-flush loss found and fixed (logger:70e3c36); waiting on jobs (see Doing)
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
- logger:b6371a4 test/exception.cc: a two-level exception built in the test. At the cc66e3a
  mutant TSan still reported nothing (180838-6a28bf, 180840-23d801 exit 0): TSan counts a vptr
  store as a write only when it changes the vptr, and ~std::runtime_error is in the uninstrumented
  library; Xapiand's ~Error chain is instrumented. Pushed.
- Mutant (local branch mutant/a5-no-describe-at-call, logger:fff8ba9, never pushed): b6371a4 with
  logger:004304e reverted, for the A5 --fails-without jobs.
- logger:46d3d0b test only: `logger_exit_test slow` (ctest logger_exit_slow), 400 lines at 1 ms a
  line queued at exit, never stalled: every line must be written. The stalled `child` now stalls one
  call 1 s (it paced 1200 lines at 20 ms, which a progress-based stop would wait out). Must fail.
- logger:70e3c36 fix: the exit-time stop joins while the LOG thread takes lines (`taken_`, one
  relaxed store per line) and abandons only after 200 ms without one. Docs: ARCHITECTURE.md,
  docs/exit-lifetime.md. Pushed.
- Probe branches (local, never pushed): probe/exit-abandon logger:333e7ff (b6371a4 + a stderr report
  when the exit join times out), probe/exit-abandon-main logger:832db5e (the same on main).
Job scripts (ignored scratch): .scratch/xapiand/logger-exit-lifetime/{llvm.sh,ab_build.sh,ab_run.sh}.

## Doing

Fifth session (fresh context): find and fix the A3 exit-flush loss.
- Cause, measured: probe 200511-3f29a3 (ASan suite at the fix + timeout report) failed exit flush
  and printed "join timed out after 213085 us": the 200 ms exit join timed out and the stop
  abandoned the queued tail. Probe run 200509-ae0766 passed (load-sensitive). The 200 ms budget and
  abandon are main's (logger:4b5b847); the quest's commits only add 3 tests running beside
  logger_queue. Pre-existing: main probe 200513-427cb2 (ASan suite at main + the report, 15 tests)
  FAILED the same CHECK with "join timed out after 227647 us"; the TSan recipe at main 193214-e96e6a
  failed it too. Main's ASan pass a62d6d was a lucky run. TSan main probe 200515-17b386 pending.
- Fix logger:70e3c36 (test logger:46d3d0b). Jobs: fails-without 201716-0c19dd (46d3d0b), pass
  201706-a7fe03 (slow + stalled child); reruns at 70e3c36: suite 201651-0c4cbe, ASan 201651-95cb5d
  and 201707-d5031e, TSan recipe 201652-34809c, A2 201652-2ff32a 201653-1b9363, A5 201653-39af86
  201653-28f0e6. CI on push of 70e3c36.

Lead (fourth message): attribute the ASan exit-flush loss; TSan recipe at main; push; ready when
every criterion has evidence.
- Results at logger:b6371a4: A5 fails-without f6bb3e (LLVM 22), 1d17de (AGENTS recipe) abort with
  "data race on vptr (ctor/dtor vs virtual call)", ~Boom from __cxa_end_catch /
  __cxa_decrement_exception_refcount vs the LOG thread; clean at the fix 42e95e, f02f00; suite
  1fef45 (18 of 18).
- ASan 181949-5797ca and TSan recipe 181954-0ec300 at b6371a4 both fail only logger_queue's exit
  flush (CHECK test/queue.cc:757). The TSan recipe shows none of the static-init reports the LLVM 22
  libc++ build gave. Exit flush has failed in every sanitizer suite at a fix commit (170152-8be416,
  171603-985f5d, 5797ca, 0ec300) and passed once at main alone (171634-64599a).
- Experiment 180847-4a6030 (exit child direct, 20 runs per arm, ASan): alone main 0/20 short, fix
  1/20; beside 3 ASan tests main 0/20, fix 0/20; each child 0.25-0.3 s. Inconclusive (1 of 20).
- Attribution in the suite's own conditions: ASan suite at main 193213-a62d6d, TSan recipe suite
  at main 193214-e96e6a. If main passes both, the loss is introduced: next, find what in
  logger:9a7c6b6 slows or loses the exit drain (instrument the exit child: does the 200 ms join time
  out and abandon). If main fails too, it is pre-existing and load-sensitive.

A5 (lead, third message): Xapiand TSan run 44281033140 (test_node): vptr race, LOG thread's
default_describe_exception vs ~StorageCorruptWAL on the throwing thread. Reading: the record held
an std::exception_ptr (shared ownership, not a raw reference), so the destruction followed the LOG
thread's release; the order lives in libstdc++'s exception refcount, uninstrumented, so TSan saw
no happens-before. Fixed anyway by the lead's design (text captured at the call): logger:004304e.
Jobs at head logger:b6371a4: A5 fails-without at the mutant 181931-f6bb3e (LLVM 22),
181933-1d17de (AGENTS recipe); clean at the fix 181938-42e95e, 181941-f02f00; suite 181946-1fef45,
ASan 181949-5797ca, TSan (AGENTS recipe) 181954-0ec300. Benches at cc66e3a (logger code identical
to b6371a4): producer 180856-e0632e, drain 180858-55c995. Exit-flush experiment 180847-4a6030.

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

Lead to confirm (implemented, reversible): the exit-time stop now waits as long as the drain keeps
writing (abandons after 200 ms without a line) instead of 200 ms in all. Effect a host sees: a
process exiting with many lines queued behind a slow handler takes longer to exit and loses no
lines; a stalled handler is still given up on after 200 ms. Options: (a) this (recommended: it is
what the timeout was documented for, "a handler is stalled"); (b) keep 200 ms in all and shrink
logger_queue's exit flush to fit sanitizers under load (lines still lost at exit under load).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | docs/exit-lifetime.md in the logger repo (logger:9a7c6b6, teardown fence logger:db35b6c) |
| A2 | 261118-170114-f2778f (--fails-without at logger:d031751, LLVM 22 libc++): child aborts `system_error: mutex lock failed: Invalid argument`, status 6; 261118-170117-86f821 (same, Apple clang): the same abort; 261118-170120-b58cbb: passes at logger:9a7c6b6 (LLVM 22 libc++) |
| A3 | open: ASan 181949-5797ca and TSan 181954-0ec300 at b6371a4 fail logger_queue exit flush (attribution 193213-a62d6d, 193214-e96e6a at main); CI linux-asan-ubsan and linux-tsan green at 9a7c6b6 (run 44433808350) |
| A5 | 261118-181931-f6bb3e, 261118-181933-1d17de (--fails-without at the mutant logger:fff8ba9: TSan "data race on vptr", exit 134); 261118-181938-42e95e, 261118-181941-f02f00 (clean at logger:b6371a4) |
| A4 | suite 261118-181946-1fef45 at logger:b6371a4 (18 of 18); CI green on all platforms at logger:9a7c6b6 (run 44433808350) and logger:db35b6c (run 44435549806) (main's last red, run 43887623328, was a macos-arm64 runner never acquired) |

## Result

Benchmarks, strict beside light holds (host load high, wide intervals), median ratio [95%]:
- Producer 6e7ab9e -> logger:cc66e3a (logger code identical to b6371a4), 180856-e0632e, 8 rounds:
  burst 1p 1.038 [0.968, 1.095], back to back 1p 0.998 [0.989, 1.151], end-to-end 1p 0.952
  [0.898, 1.033], burst 4p 0.957 [0.877, 1.105], back to back 4p 1.031 [0.925, 1.316], end-to-end
  4p 0.972 [0.919, 0.988]. No slowdown; the teardown load is on the LOG thread, not the producer.
- Producer 9a7c6b6 -> db35b6c (the teardown load alone), 171612-9b1d05: every interval spans 1.
- Drain (emit per line) 6e7ab9e -> cc66e3a, 180858-55c995, 15 paired rounds: 910.8 -> 877.2 ns per
  line, median ratio 0.985.

## Rulings I made

- Exit-flush loss (A3): proved by probe, not bisected: the probe names the mechanism directly
  (timeout report), and the budget is main's, not the quest's.

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
