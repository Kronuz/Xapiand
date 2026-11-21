# netlab-consensus: journal

State: waiting on jobs 261120-172038-99d4ac (diagnosis, light) 261120-172057-f95412 (selftest)
Quest: .campaign/xapiand/quests/netlab-consensus.md (the lead's; never edit it)

## Doing

Takeover (fresh contributor, 2026-11-20). The exclusive rows at consensus:2dd22b3
(261120-144148-bb326b, 261120-144150-16ef0b) measured every shape and died in table() on the
loss row's `{'error': 'no answer'}`. consensus:a447b7d: table() writes error cells; the A1 round
trip expects set + unshaped + 2 timer wakes measured by an hrtimer sleep at that delay (the 1 us
hold never arms a real timer); roles reset per shape (the isolated leader's "term 11/8" was
earlier shapes' clusters: from the cut on, the cut leader only stepped down at 2,006 ms in term 1
(CheckQuorum) and followed term 2 after heal; consensus has pre-vote). Diagnosis job: calibrate
curve, then loss1%/reorder5% at 3 nodes with --diag (applied index + leader ss -tin each 0.5 s),
load wait 90 s, rpc_timeout 500 vs 5000. Hypothesis: core.h resets a flight after rpc_timeout
(500 ms) and resends the whole 2 MiB batch; at 1% loss a 2 MiB append takes longer than that.

## Next

1. Read the diagnosis: calibration (does excess = 2 x late?), loss rows at 500 vs 5000.
2. Set the jitter term from the calibration data; rerun the exclusive rows at the fixed head.
3. Write docs/perf/netlab-consensus.md (A3), push.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | selftest 261120-172057-f95412 pending; rows pending a rerun at the fixed head (2dd22b3 rows failed in table()) |
| A2 | the same rows' partition-1ms history check, pending |
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
  held floor (ping with netem holding each packet 1 us each way: the path plus the guest timer's
  latency on a hold, measured first), which is also the jitter allowed (at least 0.1 ms). The
  unshaped round trip is recorded beside it.
- Hunt rows use ping's measured round trip with A1's 10% margin: commit p50 (one in flight) over
  RTT + loopback p99, commit p99 over 2 RTT + loopback p99, RPC p99 over RTT + loopback p99, an
  election over 3 timeouts.
- Throughput: 256 small or 32 large entries in flight (the append batch's 256 entries and 2 MiB).
  RPC: one cold call opens the lane's Bulk link, reported apart from the timed calls.
- A1 majority only: per shape, the leader and as many followers as keep it a minority are cut off;
  a write there must stay unacknowledged through heal while the majority's new leader commits one.
- A2 election bound: election_max + heartbeat + 2 RTT + 200 ms (one timeout and a vote round); the
  hunt threshold is 3 election timeouts. The partition shape runs on 1 ms one-way delay.
- History: 3 clients per node, 60% writes, 4 keys, 25 ms think time, 1 s op timeout (then unknown,
  and the client takes a new name); final linearizable reads of every key on the healed leader.
- faultlab pinned at faultlab:b64102f (bench/netlab/FAULTLAB_REV): its Python equals Xapiand's
  pin a3b71d1; only README and CI changed since.
