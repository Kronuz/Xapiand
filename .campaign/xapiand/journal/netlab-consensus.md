# netlab-consensus: journal

State: waiting on jobs 261120-141708-9ccd3e 261120-141710-78f9a8
Quest: .campaign/xapiand/quests/netlab-consensus.md (the lead's; never edit it)

## Doing

Dry runs: selftest green (261120-141201-2d02d1), node built in a guest in 194 s
(261120-141202-44cee0), 3 nodes at loopback quick passed 6 of 6 checks (261120-141208-19c21e).
Now every shape, quick, at 3 (261120-141708-9ccd3e) and 5 nodes (261120-141710-78f9a8), consensus:113572a.

## Next

1. Read the dry run's log; fix what it shows (guest tools, clock check, timings).
2. Full sweep: one exclusive job per node count, `bench/netlab/vm.sh run --nodes 3|5`, `--for A1,A2`.
3. Write `docs/perf/netlab-consensus.md` from `bench.py table` with the job ids (A3).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | selftest 261120-141201-2d02d1; VM jobs pending |
| A2 | VM jobs pending |
| A3 | `docs/perf/netlab-consensus.md` pending |

## Result

consensus:40670e3 adds `bench/netlab/`: `node.cc` (one node per process: Driver, journal, host
transport; stdin commands, JSON events stamped with CLOCK_MONOTONIC), `bench.py` (the shapes, the
checks, the history workload, the table), `selftest.py` (fake runner and cluster, four mutants),
`vm.sh` (build and run in one guest of Xapiand's VM tier).

## Rulings I made

- One namespace per node in one guest (`cluster.sh up --nodes 1 --cpus 8`), not one guest per node:
  netlab's veths and netem act in one kernel with no relay on the path (the guests' L2 goes through
  the guardian's Python hub, which loses 2-26% of bursts and adds its own latency), every node reads
  one CLOCK_MONOTONIC so a kill and the next leader's event compare, and the quest's contract is
  netlab's own API (Netlab, delay, partition, its event log). Cost: the nodes share 8 vCPUs.
- The VM-tier `up`/`ssh`/`down` commands, not `run`: `run` takes no `--cpus`, and 2 vCPUs for 5
  nodes would color the numbers. vm.sh parks the guest by its EXIT trap, and `up --owner $$` makes
  the guardian park it when the job's shell dies.
- The node is consensus's production path (Driver over journal over PosixIO, the host transport),
  not flow_bench's in-process Core loop, so a killed leader restarts on its journal and reads are
  Driver::read (read index). Defaults kept: Timing (election 1-2 s, heartbeat 100 ms), Flow
  (pipeline 1), Limits except log_bytes 768 MiB (no checkpoints in a run).
- Journals on tmpfs (/dev/shm) in an 8 GiB guest: the first dry run (261120-141208-19c21e) put
  them on the guest's disk and loopback commit p50 read 3.0 ms of fdatasync; on tmpfs a shape's
  delta over loopback is the network's. 64 KiB runs stop at 2,048 entries (128 MiB per node).
- A1 round trip: ping's median from node 1 to each peer within 10% of the set round trip, past the
  unshaped path's own round trip (the floor, measured first), which is also the jitter allowed (at
  least 0.1 ms).
- A1 majority only: per shape, the leader and as many followers as keep it a minority are cut off;
  a write there must stay unacknowledged through heal while the majority's new leader commits one.
- A2 election bound: election_max + heartbeat + 2 RTT + 200 ms (one timeout and a vote round); the
  hunt threshold is 3 election timeouts. The partition shape runs on 1 ms one-way delay.
- History: 3 clients per node, 60% writes, 4 keys, 25 ms think time, 1 s op timeout (then unknown,
  and the client takes a new name); final linearizable reads of every key on the healed leader.
- faultlab pinned at faultlab:b64102f (bench/netlab/FAULTLAB_REV): its Python equals Xapiand's
  pin a3b71d1; only README and CI changed since.
