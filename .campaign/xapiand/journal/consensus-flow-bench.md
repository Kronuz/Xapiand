# consensus-flow-bench: journal

State: waiting on jobs 261118-033049-525386 261118-033700-728743
Quest: .campaign/xapiand/quests/consensus-flow-bench.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code in consensus worktree `consensus-wt/xapiand/consensus-flow-bench` (branch
quest/xapiand/consensus-flow-bench). `bench/flow_bench.cc` (modes barrier, one, many, gap),
`bench/flow_bench.cmake` (adds the target via `CMAKE_PROJECT_consensus_INCLUDE`),
`bench/flow_bench_vm.sh smoke|a1|a2|a3` (VM 18782, netem on lo; starts/stops the VM only if
it found it stopped). macOS build + dry run: job 261118-033049-525386; VM smoke (dry run, strict): 261118-033700-728743.

## Next

1. Fix what the macOS dry run shows; VM smoke job (light), then a1, a2, a3 as exclusive jobs
   `--after` the smoke.
2. docs/perf/flow-control.md from the jobs' output, each row's job id.

## Blocked on

Nothing. Finding for the lead (not blocking): the Driver cannot pipeline. `Worker` builds
every Core with `Flow{}` (worker.h:144,172,173,646) and `GroupOptions` has no `Flow`, so
`pipeline = 1` is fixed for every Driver group. The bench drives `Core` directly over the
host transport for that reason. Fix (not mine; files `include/consensus/driver.h`,
`include/consensus/worker.h`): a `Flow flow{}` in `GroupOptions` passed to each `Core`.
Also needed, not mine: `CMakeLists.txt` two lines adding `consensus_flow_bench` (until then
`bench/flow_bench.cmake` does it).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- R1 The bench runs `Core` per group (not the Driver) over the host transport, with its own
  storage: one thread per node writing every pending persist into a preallocated 64 MiB ring
  and making them durable with one fdatasync (a turn barrier); the Driver fixes pipeline 1.
- R2 "Barrier slowed 10x": node 2 stores group 0 on its own storage thread whose barrier
  sleeps 9x its measured fdatasync after each one; the unslowed A/B arm uses the same split
  storage with factor 1.
- R3 Commit p99 with 3 voters does not see node 2 (leader + node 3 commit), so A2 also
  reports each group's apply latency at node 2, the head-of-line signal.
- R4 Unsent bytes of the slow group = entry bytes its leader Core sent to node 2 minus entry
  bytes node 2's loop was handed (sampled every 2 ms): what the transport holds for it.
