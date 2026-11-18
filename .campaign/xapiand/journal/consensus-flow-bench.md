# consensus-flow-bench: journal

State: waiting on jobs 261118-112720-46b10c 261118-113805-593a91 261118-113818-5a98ce 261118-114928-9bef7d 261118-114946-13cf10
Quest: .campaign/xapiand/quests/consensus-flow-bench.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code in consensus worktree `consensus-wt/xapiand/consensus-flow-bench` (branch
quest/xapiand/consensus-flow-bench). `bench/flow_bench.cc` (modes barrier, one, many, gap),
`bench/flow_bench.cmake` (adds the target via `CMAKE_PROJECT_consensus_INCLUDE`),
`bench/flow_bench_vm.sh smoke|a1|a2|a3` (VM 18782, netem on lo; starts/stops the VM only if
it found it stopped). macOS build + dry run: job 261118-034144-72af4b; VM smoke 261118-034556-43f576 ran every mode (VM fdatasync p50 105 ms for 2 MiB:
the host's full flush). Smoke 261118-035112-b6d316 passed (modeled 1 ms barrier measures 2.1 ms p50 in the VM;
one P4 at 10 ms 15.7 MB/s; many slow10 hot: 99 groups keep 40 MB/s). Queued exclusive: exclusive A1 261118-035113-5edaf4,
A2 261118-035115-004ebb, A3 261118-035116-0a37d4.

Session 2 (resumed): A1 5edaf4 and A2 004ebb died of SIGPIPE (exit 141) in a Cluster's
shutdown: the bench did not ignore SIGPIPE (connection.h makes it the host's duty on Linux).
A1's rows also show zero-commit rows and P1 above P4: the bench never checkpoints, so the
default 64 MiB log (256 x 256 KiB) fills and every later proposal is refused LogFull
(hypothesis; diag job 261118-112702-edf7a9 counts refusals at 64 vs 1024 MiB). The three
earlier failures (525386, fe310f, da434f) were fixed by e02b94b/84a9571 and superseded by
passing smokes. consensus d511a44: SIGPIPE ignored, refusal counters, FLOW_LOG_MIB. Merged main
(e8a41de); pushed; build-test 261118-112720-46b10c.

## Next

0. Diag edf7a9 timed out rebuilding in the VM beside light holds (exit 124): e7a453b adds a
   `build` mode run as its own light job (593a91), diag 5a98ce after it, A1 9bef7d and A2
   13cf10 (exclusive) after the diag, build-test 46b10c at d511a44 (rerun at the final head).
   Read diag: confirm LogFull; find what holds P4 below P1 (rtt 0 slower than rtt 10). If the
   bench changes, resubmit a1/a2 (the new submission replaces the queued ones).

1. Fix what the macOS dry run shows; VM smoke job (light), then a1, a2, a3 as exclusive jobs
   `--after` the smoke.
2. docs/perf/flow-control.md from the jobs' output, each row's job id.

## Blocked on

Nothing. Finding for the lead (not blocking): the Driver cannot pipeline. `Worker` builds
every Core with `Flow{}` (worker.h:144,172,173,646) and `GroupOptions` has no `Flow`, so
`pipeline = 1` is fixed for every Driver group. The bench drives `Core` directly over the
host transport for that reason. Fix (not mine; files `include/consensus/driver.h`,
`include/consensus/worker.h`): a `Flow flow{}` in `GroupOptions` passed to each `Core`.
Lead (msg): no CMakeLists.txt edit; the bench loads from the configure line
(`-DCMAKE_PROJECT_consensus_INCLUDE=$PWD/bench/flow_bench.cmake`), in flow_bench_vm.sh and the
doc's reproduce line. Findings 1-2 went to Detent (driver-pipeline); the doc must state
per-group commits/s at pipeline 1 and pipelined side by side (commands / timed seconds from
the `one` lines).

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
- R5 The VM script keeps a marker (~/.scratch/flow-bench-vm-started) for a VM it started, so a
  job stopped mid-run leaves the VM for the next flow job to stop: superseding a running smoke
  (261118-034150-b883f7) left the VM up. macOS dry run 261118-034144-72af4b: every mode runs.
- R6 Rows run a modeled 1 ms barrier (FLOW_BARRIER=1000: sleep after the page-cache write)
  beside real fdatasync: the VM's fdatasync is ~100 ms (qemu on APFS full flush), which caps a
  group at one command per leader barrier and hides the transport. Core persists one proposal
  at a time (Propose while busy is Rejected Busy): per-group throughput <= command / leader barrier.
- R7 The bench ignores SIGPIPE (connection.h: the host's duty on Linux) and sets the retained
  log limit to 1 GiB (FLOW_LOG_MIB): it never checkpoints, and 64 MiB holds 256 commands.
- R8 The VM build is its own light job (`flow_bench_vm.sh build`): timed jobs only measure.
- R4 Unsent bytes of the slow group = entry bytes its leader Core sent to node 2 minus entry
  bytes node 2's loop was handed (sampled every 2 ms): what the transport holds for it.
