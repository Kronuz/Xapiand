# cluster-harness: journal

State: waiting on jobs 261116-120747-f043a1 261116-120748-f8c92d 261116-120749-48d2e7 261116-120804-c631b7 261116-120804-7ab3fb
Quest: .campaign/xapiand/quests/cluster-harness.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The correctness review's findings (CHANGES at Xapiand:45d76f523) are fixed at Xapiand:df3596550;
the evidence jobs rerun there.

## Next

1. Read A2 (261116-120747-f043a1), A1 (261116-120748-f8c92d), A4 (261116-120749-48d2e7),
   build-test (261116-120804-c631b7) and e2e (261116-120804-7ab3fb) at Xapiand:df3596550; fill the
   Evidence table from `campaign brief`; then ready for the reviewer's confirm.
2. A3 (tier 2, `tools/vm/cluster.sh`), in a follow-up session: copy a verified noble cloud image
   (strata's `tools/vm/linux.sh` checksum; read, never touch its VM dir), three qcow2 overlays,
   cloud-init seeds, `-netdev socket,mcast=230.0.0.1:PORT` as the guests' L2 plus a user netdev
   for SSH on 18780-18782, a multicast probe from each guest, and a `dm-log-writes` device under a
   data directory with a small replay tool (the log format is a superblock and entries; write our
   own rather than build xfstests in the guest). One command per batch, parked on every exit.

## Blocked on

Nothing.

## Evidence

At Xapiand:df3596550 (the review fixes):

| Criterion | Evidence |
|---|---|
| A1 | 261116-120748-f8c92d (queued; reruns 261116-114308-647626, which passed at Xapiand:d853aafea) |
| A2 | 261116-120747-f043a1 (queued; reruns 261116-113744-61bbf2). Fails without: the three check mutants 261116-120747-b1209b (written), 261116-120747-d81b09 (prefix), 261116-120748-80201d (linearizable), each passed; F1's mutant 261116-120801-b68506 passed (45d76f523's history.py fails the divergent-logs and lagging-replica cases) |
| A3 | not started; left for a follow-up |
| A4 | 261116-120749-48d2e7 (queued, strict; reruns 261116-114308-6b17b6, met at Xapiand:d853aafea). The partition test 261116-120754-9af804 passed, and F2's mutant 261116-120758-c77697 passed (45d76f523's proxy.py leaks 4 of 7 events) |
| A5 | `harness/cluster/README.md` |

Review findings (`.campaign/xapiand/reviews/cluster-harness-correctness.md`):

| Finding | Fix | Confirm |
|---|---|---|
| F1 | `prefix` is D10's consistent prefix: one write order per key, monotonic per (node, launch), never behind a `quorum` write the node acknowledged; history lines carry `node` and `launch` | F1=261116-120746-d9ce7d (rerun of 261116-115748-a53257): 7 of 8 as designed. Its case 1 still differs because its lines name no `node`, and the check reads a read without a node as from any replica (ruling below). selftest plants the same case with nodes and rejects it (261116-120747-f043a1, and 261116-120801-b68506 without the fix) |
| F2 | A partition holds FIN and resets like bytes; heal delivers them in order (README fault table) | F2=261116-120746-2a5167 (rerun of 261116-115303-016b61): exit 0, nothing crosses. Also 261116-120754-9af804 and mutant 261116-120758-c77697 |
| F3 | A same-time invocation after a client's completion stays after it; Recorder times strictly increase | The probe's tie case rejects in 261116-120746-d9ce7d; a selftest case |
| F4 | An observed `none` write completes at infinity (brute force too) | The probe's case 7 accepts in 261116-120746-d9ce7d; a selftest case |
| F5 | checklist.py watches its parent and kills the nodes within a second; run.sh's watchdog ends with run.sh | 261116-120746-fe7bf0 (rerun of 261116-115524-c204ef): exit 0 |
| F6 | A node that never answers makes the run exit 2 | By reading: `verdict` |
| F7 | The fault table says a partitioned link's proxy accepts a connect that then reaches nothing | README |
| F8 | Every operation records `node` and `launch` | Part of F1 |
| F9 | README: writes only `prefix` reads observed are outside the linearizable check, per D4 | README |

## Result

`harness/cluster/` (Xapiand:8ba28ec89, review fixes at Xapiand:df3596550):

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
- `history.py`: the history format and checker (written; prefix as D10's consistent prefix, a
  cycle search over write-order edges; linearizable by Gibbons and Korach's zones).
  `selftest.py`: the planted histories and brute-force cross-checks of both (1,878 and 1,725
  random histories, all agreeing). `proxy_test.py`: what a partition holds and heal delivers.
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
- Partition is a silent hold of bytes, FINs, resets and new connections, not a reset; heal
  delivers what was held in order, as TCP retransmission would to connections that outlived the
  partition. `reset()` is the separate RST fault. A link carries the connections its source
  opens, both ways.
- Prefix (lead's ruling on F1): a read whose line names no `node` may come from any replica, so
  only the single write order binds it. The floor after a restart is the `quorum` writes the
  node itself acknowledged (`node` on the write), read as "durable there"; a write acknowledged
  through a forwarding node will need the leader named on it once quest 13 forwards.
- `none` writes order nothing by real time in the prefix check, since they may commit after
  their acknowledgement (D4).
- The A2 mutants disable one check through `check(disabled=...)`, which only the self-test passes;
  the CLI cannot disable a check.
- A3 deferred: tier 2 needs a guest image, cloud-init, the mcast L2, dm-log-writes and a replay
  tool, which do not fit this session's budget after tier 1.
