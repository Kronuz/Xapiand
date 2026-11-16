# The cluster harness

Two tiers prove cluster claims with jobs (`docs/design/cluster-release.md` D14). Tier 1 runs N
`xapiand` processes on `127.0.0.1` and runs in CI. Tier 2 boots 3 to 5 Linux VMs under QEMU for
multicast and power loss, and runs under quiet holds.

## Tier 1: processes on one host

```sh
harness/cluster/run.sh check --nodes 3                 # the checklist; exit 0 green, 1 red, 2 harness error
harness/cluster/run.sh check --nodes 1                 # one node: every item applies and passes on master
harness/cluster/run.sh check --nodes 5 --expect-fail membership,read,search,aggregations,failover,replay
harness/cluster/run.sh history build/cluster/run-123/history.jsonl --level linearizable
harness/cluster/run.sh selftest [--mutant written|prefix|linearizable]
harness/cluster/run.sh bandwidth --mbit 100 [--streams 4]
harness/cluster/run.sh proxytest                       # a partitioned link holds every event until heal
```

`check` takes `--bin` (default `build/bin/xapiand`, or `$BIN`), `--out DIR` (default
`build/cluster/run-PID`), `--items` to run a subset in order, `--wait` (seconds each convergence
may take, default 30) and `--timeout` (the whole run, default 1,200 s). `--expect-fail` makes the
exit status 0 only when exactly the named items fail and none raises: that is how a job shows a
cluster that does not exist yet failing each item by name.

The checklist is the Xapian-era `harness/cluster_check.sh` (at `5a517b052^`) with every item
always on:

| Item | Passes when |
|---|---|
| `membership` | every node lists every member, and all name one leader among them |
| `read` | a PUT on node 1 is returned by get-by-id on every node |
| `search` | a search from the last node finds it |
| `aggregations` | `_doc_count`, `_sum` and `_avg` over 40 documents are exact on every node, also with `_check_at_least: 1` |
| `failover` | at N ≥ 3: `kill -9` of the leader, the survivors elect another, a write through one reads back from another, and the killed node rejoins under one leader. At N = 1: the node restarts, leads and serves its write. N = 2 skips |
| `replay` | 40 PUTs on node 1 without a commit, `kill -9`, a restart: every node serves all 40 and the last by id, and no node logs a WAL error |
| `history` | every write and get-by-id above, recorded, passes the checker at its own levels |

A node whose `GET /` has no `nodes` list is a cluster of one that it leads, so on a build without a
cluster, N = 1 passes and N > 1 fails `membership` with each node seeing only itself.

