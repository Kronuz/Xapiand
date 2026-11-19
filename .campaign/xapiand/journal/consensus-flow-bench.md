# consensus-flow-bench: journal

State: ready for lead (review CHANGES folded: F1-F4, F6-F8 in consensus:d9605f4; CI on it pending)
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

0. Pushed consensus:d9605f4 (doc-only fixes for the review); build-test 9273aa reused (docs-only); earlier push consensus:d7091f6 (A3 rows + main merge); build-test 261118-222741-9273aa passed; check CI (freebsd was provisioning-flaky); correctness review.
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
| A1 | 261118-114928-9bef7d (exclusive, e7a453b) |
| A2 | 261118-114946-13cf10 (exclusive, e7a453b) |
| A3 | 261118-135806-e23693 (exclusive, e7a453b) |
| A4 | `docs/perf/flow-control.md` in consensus (A1, A2, A3 rows, consensus:afea684) |

Lead msg (all five jobs passed; diag confirms LogFull; fill rows, finish A4, push): A1/A2 rows
in consensus:33e0c5a, pushed; A3 rerun queued since 0a37d4 predates the merge of main's
transport changes. Diag 5a98ce ran beside light holds: sanity only, not cited as evidence.

## Result

consensus `docs/perf/flow-control.md`. A1 (9bef7d): 256 KiB appends, modeled barrier,
pipeline 4 against 1: 1.12x / 1.69x / 3.70x faster at 0 / 1 / 10 ms (54.4 vs 48.6, 55.5 vs 32.8,
54.8 vs 14.8 MB/s); fdatasync 1.62x / 1.83x / 4.53x. 2 MiB appends: pipeline 1 wins by 14-36%.
Pipeline 4 is held at ~55 MB/s (~210 commands/s) by the leader's two barriers per command
(proposal, then commit index per one-entry ack). A2 (13cf10): slow follower leaves the other 99
groups at 40.5 MB/s offered, node-2 apply p99 unchanged; slow group's undelivered <= 1 MiB. A3 (e23693): behind Bulk the control frame's longest delay
6.5-37.4 ms, longest gap 14.2-38.9 ms (includes the 10 ms cadence) against stated bounds of
15-122 ms (election_min 1 s); 300/300 control frames arrived in every row; Control-lane data
waits 1.6-2.0x Bulk's p50 on a 1 Gb/s link. Single 3 s run per row.

CI at 33e0c5a: only freebsd-x86_64 failed, in vmactions provisioning (pkg fetch error while
installing cmake etc., `ssh exited with code 3`), before any build; the commit changed docs only.
Not mine; rerun on the merge head. Review not waived: the bench/doc are measurement claims
(gap metric includes the 10 ms cadence; bound compared at the measured rate), which a fresh reader
should check; no product code changed.

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

Review consensus-flow-bench-correctness (CHANGES at d7091f6), folded in consensus:d9605f4, doc only
(no code change, so confirmation cites the reviewer's probe job 261118-224935-cfdbeb: ping 0.02 /
2.8 / 12.2 ms, P3 repeats; `campaign verify` reused build-test 261118-222741-9273aa):
- F1 rows relabeled by measured RTT (2.8 and 12.2 ms), netem note and ping values in "How".
- F2 Control rows reported first as R19's case with four-repeat ranges; Bulk stated as a separate
  connection (links.h slots by node and lane) showing cross-connection interference; the two
  exceedances called single tail events; "does not claim" dropped.
- F3 table compares delay max to the bound (gap kept last as liveness); F4 A2 scoped to a slow
  barrier; F6 single-run rows marked; F7 "about three quarters busy"; F8 idle tail and disk clause.
- F5 (no value test) for the lead's hunt list; F9 (T7 gate text vs quest criteria) the lead's call.
