# cluster-harness: journal

State: waiting on jobs 261116-113803-e3d7df 261116-113756-045bc1 261116-114144-9d35dc 261116-114148-339c7a 261116-114152-beba20 261116-113805-6684ab 261116-113806-d87326
Quest: .campaign/xapiand/quests/cluster-harness.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Tier 1 (A1, A2, A4) and the README (A5) are written at Xapiand:8ba28ec89; their jobs are queued.

## Next

1. Read the A1, A4, mutant and gate-check jobs; fill the Evidence table from `campaign brief`.
2. A3 (tier 2, `tools/vm/cluster.sh`), in a follow-up session: copy a verified noble cloud image
   (strata's `tools/vm/linux.sh` checksum; read, never touch its VM dir), three qcow2 overlays,
   cloud-init seeds, `-netdev socket,mcast=230.0.0.1:PORT` as the guests' L2 plus a user netdev
   for SSH on 18780-18782, a multicast probe from each guest, and a `dm-log-writes` device under a
   data directory with a small replay tool (the log format is a superblock and entries; write our
   own rather than build xfstests in the guest). One command per batch, parked on every exit.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261116-113803-e3d7df (pending): `--build` master's xapiand, then `run.sh check --nodes 1` green, `--nodes 3` and `--nodes 5` with `--expect-fail membership,read,search,aggregations,failover,replay` |
| A2 | 261116-113744-61bbf2 passed: 3 planted histories rejected at linearizable and accepted at prefix; zones agree with brute force on 1,878 random histories. Mutants (`--fails-without`, exit 1 means the test failed): written 261116-114144-9d35dc, prefix 261116-114148-339c7a, linearizable 261116-114152-beba20 (pending). 261116-113747-7b7eb1, -113750-28fadf and -113753-8a9769 ran the same mutants without the exit inversion (each exit 1, the self-test RED); superseded |
| A3 | not started; left for a follow-up |
| A4 | 261116-113756-045bc1 (pending, strict): 48 MiB through a link capped at 100 Mb/s, 1 and 4 streams |
| A5 | `harness/cluster/README.md` |

## Result

`harness/cluster/` (Xapiand:8ba28ec89):

- `run.sh`: one entry (`check`, `history`, `selftest`, `bandwidth`); runs the Python side in its
  own process group and kills the group on exit, on INT/TERM/HUP, and after `CLUSTER_TIMEOUT`.
  Probed by hand: SIGKILL of the Python side, SIGTERM of run.sh and the watchdog each leave no
  node and no `sleep` behind.
- `cluster.py`: `Cluster` (launch, ready, kill9, pause/resume, stop, restart, view) over N nodes
  with OS-assigned HTTP and peer ports; node arguments are templates whose `{peers}` and `{seed}`
  point through the mesh.
- `proxy.py`: a `Link` per directed node pair: partition (a silent hold, not a reset), heal, reset,
  and a bandwidth cap (a pacing bucket per direction shared by the link's connections, 20 ms
  slack). Probe: 24 MiB at 100 Mb/s measured 100.73 (1 stream) and 100.70 (4 streams).
- `history.py`: the history format and checker (written, prefix, linearizable by Gibbons and
  Korach's zones). `selftest.py`: the planted histories and a brute-force cross-check.
- `checklist.py`: the six cluster_check.sh items plus `history`; `--expect-fail` for builds
  without a cluster.

Leaves open: A3 (tier 2), and CI wiring of tier 1: `.github/` and `run_all.sh` are outside this
quest's file set, and run_all.sh's `cluster` entry is journal-replaces-wal's per the contract.

## Rulings I made

- A node whose `GET /` has no `nodes` list is a one-node cluster it leads. On master N = 1 passes
  every item and N > 1 fails `membership` with each node seeing only itself.
- `failover` at N = 1 is kill -9, restart, leads again, serves its write; at N = 2 it is skipped
  (no quorum without the leader), as cluster_check.sh ran it only at N >= 3.
- `replay` keeps cluster_check.sh's WAL-error grep (the WAL stays until 7b, D13); the journal's
  quests extend `WAL_ERRORS`.
- Every write carries a fresh `token` field, the history's value, so retries never repeat a value
  and reads name their write; the checker refuses a key that writes one value twice.
- Outcomes: 2xx ok, 4xx fail, 5xx and no response info; a 404 get-by-id is an ok read of null.
  Reads default to level `prefix` (`--read-level` and `--read-query` change it once quest 13
  defines the API); writes record `quorum`.
- Linearizability is the zone test, not a search: writes are unique and reads name their write, so
  it is exact and polynomial. An unread `none` or `info` write is optional; an `info` write some
  read observed completes at infinity. The self-test cross-checks it with an exhaustive search.
- Partition is a silent hold (bytes and new connections wait), not a reset; `reset()` is the
  separate RST fault. A link carries the connections its source opens, both ways.
- The A2 mutants disable one check through `check(disabled=...)`, which only the self-test passes;
  the CLI cannot disable a check.
- A3 deferred: tier 2 needs a guest image, cloud-init, the mcast L2, dm-log-writes and a replay
  tool, which do not fit this session's budget after tier 1.
