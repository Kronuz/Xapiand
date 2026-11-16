# cluster-gate: The release's cluster gate

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status cluster-gate`.

## Outcome

The release's cluster gate (design section 7) runs on local processes and 3 to 5 local VMs, never
AWS, and its report, `docs/gates/cluster-release.md`, holds every row with its commit and jobs.
`format-freeze` lands after it. Reviewed with the `system` lens.

## Acceptance criteria

- A1 R1 Parity: section 1's checklist on 3 and 5 processes and on 3 and 5 VMs (jobs)
- A2 R2 No acknowledged loss: 200 seeded runs of 60 s per fault tier (`kill -9`, `SIGSTOP`, proxy
  partitions) with zero `quorum` writes lost; histories of `quorum` writes and `linearizable`
  get-by-id linearizable, `prefix` get-by-id a consistent prefix; VM power loss through
  `dm-log-writes`, 20 runs at D14's sampled marks (jobs, each at most 15 minutes)
- A3 R3 No split brain: at most one leader per group per term, and digests per group, stored blobs
  included, equal after healing, from R2's runs (the same jobs' checker output)
- A4 R4 Simulation: group-sim's rows, cited by job
- A5 R5 Failover: write unavailability after a leader kill, p99 over 100 kills on VMs, stated
  against the old 300 s stall plus a 10 to 30 s election (jobs)
- A6 R6 Migration: cluster-migration's jobs, cited
- A7 R7 The journal's cost: journal-replaces-wal's T11, T12 and Q14 jobs, cited
- A8 R8 Against the Xapian-era cluster: ingest, search and aggregation on the same 3 VMs, beating
  the final Xapian release on every row, ingest at `ack=none` with `quorum` beside it; the old
  cluster on D14's multicast L2; five interleaved trials (`exclusive` jobs)
- A9 R9 Every gate: CI's sanitizers green at the gated commit, one CPU-hour of fuzzing per new
  decoder (consensus-rpc-lane, shard-rpc, index patterns), fixtures, coverage (CI and jobs)
- A10 R10 Scale statement: memory, descriptors and idle CPU per group at 10^4 groups per node;
  first-write latency for a new index and after a reopen, against master; the per-host retained turn
  log after 10^4 write-once indexes go idle; keepalives per second against the node count; the
  root's records, memory, checkpoint and install, a node's cache, and end-to-end index creations per
  second against index and shard counts (placement-root A5, placement-repair A5, shard-groups A8,
  A9; measured jobs and the statement)
- A11 [decision] cluster-release Q4, the gate's scope: pending owner; lead default, 3 to 5 local VMs
  (D91's open question), the 30- and 300-node rows after the release (D35)
- A12 [artifact] `docs/gates/cluster-release.md`

## Contracts

Consumes every cluster quest's outcome; provides the gate report `format-freeze` and `release`
read.

## Decisions

- Design section 7, D14, D35 (no 30- or 300-node hardware), the no-AWS ruling: VMs are local QEMU,
  driven by `tools/vm/cluster.sh` under quiet holds; Detent's VMs on 18762 and 18763 stay untouched.
- Design section 5: phase7b's A4 runs here at three nodes only; A5 follows the release.

## May land early

The report's rows as each lands, with the verdict last.
