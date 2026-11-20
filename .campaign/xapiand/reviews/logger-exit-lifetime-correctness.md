# logger-exit-lifetime review: correctness

Commit: logger:0f1063d66108d5190e80b014a5d5b861c30d71b1
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A2: 261118-204611-770ed2 and 261118-204611-969d29 pass at the head. The fails-without jobs fail
  for the right reason: 261118-170114-f2778f and 261118-170117-86f821 abort with
  `system_error: mutex lock failed` in the `once` child under LLVM 22 and Apple libc++;
  261118-171555-da551f and 261118-171557-af5502 lose both late lines (the routine and the severe
  one). The slow-drain and cap tests also fail for the right reason: 261118-201716-0c19dd writes 156
  of 400 lines; 261118-202723-694dbd prints no report.
- A3: ASan suites 261118-204609-7d5079 and 261118-204610-c89e2b, and TSan suite
  261118-204610-fedb55, pass at the head.
- A4: suite job 261118-204609-df150a (20 of 20, `logger_transcript` among them), and GitHub CI run
  44460551314 at the head, green on every platform: Linux gcc and clang, arm64 gcc, TSan,
  ASan+UBSan, macOS x86_64 and arm64, FreeBSD.
- A5: 261118-204611-580d8d, 261118-204612-0dc795 and repeats 336200, b7d8ee and 778bc7 pass under
  TSan at the head. Fails-without jobs 261118-181931-f6bb3e and 261118-181933-1d17de run at mutant
  fff8ba9, which reverts only logger:004304e. They report the race the quest names: a vptr write in
  `Boom::~Boom` (from `__cxa_end_catch`) against a read in `Logging::emit` on the LOG thread.
- The transcript golden is unchanged on the branch (`git diff main..HEAD` does not touch
  `test/transcript.golden`). `logger_transcript` passes in df150a, and the transcript covers
  `L_EXC` with and without text, the tint, and truecolor and ansi16 markers with the gradient.

## What I checked myself

All probes are in `.scratch/xapiand/review-logger-exit-lifetime-correctness/`. `probe.cc` has five
modes, each a child process with a deadline. `build.sh plain|asan|tsan` builds them against the
job checkout with LLVM 22 and its own libc++. `run.sh FLAVOR` runs them. `ab_build.sh` and
`ab_run.sh` are the contributor's A/B scripts, copied.

- 261118-205848-17a83b (plain, head), 261118-205850-06cc3a (ASan, head) and
  261118-205851-bbd993 (TSan, head); 261118-205853-b138f4 (plain, main logger:6e7ab9e), for
  attribution.
- 261118-205904-4a81f0 (drain A/B) and 261118-205906-d834d2 (producer A/B), main logger:6e7ab9e
  against the head. The contributor's e0632e and 55c995 ran at logger:cc66e3a, which is before the
  exit wait's per-line `taken_` store.

Answers to the lead's questions:

1. Destroyed statics at exit. Yes, one path remains (F1). After the teardown, a line that carries an
   exception calls `Logging::hooks.describe_exception` from `add_line` (`describe_at_call`,
   logger.cc:675 and logger.cc:1789) before `emit`'s fence. The fork child and an `exit()` on the
   LOG thread do not reach a destroyed static: both hang first (F3). An `exit()` from a non-main
   thread while other threads log is the window the doc's last bullet names.
2. The dropped-line count is not exact (F2). It counts deferred lines that are not due, which no
   exit writes, and it can count a line that a racing producer then writes itself. The exit wait
   does not deadlock when a handler logs from the LOG thread (probe `echo`: 100 lines and 100 echoes,
   a clean exit under plain, ASan and TSan, with no sanitizer report). An `exit()` called on the LOG thread does hang (F3).
3. Describing at the call writes the same text for every exception type, with two exceptions: a
   hook that throws (F5), and a host hook that reads thread-local state. The golden is
   byte-identical.
