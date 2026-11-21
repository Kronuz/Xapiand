# track-c-gates: journal

State: ready for review
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 2026-11-21. The five jobs ended exit 0 and are in docs/gates/track-c-c4.md (strata:605f1dd5,
pushed): Linux mutants 16/16 (1,900 s, 100-175 s each), FreeBSD ctest 91/91, FreeBSD mutants 16/16
(1,764 s, 94-140 s), T8 seed 5 (44.6M units, 991.5 CPU-s; 65.8 CPU-min at 8801b99), R19 repeat.
Per-mutant seconds and a window/data_queue sizing note went to Detent (msg-190).

R19 at 8801b99, the lead's two questions:
1. Gap floor: flow_bench gap sends a heartbeat every 10 ms (`next += 10'000`, 500 us poll,
   flow_bench.cc:788-795), so a gap is >= 10 ms plus poll, VM oversleep and delivery spread; idle
   gaps 12.0-12.7 ms (repeat). 0 ms loaded gaps 13.1/12.5/15.3 against 12.8/11.5/13.5 bare, i.e.
   1.0/0.4/3.2 ms over the 12.1 ms idle floor: under bound + floor. Delay max (direct wait): R19's
   case 256 KiB Control 7.8 against 11.5 ms (first run 59.0: 1 of 2 past); 2 MiB Control past at
   0 ms in both runs (24.4/12.8, 29.9/14.6). Worst 59.0 ms delay, 53.7 ms gap, against 1,000 ms.
2. Drops: enqueue's data_queue overflow (host_transport.h:694-695), not send's maximum_frame
   (4 MiB default, connection.h:31; a 2 MiB frame is 2,097,177 B) nor an unstored route. With the
   25 B header the 8 MiB queue holds 3 frames, the 4 MiB window 1; top_up over-posts (24 MiB target,
   counted at post), so a lagging group gets a 5th. Expected overflow (Raft resends); the row
   measures the control wait with queues at cap. Bench sizing, not a consensus bug.

Architect's reviews (strata:ea2652b1): C2 and C4 APPROVE WITH CONDITIONS. C4 conditions 1-2 (F1, F2,
F4) folded in at strata:560e8851: Worker's Flow{} stated in "What is built" with D161; 2 MiB Control
is the deployed case; R19 status states dd170c 0 ms gap 34.4/27.2 and e1f249d 10 ms 52.3/46.3 past
bound + floor; T7a/T7c 20% clause computed from the jobs' follower rates (878.6, 639.5 MB/s): miss,
6.2-8.4%, leader-bound. Residuals listed: C4 3-5 (5 = D161), C2 1-2.

## Next

1. Lead checks the fold-in and lands; C4 residuals 3-5 and C2 residuals 1-2 are owed before the
   milestones pass, not before landing.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | docs/gates/track-c-c2.md (artifact), strata:45b0ab43: every row at consensus 8801b99 / journal 178030b filled with its job: macOS 4592f2 86e99e 18b807 83dd88 75f543 b711ce 2b2fc3 369f91 c43dad 4175b8 b0da39; Linux VM 124538 2096f8; FreeBSD VM dd591f d74166. **T9 jpower 200 fails at journal 178030b** (18b807 macOS, d74166 FreeBSD: 7 failures, seeds 91 140 145 199 122), passed at d90797c (e136e7); Detent msg-183 |
| A2 | docs/gates/track-c-c4.md, strata:560e8851: every row at e1f249d and 8801b99 with its job: 0bef2c 3b7ebd dd170c 273aa8 846669 7cef64 c212a8 aa8e3e c7a05f 1303bf 91e7a8 fd5a96 4e57e4 ce0b7a 743b1d fa2caa b7499c; Linux suite ci: consensus run 44829606479 (8801b99, all platforms green); T7c miss (3.18x); R19 256 KiB Control past at 0 ms in 1 of 2 runs (59.0/15.1, then 7.8/11.5), gaps under bound + idle floor; T7e and D18 not runnable (group-heal) |
| A3 | docs/reviews/track-c-c2.md and track-c-c4.md (strata:ea2652b1): both APPROVE WITH CONDITIONS; C4 conditions 1-2 folded in at strata:560e8851; open conditions listed as each report's residuals |

## Result

strata docs/gates/track-c-c2.md and docs/gates/track-c-c4.md (strata:560e8851): every C2 and C4
row at consensus 8801b99 / journal 178030b (and C4 at e1f249d) with its jobs. Open: C2 T9 jpower 200
fails at journal 178030b (Detent msg-183); C4 T7c misses 4x (3.18x modeled, 3.37x fdatasync); R19's
0 ms row past in 1 of 2 runs; T7e and D18 wait for group-heal; the architect's review (A3).

## Rulings I made
- 11-21: R19's gaps are judged against the stated bound plus the same setting's idle gap (the
  probe's 10 ms cadence floor); delay max stays the direct measure. The "fatal: not a git
  repository" line in the VM mutants logs is c4-body.sh asking git inside the archived tree (rig);
  vm.sh prints the commit before it.
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
