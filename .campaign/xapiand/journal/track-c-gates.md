# track-c-gates: journal

State: waiting on jobs 261118-115848-f3b916 261118-115851-dc4a0e 261118-115852-e136e7 261118-115853-7b6023 261118-115854-27e504 261118-115856-711c4e 261118-115858-715ac7 261118-115900-e84a48 261118-115901-bbd336 261118-115910-a06ad3 261118-115920-3261e1 261118-115921-23c9df 261118-115922-f6e7a6 261118-115923-898c2d
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 (C2 rerun, strata:86844bb0, docs/gates/track-c-c2.md). Done and in the report: macOS rows at journal 4d364f1, own-pins rows, 240-seed cluster run (pass), five fuzz jobs (pass, 7.57M executions). T9 on faultfs at journal d90797c (lead's msg), submitted: build f3b916, rows dc4a0e, turn_log_power 200 seeds e136e7, mutants 7b6023 and 27e504, consensus build 711c4e and rows 715ac7, Linux/FreeBSD VM chain 3261e1 23c9df f6e7a6 898c2d. Finding: the T2 budget job 083057-825f85 failed on the Driver simulation at seed 399 of 10,000 (healed group: node 3 applied 120, application 0); replays of seed 399 at journal 4d364f1 (a06ad3), consensus's own pins (e84a48) and journal d90797c (bbd336) attribute it. Host was loaded (light slots full); jobs queued. A2 skeleton committed (strata:6c239fcb).

## Next

1. When the jobs finish, fill the Pending cells of docs/gates/track-c-c2.md from each log (/Users/kronuz/.scratch/campaign/jobs/ID/a1/log). A job that timed out in the load window is invalid: rerun it as an ordinary rerun.
2. Seed 399: if it fails at every journal, report it to the lead as a consensus finding (core or Driver harness), with the replay command `SIM_DRIVER_SEEDS=1 build/consensus_simulation_test 1 3000 399`; if only at one journal, name it. Then run the budget past it (seeds 400-10000) so the rest is covered.
3. A2 fills after consensus-hello-limits and consensus-flow-bench land. A3 is the architect's.

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
- The first jobs 261118-082210-ac3d79 ... 261118-082213-e9d892 failed from my shell quoting (zsh), not evidence; the first Linux journal job 261118-082315-f9c2a9 failed three tests from the VM's umask 0002 (rig), reruns use umask 022.