4. Hot path: no measurable cost. In the drain A/B (4a81f0) the head runs at 1.003x main's time
   per line, median of 15 paired rounds, range 0.913 to 1.067. In the producer A/B (d834d2) every
   case's interval contains 1.0: burst 1.009 [0.848, 1.051], end-to-end 0.987 [0.846, 1.058],
   4 producers end-to-end 0.956 [0.886, 1.048]. Both ran at light level beside other holds. The
   fence is one relaxed load per written line on the LOG thread, and the exit wait adds one
   uncontended relaxed store per line.
5. The tests that must fail without the fix fail for the right reasons (see Evidence).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A line that carries an exception, logged after the teardown, calls the host's destroyed `Logging::hooks.describe_exception`. The fence protects `emit` and `log`, but `describe_at_call` runs earlier, in `add_line` (logger.cc:1789) and the `Logging` constructor (logger.cc:1562). This contradicts docs/exit-lifetime.md ("none of the three is touched") and A1. logger:004304e opened this path: it moved the hook call out of `emit`, which is fenced. This is not a regression
against main: there, the same probe crashes in `emit`, which has no fence (b138f4, SIGSEGV). But it
is a hole in the fence this quest adds and documents as closed. A static destroyed after logger.cc's statics that runs `L_EXC` inside a `catch` crashes the process at exit when the hook's functor is heap-held. | Probe `late-exc`: SIGSEGV (status 11) in plain 17a83b. TSan bbd993 reports a SEGV in `describe_at_call`. ASan 06cc3a reports `heap-use-after-free` in `describe_at_call` <- `Logging::add_line` <- `LateExc::~LateExc` <- `exit`, on memory freed by `__cxa_finalize_ranges`. | `describe_at_call` loads `torn_down` first and, once it is set, uses `default_describe_exception` (which reaches no static), or returns no description. Test: `logger_exit_test late` also logs an `L_EXC` from its late static, with a heap-held describe hook whose destructor sets a flag and whose call exits nonzero once the flag is set; the test checks the line reaches stderr. |
| F2 | no | gap | The exit report "N lines not written" is an upper bound, not a count. `pending_count()` includes deferred lines filed in the wheel that are not yet due. A normal exit releases those unwritten (`close()` resets the wheel), and an abandoned one never completes them. It also counts a line a producer posted that the producer then writes itself through `write_untaken`, if the report reads the count between the two steps. The `capped` test's exact `cut + written == 200` holds only because it has no deferred lines and one producer. | Probe `count`, 20 queued lines and 5 lines due in an hour: "22 lines not written" + 3 written = 25 (17a83b, 06cc3a, bbd993). | Either subtract what the wheel holds (`wheel_->size()` once parked), or word the report "at most N lines". Deferred lines can also be reported apart. The owner's call. |
| F3 | no | gap | Exit hangs forever in two cases. The first is a fork child of a process whose LOG thread runs: it inherits the stop, its `join` can never succeed, and `abandon()` spins on `parked`. The second is `exit()` called on the LOG thread (from a handler or a deferred task): the stop joins and then waits for the thread it runs on. Both hangs exist at main too (b138f4), so the branch did not introduce them. But the branch's doc says the 5 s cap "keeps exit under the shortest grace a supervisor gives before SIGKILL", and it does not. The same holds for a handler blocked on a write: `abandon()` waits for the line in progress without a bound. | Probes `fork` (killed after 8 s) and `logexit` (killed after 20 s): head 17a83b, 06cc3a and bbd993; main b138f4. | Skip the stop when the caller is the LOG thread or the LOG thread is not in this process (compare a pid recorded at start). Word the cap as a bound on the drain, not on exit. A follow-up hunt is fine. |
| F4 | no | nit | After the teardown, `emit` writes `line.str` alone and drops `line.exc`, which the line owns and which is safe to append. A late `L_EXC` loses its description. | logger.cc:1841-1844 | Append `": " + *line.exc` in the raw write. |
| F5 | no | nit | A describe hook that throws used to propagate out of `emit`. It now becomes the text "unknown exception" (logger.cc:682). This is a behavior change for that case only, and arguably better. | logger.cc:675-686 | Keep it, and note it in docs/API.md next to the hook. |
| F6 | no | nit | Describing at the call runs the host hook on the producer for every exception line, including a `clears` watchdog that is later cancelled and never written. This costs nothing for Xapiand's hook (a rethrow and `what()`, src/main.cc:1084), but a costlier hook now runs on the hot thread. | logger.cc:1788-1790 | None needed now; the hook's comment in logger.h already says it runs when the line is logged. |