Each run directory holds `nodeI/node.log` (every launch's command line, then the node's output),
`nodeI/data`, `history.jsonl` and `summary.json` (each item's status). A green run deletes the data
directories unless `--keep`.

### Processes

`run.sh` starts the Python side in its own process group, and every node joins it. On exit, on
SIGINT, SIGTERM or SIGHUP, and after `CLUSTER_TIMEOUT` seconds (default 1,800), `run.sh` sends the
group SIGTERM, then SIGCONT and SIGKILL to whatever is left, so a paused node dies too. The
Python side also stops its nodes on its own `--timeout` and on a signal, and kills them within a
second when run.sh itself dies without reaping (SIGKILL). A node that never answers `GET /` makes
the run a harness error (exit 2), not a failed item.

### Faults

Scenarios drive `cluster.Cluster` and its `proxy.Mesh` from Python:

| Fault | Call | What the node sees |
|---|---|---|
| crash | `cluster.kill9(node)` | SIGKILL: no shutdown, the page cache survives (not a power loss) |
| stop | `cluster.stop(node)` | SIGTERM, SIGKILL after 10 s |
| pause | `cluster.pause(node)`, `cluster.resume(node)` | SIGSTOP and SIGCONT: a node that stops answering, then wakes with stale state |
| restart | `cluster.restart(node)`, `cluster.launch(node)` | the same ports, data directory and arguments |
| partition | `mesh.isolate(i)`, `link.partition()`; `mesh.heal(i)`, `link.heal()` | silence: bytes, FINs and resets in flight wait, and a new connection is accepted by the proxy but reaches nothing; healing delivers what was held, in order, as TCP retransmission would to connections that outlived the partition |
| reset | `link.reset()` | every connection on the link aborted, as by a RST |
| bandwidth | `mesh.cap(mbit)`, `link.cap(mbit)` | each direction of a link paced to the cap, shared by its connections like one wire |

Links are directed by who connects: link (i, j) carries the connections node i opens to node j,
both ways, so `link.partition()` silences those and leaves j's connections to i alone, and
`mesh.isolate(i)` silences every link that touches node i. The proxy forwards bytes without reading them, so TLS passes through.
`run.sh bandwidth` measures one capped link and fails when it is more than 10% off the cap.

Nodes reach each other through the mesh only when their arguments name the links. `--node-args`
(every node) and `--join-args` (every node but node 1) are templates over `{index}`, `{name}`,
`{http}`, `{peer}`, `{data}`, `{peers}` (the link addresses this node dials the others by, in node
order, comma-separated) and `{seed}` (its link to node 1). A node that learns its peers' addresses
from the cluster instead of from its arguments bypasses the proxy, and its partitions are not
tested.

### Ports

| Ports | Used by |
|---|---|
| OS-assigned on `127.0.0.1` | each node's HTTP and peer port, and each link's listener (`run.sh` prints them) |
| 18780 and up | tier 2's guests' SSH, one per VM |
| 18762, 18763 | Detent's VMs: never touched |
| 18772, 18774 | strata's VMs: never touched |

## Histories

One JSON line per invocation and one per completion, in the order they happen:

```json
{"client": "put@node1", "op": "write", "key": "cluster_probe/1", "value": "t1", "level": "quorum", "outcome": "invoke", "time": 0.104}
{"client": "put@node1", "op": "write", "key": "cluster_probe/1", "value": "t1", "level": "quorum", "outcome": "ok", "time": 0.311}
{"client": "get@node2", "op": "get", "key": "cluster_probe/1", "value": null, "level": "prefix", "outcome": "invoke", "time": 0.312, "node": "node2", "launch": 1}
{"client": "get@node2", "op": "get", "key": "cluster_probe/1", "value": "t1", "level": "prefix", "outcome": "ok", "time": 0.320, "node": "node2", "launch": 1}
```

- `client`: one sequential client; it completes each operation before it invokes the next.
- `op`: `write`, `get` (get-by-id) or `read` (any other read of the key).
- `key`: the register, `index/id` here. `value`: a JSON scalar. A write's value is unique on its
  key and never null; a read's invocation carries null and its completion the value read, null
  for not found.
- `level`: a write's acknowledgement level (`quorum` or `none`), or a read's level (`prefix` or
  `linearizable`).
- `outcome`: `invoke`, then `ok`, `fail` (it did not happen: a 4xx) or `info` (unknown: a timeout,
  a reset or a 5xx). An invocation without a completion is `info`.
- `time`: seconds on one monotonic clock. A client's invocation at the same time as its previous
  completion is taken as after it.
- `node` and `launch` (optional, read from the invocation): the node that served the read or
  acknowledged the write, and its launch, counted from 1, so a restart starts the next. A read
  without a `node` may come from any replica. Other fields are ignored.

The checker (`history.py`) runs three checks on each key:

1. `written`: every read returns null or the value of a write that had begun and was not refused.
2. `prefix`: consistent prefix as cluster-release D4 and durability-modes D10 define it, any
   replica's snapshot of one log applied in order. One order of the key's writes must explain
   every read as the prefix ending at its value (null: the empty prefix), where an acknowledged
   `quorum` write that completed before another began comes first, each node's launch reads
   monotonically in real time, and a node never reads behind a `quorum` write it acknowledged. A
   restart may go back, never past that. The rules are edges between writes, so the check looks
   for a cycle; `selftest.py` compares it with every order on 3,000 random histories.
3. `linearizable`: the `quorum` writes acknowledged, the writes some read observed, and the reads
   at level `linearizable` linearize. It is Gibbons and Korach's zone test for registers with
   unique writes: exact, and polynomial where a search for a linearization is exponential;
   `selftest.py` checks it against an exhaustive search on 3,000 random histories. An unread
   `none` or `info` write may be lost; an observed one may commit any time after it began, since
   `none` acknowledges before commit. A write that only `prefix` reads observed is not in this
   check, as D4 scopes linearizability to `quorum` writes and `linearizable` reads.

`--level` checks every read at that level instead of its own. The self-test plants a lost
acknowledged write, a stale read and a stale get-by-id from a partitioned old leader, which fail
at `linearizable` and pass at `prefix`, and `--mutant CHECK` shows each check is load-bearing.

## Tier 2: VMs

`tools/vm/cluster.sh` (up, run, collect, down) boots 3 to 5 Linux guests under QEMU on one
`-netdev socket,mcast=...` L2, with SSH on 18780 and up and a `dm-log-writes` device under each
data directory for power loss. It parks every VM on every exit and runs under quiet holds. It is
not built yet (quest `cluster-harness` A3).
