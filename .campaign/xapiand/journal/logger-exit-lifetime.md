# logger-exit-lifetime: journal

State: head logger:455cad0 (test-only after fc698e2); waiting on jobs 261118-214915-0e2ec6 261118-214915-84fc5a 261118-214916-ce95b6 261118-214917-50faa0 261118-214917-0177e9 261118-214918-1cd38b 261118-214918-35cf03 261118-214918-7f4e7e 261118-214919-1e22af 261118-214920-4695f1 261118-214920-31f08e 261118-214920-9f753a 261118-214921-e6db5a 261118-214921-e60e8b 261118-214922-5150a7 261118-214922-6e3841 261118-214922-21a59e 261118-214923-74fb0f and CI; then ready for review
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
- logger:107d707 test only: `logger_exit_test capped` (ctest logger_exit_capped), 200 lines at 50 ms
  queued at exit; the stop must cut at 5 s and report the cut count, which with the handler's own
  adds to 200. Must fail.
- logger:cfcec69 lead's ruling: `kExitCap` 5 s total (docker stop's 10 s SIGKILL grace, Kubernetes
  30 s) beside `kExitStall` 200 ms; either way the stop writes "logger: exit stopped the LOG thread
  (5 s cap|stalled 200 ms): N lines not written" to stderr. A pass that sees `abandoned` adds its
  written lines to `completed_` first, so N (pending_count) is exact. Docs name the cap and reason.
  Pushed.
- logger:0f1063d `mach_clock()` and `tick_stamps()` made in `LogWorker::get()`'s creation lambda,
  before the LOG thread starts (pthread_create orders them before its reads). Pushed.
- Review CHANGES (reviews/logger-exit-lifetime-correctness.md), lead's message:
  - logger:ab42a67 test: `late` also logs L_EXC from the late static with a heap-held describe hook
    that exits 95 once destroyed; expects "late exception line: late boom" on stderr.
  - logger:d037d5b F4: the raw stderr write after the teardown appends the line's description.
  - logger:a3a7ef6 F1: `describe_at_call` loads `torn_down` first; set, it uses the std default.
  - logger:ef5f968 test: `fork` (logger_exit_fork) and `logexit` (logger_exit_logexit), each child
    run with a deadline.
  - logger:e74c64e F3: the stop returns at once when getpid() differs from the LOG thread's
    creator's or it runs on the LOG thread (`tid_`). Doc: the cap bounds the drain; a handler
    blocked forever in a write is the case it does not bound.
  - logger:dec68de F2: abandoned, the LOG thread expires the due lines out of the wheel and keeps
    the count still filed (`not_due_`, read after `parked`); report "at most N lines not written".
    F5/F6 documented in docs/API.md. Pushed.
  - Reviewer's probes copied to .scratch/xapiand/logger-exit-lifetime/review-probe (count mode
    parses "at most N"; late-exc now sees "late exc line: late boom", which it reports as ok).
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
  201653-28f0e6. CI run 44457207582 at 70e3c36: green on all 9 platforms (linux-asan-ubsan,
  linux-tsan, freebsd, macos x86_64/arm64, linux gcc/clang/arm64).
- Lead ruling (5 s cap): logger:107d707 + logger:cfcec69. Jobs at cfcec69: suite 202715-a8a813, ASan
  202715-8697af 202716-10cb25, TSan recipe 202716-acc745, A2 202717-fdc85d 202717-6ea3dd, A5
  202718-79d2fa 202718-f4715b, slow+stall 202719-f5790d, capped pass 202727-7e8fc4, capped
  fails-without 202723-694dbd (107d707), slow fails-without 201716-0c19dd (46d3d0b). CI run
  44458400155. Jobs at 70e3c36 are superseded. Main TSan probe 17b386 cancelled (the ASan main
  probe already attributes it).
- Lead msg (79d2fa failed): LLVM 22 TSan logger_exception_test at cfcec69 reported "data race in
  LogWorker::advance": the LOG thread reads `mach_clock()::clock` while main's first
  `ring_published` constructs it (MachClock::MachClock); not `taken_` or `pending_count`. Same class
  as main's 175038-d98aaf (mach_clock, tick_stamps, get()::worker statics; the LLVM 22 libc++abi
  guard is invisible to TSan), so pre-existing and timing-dependent (42e95e was clean). Fix
  logger:0f1063d. Jobs at 0f1063d: 79d2fa rerun 204611-580d8d + repeats 204614-336200,
  204615-b7d8ee, 204617-778bc7; AGENTS recipe A5 204612-0dc795, suite 204610-fedb55; capped
  204612-f4350c, slow+stall 204612-fdaa15; suite 204609-df150a; ASan 204609-7d5079 204610-c89e2b;
  A2 204611-770ed2 204611-969d29; LLVM 22 TSan full suite (info) 204624-bc79bc. Attribution at
  b6371a4 (before the exit changes), 4 runs: 204618-8f59b1 204620-d7ed83 204621-930578
  204622-0b0cd4.
  Passed so far: suite 204609-df150a, 79d2fa rerun 204611-580d8d; CI run 44460551314 green at
  0f1063d. Next: when the rest pass, paste the brief's evidence table, mark ready for review.
