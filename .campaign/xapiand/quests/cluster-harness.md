# cluster-harness: Multi-process and VM cluster harness

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster-harness`.

## Outcome

Xapiand has the two harness tiers of `docs/design/cluster-release.md` D14, so every later cluster
quest proves its claims with jobs instead of words. Tier 1 runs N `xapiand` processes on
`127.0.0.1` with their own ports and data directories, injects `kill -9`, `SIGSTOP` and partitions
through a TLS-transparent TCP proxy (Detent's `tests/detent_system/network.py` shape, written here),
and checks recorded histories for linearizable writes and get-by-id. Tier 2 boots 3 to 5 Linux VMs
under QEMU on one `-netdev socket,mcast=...` L2, SSH on ports 18780 and up, with a `dm-log-writes`
device under the data directory for power loss. Tier 1 runs in CI and as the `cluster` suite of
`harness/run_all.sh`; tier 2 runs under quiet holds. No AWS. Detent's VMs on 18762 and 18763 and
strata's on 18772 and 18774 are never touched.

## Acceptance criteria

- A1 The six items of the Xapian-era `harness/cluster_check.sh` (at `5a517b052^`, lines 144-321;
  design section 1), rewritten for N = 3 and 5 processes: every node sees every member and one
  leader leads; a write on node 1 is read on every node; a search from any node finds it; exact
  `_doc_count`, `_sum`, `_avg` over 40 documents on every node; a leader kill elects a new one and
  the killed node rejoins; `kill -9` with uncommitted writes then a restart serves all 40. A job at
  this quest's head shows N = 1 passing and the cluster items failing as expected on master (each
  failure named, none crashing the harness)
- A2 The history checker rejects three planted histories: a lost acknowledged write, a stale read,
  and a stale get-by-id answered by a partitioned old leader in a history checked as linearizable;
  it accepts the same histories at `prefix`, where only the consistent-prefix check applies: a job,
  and `--fails-without` jobs with each check disabled in turn
- A4 The proxy's bandwidth cap: a stream through a proxy capped at 100 Mb/s measures within 10% of
  the cap (a job), which shard-groups A5 relies on
- A5 [artifact] `harness/cluster/README.md`: how to run each tier, the fault vocabulary, the port
  map, and the history format the checker reads

## Contracts

Provides `xapiand:cluster-harness`: `harness/cluster/run.sh` (start N, fault, collect), the
history format (one JSON line per invocation and completion: client, op, key, value, level,
outcome), the checker's CLI, and `tools/vm/cluster.sh` (up, run, collect, down; parks every VM on
every exit). The `cluster` suite entry in `harness/run_all.sh` is added by journal-replaces-wal,
which owns `run_all.sh` (F10); until then `harness/cluster/run.sh` is called directly.

## Decisions

- Lead: the checker is written here, in Python under `harness/cluster/`, not borrowed from
  Detent's tree; Detent's `network.py` is the shape, not a dependency.
- Lead: VM work is one command per batch (up, run, collect, down), never a VM left up across
  agent turns.

## May land early

None. The VM tier (old A3) is quest cluster-vm-tier.