## Not run

- Linux and FreeBSD for the probes. CI covers the suite there; the probes ran on macOS x86_64 only.
- An `exit()` from a non-main thread while another thread logs. Only reasoned: it is the window the
  doc's last bullet names, and the fence cannot close it without a lock.
- Xapiand's aggregations test 20 of 20 under one libc++ after the repin: that is coverage-areas'
  check, per the quest's decision.

## Re-check

Commit: logger:455cad0
Verdict: APPROVE
Date: 2026-11-18

Scope: the ten commits logger:0f1063d..455cad0, which are ab42a67, d037d5b, a3a7ef6, ef5f968,
e74c64e, dec68de, 9c3ef8e, 385584f, fc698e2 and 455cad0.

### Evidence I relied on

- **Suite, sanitizers and targeted tests at 455cad0.** These passed:
  - The suite: 261118-214915-0e2ec6.
  - ASan, run twice: 84fc5a and ce95b6.
  - TSan with the AGENTS.md recipe, run twice: 50faa0 and 0177e9.
  - The exit tests `late`, `fork`, `logexit`, `capped` and `slow`: 5150a7.
  - The cap test 261118-214921-e60e8b, the slow-drain test e6db5a, and the A2 and A5 jobs
    (1cd38b, 35cf03, 7f4e7e, 1e22af, 4695f1, 31f08e, 9f753a).
- **CI.** Run 44467754116 at 455cad0 is green on every platform, linux-asan-ubsan among them. CI
  run 44466356229 at fc698e2 failed only on LeakSanitizer, which reported the capped child's own
  `new std::vector<Log>` (exit.cc:183) and the lines it holds (exit.cc:185). Nothing came from
  logger.cc. 455cad0 roots that vector in a global.
- **Tests fail without the fixes, for the right reasons.**
  - F1: 261118-211550-396b63, at d037d5b (before a3a7ef6), segfaults in `child-late` after the two
    plain late lines.
  - F3: b2be3e (`fork`) and a0d2cc (`logexit`), at ef5f968 (before e74c64e), both hang.
- **My probes, run by the lead from a copy at fc698e2.** The only change in the copy is the
  report's parse, now "at most". Plain ec0596, ASan 307888 and TSan 495b67 report:
  - `late-exc`: a clean exit, and the line is written as "late exc line: late boom". My check
    looked for the bare text, so it printed "late line is missing". The text is right.
  - `fork` and `logexit`: both exit 0.
  - `count`: "at most 17" plus 3 written equals 20.
  - `echo`: 100 lines and 100 echoes, with no sanitizer report.
  - The same probes at 455cad0 pass too (6e3841, 21a59e, 74fb0f).

### F1 to F6

- **F1 fixed.** `describe_at_call` loads `torn_down` and falls back to
  `default_describe_exception` (logger.cc:682), which reaches no static. The test is
  `logger_exit_test late`'s `L_EXC` with a heap-held hook that flags its own destruction.
- **F4 fixed.** The raw write keeps the description the line owns.
- **F2 fixed.** `close()` counts what stays filed after an abandon (`not_due_`), and the report
  says "at most". Before fc698e2, an abandon that came during the last look still released the
  wheel. The capped test now files 5 deferred lines that are not due, and it still adds up to 200.
- **F3 fixed for the fork child and for `exit()` on the LOG thread.** The doc now states that a
  handler blocked forever in a write is not bounded.
- **F5 and F6** are documented in docs/API.md.

### Question 1: does resizing the exit-flush test (9c3ef8e) hide a real exit-latency problem?

No. It fits the test to TSan's cost under load, and the exit drain did not get slower.