- Review round jobs: fails-without F1 211550-396b63 (d037d5b, late), F3 211530-b2be3e (fork),
  211531-a0d2cc (logexit) at ef5f968; pass at dec68de 211551-f2d9be (late fork logexit capped
  slow); review probes 211533-3692bb (plain), 211535-b29cd5 (ASan), 211536-fab87d (TSan); reruns at
  dec68de: suite 211538-f22be9, ASan 211539-e2b84a 211539-de38d7, TSan recipe 211539-19d855, A2
  211540-46d3e1 211540-5fa769, A5 211540-ef50bc 211540-b21587 211541-bb2b1d 211542-bf5dcf
  211542-b01ba9, slow+stall 211541-c3bd82, capped 211541-54fa80. Next: when all pass, CI green,
  evidence table, ready for review.
- Lead msg (probes exit 3, TSan recipe 19d855 exit 8), read:
  - Probes 3692bb/b29cd5/fab87d: a build error, not a probe result: the quest slot's
    build-probe-* cache came from the reviewer's source dir. Rerun --fresh at dec68de
    (213022-e130d8, 213023-5eb902, 213025-2ee506): late-exc, fork, logexit, echo ok; count a REAL
    miss: "at most 22" for 17 due (+5 deferred). Cause: the stop abandoned the LOG thread inside
    close()'s last look, and close() then released the wheel without counting. Fix logger:fc698e2
    (close() checks `abandoned` after the last look); test logger:385584f (capped child also files
    5 deferred lines due in an hour). Probes at fc698e2: 213654-ec0596 (plain), 213656-307888 (ASan),
    213657-495b67 (TSan); the fresh runs at dec68de are its fails-without. Probes PASSED at fc698e2 in all
    three: count "17 reported not written + 3 written = 20"; late-exc's "late line is missing" is its
    literal match on "late exc line", now "late exc line: late boom" (F4), status 0.
  - 19d855: only logger_queue's exit flush, a new mode, not main's e96e6a set: "logger: exit stopped
    the LOG thread (5 s cap): at most 1503 lines not written". Measured: TSan recipe child alone
    213026-b554b4 1.9-4.7 s per run; 213314-4bbdf2 splits it: drain about 1.1 s for 3600 lines
    (0.3 ms a line), so beside the suite it reaches the cap. Fix logger:9c3ef8e: the exit flush child
    queues 900 lines (4 x 200 + 100). No static-init reports in 19d855.
  - Reruns at fc698e2: suite 213659-24fa6a; ASan 213700-cba5c4 213700-bd7a8d; TSan recipe
    213700-dfe75e 213701-0b38c9; A2 213701-5bf09d 213701-b3b490; A5 213702-278777 213702-35a4ac
    213702-10d630 213702-8b6f13 213703-3c2535; slow+stall 213703-ff918e; capped 213703-87e194;
    late/fork/logexit/capped/slow 213704-9a8ef7.
  - Since logger:0f1063d (for the re-check): ab42a67 (test, late L_EXC), d037d5b (F4), a3a7ef6 (F1),
    ef5f968 (test, fork/logexit), e74c64e (F3 + doc), dec68de (F2 report + F5/F6 API doc), 9c3ef8e
    (test, exit flush 900 lines), 385584f (test, capped + deferred), fc698e2 (F2: abandoned in the
    last look).
- Lead msg (CI run 44466356229 red at fc698e2, linux-asan-ubsan logger_exit_capped): LSan's two
  reports are the TEST's own allocation, not the logger's: direct 24 B at exit.cc:183 (`new
  std::vector<Log>` in child_capped) and indirect 384 B (its buffer, exit.cc:185), the deferred
  handles kept so the lines stay filed, rooted only by a local. The logger's abandoned lines are
  not reported (reachable from the rings and the wheel). The other exit tests (exit, once, late,
  slow, fork, logexit) passed under LSan in that run. Fix logger:455cad0 (test only): a global
  `capped_deferred` pointer roots the vector; no suppression. Since 0f1063d, for the re-check, add
  455cad0 after fc698e2. Reruns at 455cad0: 261118-214915-0e2ec6 261118-214915-84fc5a 261118-214916-ce95b6 261118-214917-50faa0 261118-214917-0177e9 261118-214918-1cd38b 261118-214918-35cf03 261118-214918-7f4e7e 261118-214919-1e22af 261118-214920-4695f1 261118-214920-31f08e 261118-214920-9f753a 261118-214921-e6db5a 261118-214921-e60e8b 261118-214922-5150a7 261118-214922-6e3841 261118-214922-21a59e 261118-214923-74fb0f.

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

0. Read the jobs above; if all pass, paste `campaign brief`'s evidence table, then mark ready for
   review. If an ASan suite still fails exit flush, read its log for a timeout.

1. Read the jobs; if 171634-64599a also fails at main, the exit-flush loss is pre-existing (report
   it; the join timeout is the cause to check); else investigate in my change.
2. Paste `campaign brief`'s evidence table; bench numbers to Result.
3. Mark ready for review.

## Blocked on

Nothing. (Resolved by the lead's ruling, msg after the fifth session: keep the progress wait, add a
5 s cap with a stderr report; done in logger:cfcec69.) Earlier text, kept for the record: Lead to confirm (implemented, reversible): the exit-time stop now waits as long as the drain keeps
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

- logger_queue's exit flush resized to 900 lines (test only): the 5 s cap (lead's ruling) cuts a
  3600-line TSan Debug drain beside the suite; the test still covers threads, records and main.
- F1 fails-without filed under A2 and F3's under A4 (A1 is an [artifact] criterion, proven here).
- The stop reports on stderr when it gives up after a stall too, not only at the cap: lines lost
  either way are never silent.
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
