# netlab-consensus: journal

State: waiting on detent:consensus-full-window-stall (D165), then the lead reruns A1/A2 at the merged head; ready for review at consensus:c1a2138. A1/A2 jobs are red only on the loss row's stall checks (Finding 1, Detent's fix); every other check passed. Narrowing jobs cancelled (msg-185).
Quest: .campaign/xapiand/quests/netlab-consensus.md (the lead's; never edit it)

## Doing

Exclusive rows and the calibration are queued at consensus:e12ee46 (a447b7d plus the doc's method
section). Diagnosis 261120-172038-99d4ac and selftest 261120-172057-f95412 (green) are read.

Calibration (in 261120-172038-99d4ac, light, on a busy host): ping's excess over the set round trip
against 2 x the median sleep lateness at the same delay:
| hold ms | 0.001 | 0.1 | 0.5 | 1 | 2 | 5 | 20 | 80 |
| excess ms | 0.29 | 2.94 | 2.14 | 1.19 | 2.02 | 3.00 | 7.35 | 3.00 |
| 2 x late p50 | 0.30 | 2.97 | 2.17 | 1.16 | 2.24 | 3.00 | 5.63 | 2.45 |
It agrees within 0.25 ms from 1 us to 5 ms. At 20 and 80 ms the wakes' spread is wide on a loaded
host (late p90 8.4 and 5.1 ms), which the check's spread term absorbs. The exclusive calibration
gives the table for the doc.

## Finding for Detent: 64 KiB entries at 32 in flight stall under 1% loss and 5% reorder

Measured in 261120-172038-99d4ac (3 nodes, 20 ms one way, light): `--diag` sampled every node's
applied index and the leader's `ss -tin` every 0.5 s, with the load wait raised to 92 s.
- It is not the bench's client timeout. With loss 1%, once the 32 x 64 KiB load starts, every
  node's applied index stays at 3,689 (rto 500) and 3,648 (rto 5000) from 4 s to 90 s. The load
  commits at most about 3 entries and never answers in 92 s.
- It is not the rpc_timeout resend alone. With `Timing::rpc_timeout` 5000 ms instead of 500
  (core.h:1121 resets a timed-out flight), loss still stalls, and reorder 5% stalls as well. At
  500 ms reorder finished slowly: 65 commits, p50 1,919 ms, 1.04 MB/s.
- TCP delivers what it is given: bytes_acked tracks bytes_sent on both follower sockets. At
  rto 500 the leader sends about 8.5 MB per follower in the first 16 s (roughly 4 copies of the
  2 MiB batch), then the sockets go idle: Send-Q 0, about 1.2 KB/s, heartbeats only, to 90 s.
  At rto 5000 it sends about 2.1 MB per follower (one batch) and is idle from 8 s.
- So the leader holds uncommitted entries and stops replicating them, while heartbeats flow and
  no election happens. One in flight (64 KiB) commits under the same loss: 7.7/s, p50 128 ms.
  32 x 65,536 B is exactly `Limits::rpc_bytes` (2 MiB, types.h:61): one full batch per flight.
  Candidate sites: the follower's capacity rejections (core.h:938-952 answer failure without a
  hint) with `retry_after = deadline(rpc_timeout)` (core.h:1021), and the batch bound at
  core.h:1183-1187. Not pinned down; it needs a consensus-side trace of the stuck flight's
  responses.
- Narrowing (sent to Detent by the lead; consensus unchanged): consensus:e1144c2 adds
  --large-bytes/--large-inflight and an append trace in node.cc's status (appends sent with
  entries, their entries/bytes and largest batch; answers ok/fail and the last fail's next_hint).
  Light jobs, 3 nodes, loss1% and reorder5% at 20 ms, --diag: 31 x 64 KiB 261120-180108-07a97a,
  32 x 64 KiB 261120-180110-7ec9f4, 64 x 32 KiB 261120-180112-b951fd (same 2 MiB, twice the
  entries). Stall at 31 and not at 64 x 32 KiB points to bytes; stall at both 32 and 64 x 32 KiB
  points to the 2 MiB batch, not the entry count.

## Next

1. Lead: decide whether A1/A2 stand on the logs' passing round-trip, majority-only and partition
   checks (the two exclusive jobs exit 1 on the loss row alone), or wait for Detent's
   consensus-full-window-stall and rerun both rows to turn them green.
2. Hunt rows for the lead: docs/perf/netlab-consensus.md Findings 1 (stall), 2 (reorder doubles a
   64 KiB commit to 2 round trips), 3 (loss: 4 round trips), 7 (3-node election 3.5 s at 80 ms).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | selftest 261120-172057-f95412 (green, a447b7d). Rows at consensus:e12ee46: 3 nodes 261120-174938-19c0ac (43 of 45 checks), 5 nodes 261120-174941-0e6ee9 (44 of 45). Every shape's round trip and majority-only check is `ok` in both logs (ping medians within 1.1 ms of set + floor); the failures are loss1%-20ms large_throughput (both) and its 3-node election 6,276 ms, the stall. Calibration 261120-174928-5e07e0. |
| A2 | partition-1ms in the same two jobs: elects 1,395 ms (3) and 1,455 ms (5) against a 2,304 ms bound, rejoin 1,423 and 1,480 ms, 0 violations in 2,411 and 3,968 operations |
| A3 | `docs/perf/netlab-consensus.md` at consensus:c1a2138: tables per node count with job ids, calibration, hunt rows, 8 findings |

## Result

consensus:40670e3 adds `bench/netlab/`: `node.cc` (one node per process: Driver, journal, host
transport; stdin commands, JSON events stamped with CLOCK_MONOTONIC), `bench.py` (the shapes, the
checks, the history workload, the table), `selftest.py` (fake runner and cluster, four mutants),
`vm.sh` (build and run in one guest of Xapiand's VM tier).

## Rulings I made
- msg-185 (lead, 11-20): the full-window stall is election churn. A Driver group's Core sends up to
  rpc_bytes (2 MiB) per append; at ~0.5 MB/s under 1% loss or 5% reorder it holds the Control
  connection ~4 s, heartbeats and votes queue behind it, and CheckQuorum steps the leader down;
  31 x 64 KiB stalls the same way. Narrowing jobs 07a97a, 7ec9f4, b951fd cancelled. Detent's
  consensus-full-window-stall adds per-peer append pacing and reruns this bench at e12ee46.

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
- A1 round trip (replaces the 1 us held floor, which never arms a real timer): expected = set +
  unshaped + 2 x the median lateness of 30 sleeps of the shape's delay (timer slack 1 ns),
  measured right after the pings; allowed = 10% of set + max(0.1 ms, 2 x (late p90 - median)).
  The spread term is measured, not a widened constant; the calibration supports the model.
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
- Role changes reset per shape (Bench.start). The partition row's isolated-leader term had read
  earlier shapes' clusters ("term 11/8"); the selftest now fails without the reset.
