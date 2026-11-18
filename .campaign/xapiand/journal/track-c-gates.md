# track-c-gates: journal

State: waiting on jobs 261118-082955-4b86f4 261118-082956-6d198b 261118-082957-dff3b5 261118-082959-bbd67d 261118-083001-9d463a 261118-083002-d7b981 261118-083057-825f85 261118-083059-454b43 261118-083105-13747c 261118-083106-671b49 261118-082454-b80095 261118-082456-8710c4 261118-082457-6ddcb3 261118-082459-df4d64 261118-082501-0416fc
Quest: .campaign/xapiand/quests/track-c-gates.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1: C2 rerun (code worktree strata-wt/xapiand/track-c-gates, commit strata:`git log`; docs/gates/track-c-c2.md). macOS journal and consensus rows are in the report; pending jobs (state "Pending" in its table): the two mutant jobs, the Linux and FreeBSD VM chain, the T2 budget and the 240-seed cluster run, consensus at its own pins, five 1,500 s libFuzzer jobs. A2: docs/gates/track-c-c4.md skeleton is committed, every row pending on consensus-hello-limits, consensus-flow-bench or group-install, no numbers.

## Next

1. When the jobs finish, fill the Pending cells of the C2 table (result, status, job) from each job's log (`/Users/kronuz/.scratch/campaign/jobs/ID/a1/log`); record executed units per fuzz job; a failure is reported, not hidden.
2. Rig scripts: ~/.scratch/c2-gate/{body-v3.sh,vm-v3.sh}. If a VM job exits 75 the VM was running (not ours): resubmit.
3. A2 fills after consensus-hello-limits and consensus-flow-bench land. A3 is the architect's.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | docs/gates/track-c-c2.md (artifact) with jobs; pending cells marked |
| A2 | docs/gates/track-c-c4.md skeleton at strata:6c239fcb; every row pending, waiting on consensus-hello-limits, consensus-flow-bench, group-install |
| A3 | architect's review, launched by the lead |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- consensus main's own pins (journal 4272229, completion 7731315) do not build with journal main (`IO::sync_data_files`); consensus rows use `--ref` pins of journal 4d364f1 and completion f4c19b1 (journal's pin), and a row at consensus's own pins runs beside them. Not changed in consensus (lead's/Detent's).
- T8's CPU-hour: `journal_fuzz` takes both decoders through data[0]; five 1,500 s jobs (125 CPU-minutes) stand for the CPU-hour per decoder, stated as the binary's.
- The first jobs 261118-082210-ac3d79 ... 261118-082213-e9d892 failed from my shell quoting (zsh), not evidence; the first Linux journal job 261118-082315-f9c2a9 failed three tests from the VM's umask 0002 (rig), reruns use umask 022.
