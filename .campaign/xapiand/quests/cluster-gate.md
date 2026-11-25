# cluster-gate: The release's cluster gate

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status cluster-gate`.

## Outcome

The cluster milestone, measured and reviewed once at the end (replan-2026-11-20, section 1). It
builds nothing: it runs once `group-heal`, `placement`, `journal-replaces-wal` and `cluster` have
landed, on local processes and 3 to 5 local VMs, never AWS, and its report,
`docs/gates/cluster-release.md`, holds every row with its commit and jobs. `format-freeze` lands after
it, and `release` waits on it (D117). Reviewed with the `system` lens, over the four landed features
together.

## Acceptance criteria

- A1 R1 Parity: section 1's checklist on 3 and 5 processes and on 3 and 5 VMs (jobs)
- A2 R2 No acknowledged loss: 200 seeded runs of 60 s per fault tier (`kill -9`, `SIGSTOP`, proxy
  partitions) with zero `quorum` writes lost; histories of `quorum` writes and `linearizable`
  get-by-id linearizable, `prefix` get-by-id a consistent prefix; VM power loss through
  `dm-log-writes`, 20 runs at D14's sampled marks (jobs, each at most 15 minutes)
- A3 R3 No split brain: at most one leader per group per term, and digests per group, stored blobs
  included, equal after healing, from R2's runs (the same jobs' checker output)
- A4 R4 Simulation: `group-heal`'s simulator rows (10^7 steps over 1,000 seeds, 3 and 5 voters), cited by job
- A5 R5 Failover: write unavailability after a leader kill, p99 over 100 kills on VMs, stated
  against the old 300 s stall plus a 10 to 30 s election (jobs)
- A6 R6 Migration under the old `consensus-fetch-macos` load on macOS: `cluster`'s migration run (its A6), cited
- A7 R7 The journal's cost: journal-replaces-wal's T11, T12 and Q14 jobs, cited
- A8 R8 Against the Xapian-era cluster: ingest, search and aggregation on the same 3 VMs, beating
  the final Xapian release on every row, ingest at `ack=none` with `quorum` beside it; the old
  cluster on D14's multicast L2; five interleaved trials (`exclusive` jobs)
- A9 R9 Every gate: CI's sanitizers green at the gated commit, one CPU-hour of fuzzing per new
  decoder (the RPC lane, the shard RPC, index patterns), fixtures, coverage (CI and jobs)
- A10 R10 Scale statement: memory, descriptors and idle CPU per group at 10^4 groups per node;
  first-write latency for a new index and after a reopen, against master; the per-host retained turn
  log after 10^4 write-once indexes go idle; keepalives per second against the node count; the
  root's records, memory, checkpoint and install, a node's cache, and end-to-end index creations per
  second against index and shard counts (`placement`'s measurements, and the scale rows folded from the old shard-groups A8 and A9,
  distributed-search A5 and A7 and cluster-membership A5; one `exclusive` job of at most 10 minutes
  each, and the statement)
- A11 [decision] cluster-release Q4, the gate's scope: pending owner; lead default, 3 to 5 local VMs
  (D91's open question), the 30- and 300-node rows after the release (D35)
- A13 The stale-journal fix is in the release (D182): strata-journal-behind landed, Xapiand's strata and
  consensus pins include it, and the durability review's stalejournal probe (journal-replaces-wal,
  job 261124-220619-fb40f3's script) refuses with every file byte-equal at the gated commit
- A12 [artifact] `docs/gates/cluster-release.md`

## Tasks

None: the lead (or one performance contributor) submits the jobs and writes the report.

## Contracts

Consumes every cluster feature's outcome (`group-heal`, `placement`, `journal-replaces-wal`, `cluster`); provides the gate report `format-freeze` and `release`
read.

## Decisions

- Design section 7, D14, D35 (no 30- or 300-node hardware), the no-AWS ruling: VMs are local QEMU,
  driven by `tools/vm/cluster.sh` under quiet holds; Detent's VMs on 18762 and 18763 stay untouched.
- Design section 5: the old phase7b's A4 runs here at three nodes only; A5 follows the release.
- The 30- and 300-node rows and beating Elasticsearch at scale stay after the release (D91, D35).
- R8 needs `xapiand-final-release`'s binary; every `exclusive` or `strict` job is at most 10 minutes
  holding its interleaved pairs, rerun only when its interval straddles the target (D59, D61, D114).
  Findings are fixed forward, each fix with a test that fails without it.

## When done

The gate is the milestone's review and measurement, so it is itself "when done": one reviewer, lens
`system`, carrying correctness and security (membership and certificates); the rows above, as
`--later` light jobs at priority -1 where they are not timing (R2's 200 seeded runs per fault tier
and its VM power loss, R9's fuzz hours), and `exclusive` or `strict` jobs for the rest.

## May land early

The report's rows as each lands, with the verdict last.
