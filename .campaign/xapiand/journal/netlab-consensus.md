# netlab-consensus: journal

State: waiting on jobs 261120-144148-bb326b 261120-144150-16ef0b
Quest: .campaign/xapiand/quests/netlab-consensus.md (the lead's; never edit it)

## Doing

Sweep at consensus:2dd22b3: selftest 261120-144146-5b3dfb; exclusive rows `vm.sh run --nodes 3`
261120-144148-bb326b and `--nodes 5` 261120-144150-16ef0b (`--for A1,A2`). Smoke at 85f1968
(261120-143711-cc6bda, light, 3 nodes, loopback, 1ms, partition, quick): 20 of 20 checks; held
floor 0.71 ms against 0.07 unshaped; the cut leader reached term 3 isolated and named the term-2
leader 1.4 s after heal, no election after it. 2dd22b3 only adds the role trace to the result
(and the doc draft), so the smoke stands for it. Rows at 85f1968 (e3c9fd, 9444ba) cancelled.

Quick dry runs of every shape at consensus:113572a (3 nodes 261120-141708-9ccd3e, 5 nodes
261120-141710-78f9a8, light, a busy host): every check passed but the 1 ms round trip (ping
2.3-2.9 ms against 2 ms set, the guest's netem timer). Fixed in 85f1968 by measuring the held
floor. Seen: 1 ms commit p50 4.7-5.2 ms and RPC p50 4.1-4.7 ms against ping 2.5-2.8 (not at 20
ms); 64 KiB commit at 80 ms one way takes 2 round trips (325 ms); the first RPC call costs 4
round trips (Bulk dial, TLS, hellos).

## Next

1. When the exclusive rows finish (queued behind light holds at 14:55): read them; rerun a row only
   if it straddles a check; the partition's `roles` trace explains the cut leader's term 3 and its
   1.4 s rejoin.
2. Fill `docs/perf/netlab-consensus.md` (method drafted at consensus:2dd22b3; TABLE_* placeholders):
   `python3 bench/netlab/bench.py table $JOBS/<id>/a1/out/result-N.json --job N=<id>`, the
   machine section, findings and hunt rows (A3); commit, push.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | selftest 261120-144146-5b3dfb (green); rows 261120-144148-bb326b (3), 261120-144150-16ef0b (5) pending |
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
