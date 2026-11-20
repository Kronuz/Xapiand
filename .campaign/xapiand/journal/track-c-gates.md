# track-c-gates: journal

State: waiting on jobs 261120-122827-7d5c46 261120-122828-f3c9b9 261120-122831-927ca9 261120-122833-d9a63d 261120-122853-f50b1e 261120-123408-3ac775 261120-123410-8a605b 261120-123412-167bb5 261120-123414-ecad78 261120-123416-4346ee 261120-123419-22485b 261120-123421-4452cc 261120-123429-dee769 261120-123439-a293ec 261120-123441-f9ff24 261120-123443-3f67e6 261120-123446-52c072 261120-123449-c0320e 261120-123452-94f5e9
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged main into the quest branch (strata:2a72de39), no conflicts. C2: the 13 jobs read; the macOS
faultfs rows are filled in docs/gates/track-c-c2.md (strata worktree, strata:9e975691). Findings:

- Driver seed 399 fails identically at journal 4272229 (e84a48), 4d364f1 (a06ad3) and d90797c
  (bbd336): "the healed group did not apply a new entry on every data voter", node 3 committed 120,
  applied 120, application 0 (applied since its restart). A consensus finding (its Driver or its
  simulation harness: `test/driver_simulation.cc:185`), not journal's. Replay:
  `SIM_DRIVER_SEEDS=1 build/consensus_simulation_test 1 3000 399`. The budget past it (seeds
  400-10000, job f50b1e) is queued.
- The Linux VM job 3261e1 failed journal_crc_fixture ("journal file requires exclusive ownership and
  a singly linked regular inode"): rig, `git archive` extracts with tar.umask 002 (664/775). Its
  chain (23c9df, f6e7a6, 898c2d) was skipped. vm-v5.sh uses `git -c tar.umask=022 archive`; the four
  VM jobs are resubmitted (7d5c46, f3c9b9, 927ca9, d9a63d), chained.

C4 (docs/gates/track-c-c4.md): consensus-hello-limits is dropped (folded into consensus-rpc-lane,
landed, consensus:e1f249d), group-install is dropped (folded into group-heal, planned): T7e and D18
have nothing to run and say so. consensus-flow-bench landed (measured at e7a453b). Jobs submitted at
consensus e1f249d: T8 hello fuzz (build 3ac775, four 1,000 s runs 8a605b 167bb5 ecad78 4346ee);
Suite macOS 22485b, Linux c0320e, FreeBSD 94f5e9 (VMs chained after C2's VM chain); H1-H4 4452cc and
the mutant control dee769 (mutant commit consensus:53b223a); T7a-c, T7d, R19 as exclusive VM jobs
f9ff24, 3f67e6, 52c072 (build a293ec). CI is green at e1f249d (run 44778137504, 8 platforms).
The lead's deferred check 4feb50 (consensus-rpc-lane) failed from `-R rpc_stream_fuzz` matching no
test, a wrong name, not a crash.

## Next

1. When the jobs finish, fill the Pending cells of docs/gates/track-c-c2.md and track-c-c4.md from
   each log (/Users/kronuz/.scratch/campaign/jobs/ID/a1/log). A job that timed out in the load
   window is invalid: rerun it. Note the T7 figures against their targets (T7c was 3.70 times at
   e7a453b, below its 4 times).
2. Fill the T8 row's CPU time from the `time -p` lines (user + sys) of the four fuzz jobs.
3. The VM chain depends on C2's last job (d9a63d); if one fails, the C4 VM jobs are skipped:
   resubmit them. The C4 VM jobs run exclusive and wait for the host load to settle.
4. When every cell is filled: State "ready for review".

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | docs/gates/track-c-c2.md (artifact) with jobs; pending cells marked; T9 on faultfs job ids: f3b916 dc4a0e e136e7 7b6023 27e504 (macOS), 3261e1 f6e7a6 (VM journal) |
| A2 | docs/gates/track-c-c4.md skeleton at strata:6c239fcb; every row pending, waiting on consensus-hello-limits, consensus-flow-bench, group-install |
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
