# track-c-gates: journal

State: waiting on jobs 261120-200605-743b1d 261120-200606-fa2caa 261120-200607-b7499c 261120-200724-273aa8 261120-200804-fd5a96
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 2026-11-20 (second time). The 27 jobs at consensus 8801b99 / journal 178030b ended; both
reports are filled from them (strata:45b0ab43, pushed). Red or short rows:

- C2 T9 `jpower 200` at journal 178030b fails on macOS (18b807) and FreeBSD (d74166): 1,036,264
  checks, 7 failures, seeds 91 and 140 (512 B both), 145 and 199 (2 KiB last), 122 (256 B both).
  Passed at d90797c (e136e7). Linux (2096f8) passes but draws other cases. Detent's msg-183; not
  debugged here. Seed 91 is also in the logs (the lead's note listed four seeds).
- C4 T7c misses 4x again: 3.18 (modeled), 3.37 (fdatasync) at 8801b99 (0bef2c).
- C4 R19 at 0 ms: Control 256 KiB max 59.0 ms against the stated 15.1 ms (dd170c, one run); repeat
  273aa8 queued after the Linux mutants (same VM).
- C4 T8 at 8801b99: clean, 104.9M units, but 49.3 CPU-minutes; fifth run fd5a96 queued.
- T7d passes at 8801b99, 4 KiB arm included (the e1f249d 1.56x miss did not repeat).

## Next

1. When 743b1d (Linux mutants), fa2caa/b7499c (FreeBSD suite, mutants), 273aa8 (R19 repeat) and
   fd5a96 (T8 seed 5) end, fill their cells in docs/gates/track-c-c4.md ("queued").
2. Forward the per-mutant seconds (macOS ce0b7a: 81 to 428 s, 2,616 s total; 743b1d, b7499c) to
   Detent for consensus-mutants-per-test's TIMEOUTs.
3. Then State "ready for review"; T9's red row stays until Detent's fix is rerun.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | docs/gates/track-c-c2.md (artifact), strata:45b0ab43: every row at consensus 8801b99 / journal 178030b filled with its job: macOS 4592f2 86e99e 18b807 83dd88 75f543 b711ce 2b2fc3 369f91 c43dad 4175b8 b0da39; Linux VM 124538 2096f8; FreeBSD VM dd591f d74166. **T9 jpower 200 fails at journal 178030b** (18b807 macOS, d74166 FreeBSD: 7 failures, seeds 91 140 145 199 122), passed at d90797c (e136e7); Detent msg-183 |
| A2 | docs/gates/track-c-c4.md, strata:45b0ab43: every row at e1f249d and 8801b99 with its job: 0bef2c 3b7ebd dd170c 846669 7cef64 c212a8 aa8e3e c7a05f 1303bf 91e7a8 4e57e4 ce0b7a; Linux suite ci: consensus run 44829606479 (8801b99, all platforms green); T7c miss (3.18x), R19 0 ms miss (one run); pending: 743b1d, fa2caa, b7499c, 273aa8, fd5a96. T7e and D18 not runnable (group-heal) |
| A3 | architect's review, launched by the lead |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made
- 11-20: skipped 5ce427, f54066, 1ec7d9 resubmitted without their failed/cancelled dependency at
  8801b99 (743b1d, fa2caa, b7499c); b7499c runs after fa2caa because both use the FreeBSD VM
  (vm.sh refuses a running VM). R19 repeat 273aa8 (`--force`) waits on 743b1d for the Linux VM.
- 11-20: T8 at 8801b99 fell short of the CPU-hour on a loaded host (49.3 CPU-min); a fifth 1,000 s
  run (seed 5, fd5a96) tops it up rather than rerunning all four.
- C4 Linux suite (lead, 11-20): job 261120-152231-ab18c1 (ctest -E consensus_mutants in the VM at
  consensus 8801b99) is cancelled; consensus CI ran the full suite at 8801b99 on Linux x86_64
  (gcc, clang), arm64, macOS and FreeBSD, plus ASan+UBSan and TSan, all green: cite
  `ci:linux-x86_64-gcc` at consensus 8801b99 for that row (D57). The timed mutants (5ce427) stay local.

- consensus main's own pins (journal 4272229, completion 7731315) do not build with journal main (`IO::sync_data_files`); consensus rows use `--ref` pins of journal 4d364f1 and completion f4c19b1 (journal's pin), and a row at consensus's own pins runs beside them. Not changed in consensus (lead's/Detent's).
- T8's CPU-hour: `journal_fuzz` takes both decoders through data[0]; five 1,500 s jobs (125 CPU-minutes) stand for the CPU-hour per decoder, stated as the binary's.
- Mutant jobs 082445-df9788, 082447-920b26, 082955-4b86f4, 082956-6d198b failed in mutants.py's cmake configure on a stale cache (rig); body-v4.sh removes it; reruns are 7b6023, 27e504 at d90797c.
- The first jobs 261118-082210-ac3d79 ... 261118-082213-e9d892 failed from my shell quoting (zsh), not evidence; the Linux journal jobs 261118-082315-f9c2a9 and 261118-115920-3261e1 failed fixture tests because `git archive` applies tar.umask 002 (rig, not the VM's session umask); reruns use `git -c tar.umask=022 archive` (vm-v5.sh).
- The C4 control job for H1 is a plain job that passes when the mutant fails the test (`--fails-without` needs a non-artifact criterion, and A1/A2 are artifacts). T8's CPU-hour is four 1,000 s libFuzzer runs of the hello decoder (case 9), wall-clock stated with the `time -p` CPU figures.
- C4 runs at consensus e1f249d (main today). The C4 VM jobs are chained after C2's VM chain: both use document-gate's VMs and the scripts refuse a VM already running. Job 261120-122758-fb63b4 is a failed misquoted submit (no quest), not evidence.
- Detent ruled on msg-170 (via the lead): until consensus-mutants-per-test lands (after
  consensus-quiescence), C4's suite is `ctest -E consensus_mutants` plus `tools/mutants.sh` outside
  ctest, as separate jobs per platform. consensus is not changed here.
- C2's consensus rows run at consensus main's own pins (journal 9e11713); journal rows at journal
  main 178030b (8 commits past that pin, turn-log recovery changes).
- H1-H4 at 8801b99 used `--force`: submit returned the e1f249d job as identical, and the lead asked for
  every row at main. The flow `build` submit returned a293ec (same command); flow_bench_vm.sh rebuilds
  in the VM when the tree differs, so a1 builds first.
- Cancelled, not evidence: 261120-152020-162fb7, -152022-6c9b4b, -152027-ab2cb4, -152030-4e9c01
  (zsh passed the --env list as one word) and their dependents -152149-d610e4, -152152-6d5efe,
  -152155-aa9f88, -152158-34c2dd; -152025-9d8f43, -152033-26cdba ran suite and mutants in one job.