1. **Paired A/B, main against 455cad0.** Job 261118-222240-3b8267 runs the queue test's exit
   child (3600 lines from 4 threads and main, a `StreamLogger` to a file) through
   `.scratch/.../flush/`, 8 interleaved rounds per build.
   - Exit drain, measured from the last line's stamp to process end. Release: 35 ms (main) against
     36 ms (branch). TSan Debug: 225 ms against 240 ms, with overlapping ranges (191 to 242 against
     183 to 263).
   - Every run wrote all 3600 lines.
   - That is about 10 µs a line in Release. The 5 s cap would cut a Release exit only above roughly
     500k queued lines.
2. **What the resize answers.** The contributor saw TSan Debug cut 1503 lines at 5 s beside the
   suite: about 2.4 ms a line, against about 0.06 to 0.3 ms alone. That gap is CPU contention on an
   instrumented build, not the logger.
3. **Main was worse.** Its 200 ms total join dropped lines silently at exit. Job efd725 wrote 1196
   to 2870 of 3600 lines in Release and 606 to 1258 under TSan. The cap with a report is strictly
   better.
4. **Earlier sequential timings are not evidence.** Jobs e5e618, c7fbcb and 3f3a59 swung between
   24 ms and 2.2 s on both arms with the host's load. The paired job above supersedes them.

### Question 2: are the fork and LOG-thread skips (e74c64e) sound?

Yes.

- **The fork child.** `pid_` is recorded when `LogWorker` is built, in the process that starts
  the thread, so a fork child compares unequal and returns before `end()`. The child has no LOG
  thread to join and none that could still touch its statics.
- **`exit()` on the LOG thread.** `tid_` is stored at the top of `operator()` (logger.cc:1402).
  The statics are then destroyed by the LOG thread itself. It is inside the handler that called
  `exit()` and never resumes, so no drain runs concurrently.
- **An `exit()` before the LOG thread stores `tid_`.** `tid_` is still the default id, so the
  stop runs normally. That is correct, since the caller is not the LOG thread.
- **What is lost.** In both cases the queued lines are not written. The doc says so
  (docs/exit-lifetime.md, first paragraph).

### Re-check findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | gap | After an abandon, `close()` expires the due deferred lines with a no-op callback (logger.cc:1224). The wheel unlinks them, and each `Logging` is then held only by its own `wheel_hold`: a self-cycle that LeakSanitizer reports as a leak. A host's Linux ASan run would report one when an exit abandons with due deferred lines whose handles were dropped. No test reaches this: the capped test's lines are not due. | wheel.hh `expire()` unlinks and calls the given callback; logger.cc:1222-1226. I reasoned this out and did not run it, because LSan is CI's (Linux). | Keep the expired entries reachable (for example, push them onto `batch`, which the never-deleted worker holds), or count them without expiring: `wheel_->size()` minus the entries due by `now`. |
| R2 | no | nit | The resize applies to every build, though only TSan needed it. queue.cc already has `kThreadSanitizer` for quick sizes, so Release could keep 3600 lines across threads at no cost (36 ms). | test/queue.cc:706-733 | `kExitPer = kThreadSanitizer ? 200 : 800`, and the same for main's lines. |
| R3 | no | nit | Both skips leave `kStopping` unset. A static's destructor that logs in a fork child, or on the LOG thread after it called `exit()`, queues its line where no thread will write it, instead of reaching stderr through the fence. Setting `kStopping` (not `kClosed`, whose wait would spin on `final_done`) before returning would send those lines inline. | logger.cc:1256-1259 | Optional: `worker_state.fetch_or(kStopping)` in the skip. |
| R4 | no | gap | A fork child that logs after the fork can still block forever in `wake_for` if the LOG thread held `mtx` at the moment of the fork. Its lines are never written in any case. Both behaviors exist at main too; the real mechanism is a `pthread_atfork` reset, which is outside this quest. | logger.cc `wake_for` | A follow-up hunt, if fork-without-exec hosts matter. |

### Not run in the re-check

- An LSan run of R1: macOS ASan here has no leak checking, and Linux sanitizers are CI's.
- The paired exit-drain A/B ran at light level, because strict jobs waited behind exclusive
  holds. Both arms alternate within one job, so they see the same load.
