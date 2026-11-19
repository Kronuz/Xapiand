# logger-exit-lifetime: journal

State: waiting on jobs 261118-171555-da551f 261118-171557-af5502 261118-171600-54abd2 261118-171603-985f5d 261118-171605-c73004 261118-171608-1a6cec 261118-171612-9b1d05 261118-171614-f0f5a1 261118-171634-64599a 261118-171637-3deb48
Quest: .campaign/xapiand/quests/logger-exit-lifetime.md (the lead's; never edit it)

Code: logger repo, worktree /Users/kronuz/code/Kronuz/logger-wt/xapiand/logger-exit-lifetime,
branch quest/xapiand/logger-exit-lifetime (pushed for CI).
- logger:d031751 test/exit.cc: `logger_exit_test once` (ctest logger_exit_once), test only: must fail.
- logger:9a7c6b6 the fix + docs/exit-lifetime.md + ARCHITECTURE.md paragraph.
- logger:61edbc6 test only: `logger_exit_test late` (ctest logger_exit_late), a static destroyed
  after logger.cc's statics logs; its lines must reach stderr. Must fail.
- logger:db35b6c the teardown fence (msg from the lead, below): `Teardown` defined right after
  config/hooks/handlers sets `torn_down`; emit and log load it first and write raw to stderr.
  Pushed; CI pending.
Job scripts (ignored scratch): .scratch/xapiand/logger-exit-lifetime/{llvm.sh,ab_build.sh,ab_run.sh}.

## Doing

Lead's message (2026-11-18): keep config/hooks/handlers destructors; fence them with an
after-teardown flag checked first in emit, raw stderr after it; a test that fails without it;
producer benchmark before/after; close the doc's open point; push. Done in logger:61edbc6 and
logger:db35b6c (pushed). Jobs at db35b6c:
- A2 late, --fails-without at 61edbc6: 171555-da551f (LLVM 22), 171557-af5502 (Apple clang);
  pass at the fix (late + once): 171600-54abd2.
- A3 ASan 171603-985f5d, TSan 171605-c73004 (LLVM 22).
- A4 suite 171608-1a6cec.
- Bench (strict): producer 6e7ab9e vs db35b6c 171637-3deb48; producer 9a7c6b6 vs db35b6c (the
  load alone) 171612-9b1d05; drain 6e7ab9e vs db35b6c 171614-f0f5a1.
- ASan at 9a7c6b6 (170152-8be416) failed in logger_queue "exit flush" (CHECK n == 4*800+400 at
  test/queue.cc:757: lines lost at exit, likely the 200 ms exit join timing out under ASan with host
  load 122). Attribution job at main 6e7ab9e: 171634-64599a (ASan logger_queue).

## Next

1. Read the jobs; if 171634-64599a also fails at main, the exit-flush loss is pre-existing (report
   it; the join timeout is the cause to check); else investigate in my change.
2. Check CI for logger:db35b6c; paste `campaign brief`'s evidence table; bench numbers to Result.
3. Mark ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | docs/exit-lifetime.md in the logger repo (logger:9a7c6b6, teardown fence logger:db35b6c) |
| A2 | 261118-170114-f2778f (--fails-without at logger:d031751, LLVM 22 libc++): child aborts `system_error: mutex lock failed: Invalid argument`, status 6; 261118-170117-86f821 (same, Apple clang): the same abort; 261118-170120-b58cbb: passes at logger:9a7c6b6 (LLVM 22 libc++) |
| A3 | pending at db35b6c: 171603-985f5d (ASan), 171605-c73004 (TSan); at 9a7c6b6 ASan 170152-8be416 failed logger_queue exit flush (attribution 171634-64599a); CI linux-asan-ubsan and linux-tsan green at 9a7c6b6 (run 44433808350) |
| A4 | pending: suite 171608-1a6cec at db35b6c; CI run 44433808350 green on all platforms at logger:9a7c6b6 (main's last red, run 43887623328, was a macos-arm64 runner never acquired) |

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
