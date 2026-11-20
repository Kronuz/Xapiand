# netlab-consensus: consensus measured on a realistic network: netlab delay, loss, reorder and partitions

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status netlab-consensus`.

## Outcome

consensus's numbers stop being loopback-only. A benchmark in `bench/netlab/` runs 3 and 5 consensus
nodes, one per Linux network namespace (faultlab netlab, pinned by commit) or one per guest of
Xapiand's VM tier (`tools/vm/cluster.sh run`, whose L2 carries netlab's shapes), and measures, at
each network shape: election time after the leader is killed, commit latency (p50, p99) and
throughput for small and 64 KiB entries, and RPC-lane call latency. Shapes: loopback (the baseline),
1 ms, 20 ms and 80 ms one-way delay, 1% loss at 20 ms, reorder 5% at 20 ms, and a partition of the
leader then heal. `docs/perf/netlab-consensus.md` holds the table against loopback and names what
degrades past what the delay explains (for example commit p99 above 2x the round trip, or elections
past 3 election timeouts).

## Acceptance criteria

- A1 The benchmark runs every shape at 3 and 5 nodes and checks its own sanity: each shape's
  measured round trip is within 10% of what netlab set, and a commit is acknowledged by a majority
  only: `bench/netlab/selftest.py` (no root; a fake runner) and one VM job per node count
- A2 After a partition of the leader and heal, the cluster elects within its election-timeout bound
  and every acknowledged write is present: the run's event log passes faultlab's history checker
  (`linearizable`) in the same jobs
- A3 [artifact] `docs/perf/netlab-consensus.md`: the table per shape and node count with job ids,
  each row against loopback, and findings with numbers; anything that degrades past the delay is a
  hunt row for the lead

## Contracts

Consumes faultlab netlab (`Netlab`, `delay`, `loss`, `reorder`, `partition`, `heal`, the event log)
and the VM tier's `run` command; changes no consensus API.

## Decisions

- Lead, 2026-11-20: measured only on Linux (netlab needs network namespaces); macOS's proxy tier
  colors timings and is not used for numbers. Timing runs are `strict` or `exclusive` jobs in the VM
  (QUIET_VM), never on the host loopback.
- Owner, 2026-11-20: use faultlab and netlab for realistic-network numbers.

## May land early

The benchmark and its selftest before the VM numbers.

## Tasks

None: one contributor (performance, strong, high).
