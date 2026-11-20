# track-c-gates: journal

State: waiting on jobs 261120-151850-4592f2 261120-151852-86e99e 261120-151854-18b807 261120-151857-83dd88 261120-151859-75f543 261120-151902-b711ce 261120-151904-2b2fc3 261120-151907-369f91 261120-151910-c43dad 261120-151912-4175b8 261120-151915-b0da39 261120-151928-c212a8 261120-151930-aa8e3e 261120-151933-c7a05f 261120-151935-1303bf 261120-151937-91e7a8 261120-151939-4e57e4 261120-151941-ce0b7a 261120-151943-7cef64 261120-151953-846669 261120-152013-0bef2c 261120-152015-3b7ebd 261120-152017-dd170c 261120-152227-124538 261120-152229-2096f8 261120-152231-ab18c1 261120-152233-5ce427 261120-152236-dd591f 261120-152238-d74166 261120-152240-f54066 261120-152243-1ec7d9
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Resumed 2026-11-20 (lead's note ~/.scratch/xapiand/lead/tcg-resume.txt). Of the 19 jobs at consensus
e1f249d / 5923b6c, 15 passed; their results are in both reports (strata worktree). The others:

- C2 Driver budget past 399 (f50b1e): seed 727, the same harness bug as 399 (Detent msg-178, fixed by
  consensus:85ed0e6; Detent's jobs 261120-124431-f7bb46 fails, 261120-124619-d71c0d passes).
  Recorded in track-c-c2.md as pre-existing and fixed upstream.
- C4 suite macOS 22485b and Linux c0320e: `consensus_mutants` hit ctest's TIMEOUT 1800 with 14/16
  and 9/16 verdicts, each a kill; no mutant hangs (each has its bounded() deadline), the total is too
  long for 16 mutants. Told Detent (msg-170); ruling under Rulings.
- C4 FreeBSD suite 94f5e9: skipped because it ran `--after` the failed Linux suite.

Everything is rerun at consensus main 8801b99 (pins journal 9e11713, completion d8b538c: no `--ref`
override now) and journal main 178030b. Scripts: ~/.scratch/c2-gate/body-v4.sh and vm-v5.sh (C2),
~/.scratch/xapiand/track-c-gates-main/{body.sh,vm.sh,submit-vm.sh} (C4 and the VM chains). Job ids
by row are in the reports. The H1 control mutant at 8801b99 is consensus:f066c51.

Results at e1f249d to watch at main: T7c 3.27 times at 10 ms (miss, target 4); T7d's 4 KiB arm at
10 ms 1.56 times (one run, past 1.5); T7a's 20% clause has no bench figure (open for review);
T8 65.7 CPU-minutes, 168M units, clean.

## Next

1. When the jobs finish, replace each "Queued" cell in docs/gates/track-c-c2.md ("At consensus main"
   table) and track-c-c4.md from the logs (/Users/kronuz/.scratch/campaign/jobs/ID/a1/log).
2. VM chains: Linux = 0bef2c a1 -> 3b7ebd a2 -> dd170c a3 -> 124538 C2 consensus -> 2096f8 C2
   journal -> ab18c1 C4 suite -> 5ce427 mutants; FreeBSD = dd591f -> d74166 -> f54066 -> 1ec7d9.
   A failure skips the rest of its chain: resubmit with submit-vm.sh (bash; zsh does not split
   `--env` lists).
3. CI at consensus 8801b99: run 44829606479 (queued at 15:20).
4. Forward the per-mutant seconds (ce0b7a, 5ce427, 1ec7d9) to Detent for
   consensus-mutants-per-test's TIMEOUTs.
5. When every cell is filled: State "ready for review".

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | docs/gates/track-c-c2.md (artifact): VM rows filled at journal d90797c / consensus 5923b6c; rerun at consensus 8801b99 and journal 178030b queued; pending cells marked; T9 on faultfs job ids: f3b916 dc4a0e e136e7 7b6023 27e504 (macOS), 3261e1 f6e7a6 (VM journal) |
| A2 | docs/gates/track-c-c4.md: every row with results at consensus e1f249d and its rerun job at 8801b99 (queued); T7e and D18 not runnable (group-heal) |
| A3 | architect's review, launched by the lead |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

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
