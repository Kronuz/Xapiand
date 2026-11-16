# Xapiand in a cluster: the minimal path for the first Xapian-free release

The first Xapian-free release runs in a cluster again: an index spreads its shards across nodes,
each shard keeps replicas, a lost node fails over without losing acknowledged writes, and search
spans the cluster. All of it runs on strata's replication groups, before the format freeze.

- Status: design, revision 0, 2026-11-15, ready for review (lens `design`). Request: owner ruling
  D91 (2026-11-16, `.campaign/xapiand/PLAN.md`), which supersedes D9's single-node release.
- Read at: Xapiand `master` `a9cfa2cba`; the Xapian-era cluster at `5a517b052^`, the parent of
  "Delete the Xapian engine and the cluster layer" (v1.0.0-alpha.9 runs the same cluster code);
  strata `main` `9bcb142e`; consensus `main` `a95847e`; journal `main` `8f3e410`; Detent `main`
  `13a2ae42`; prospect `main`. Clean room: nothing here was read from Xapian's sources,
  `src/sortable_serialise.*`, the GPL Python module, `src/server/remote_protocol_views.*` beyond
  its license header, or strata-oracle.
- Binding inputs: D13 (the WAL stays until 7b), D21 (the coroutine drivers), D54 (the remote-leader
  hop and `?commit`), D83 (consensus late appends), D87 (pre-release bytes change freely), the
  trusted-cluster threat model (durability-modes Q9), one replication mechanism, nested index paths,
  and Detent D97: the owner's answers to async-drivers Q2 (a Sink), Q3 (one journal directory per
  host, artifacts by opaque group id), Q4 (quiescence is its own consensus quest) and Q7 (one
  certificate per node).
- Nothing here is built or measured. Every number is cited from the document or job that holds it.

## Abstract

The Xapian-era cluster gave users multicast discovery, `number_of_shards` and
`number_of_replicas`, writes accepted at any node, asynchronous pull replication, and distributed
search with exact aggregations. Its failover barely worked. A dead primary waited 300 s before a
vote, and a shard with one replica could never be promoted, because its vote needed 2 of 2. A
write acknowledged before replicas pulled it was lost when the primary died, with a window of up
to about 13 s under sustained writes.

The recommendation is the smallest build that reaches parity with that cluster and fixes its
correctness:

1. Every shard is one strata replication group, at every replica count, `rf = 1` included. Each
   voter applies every committed batch (document mode), and the journal replaces the WAL.
2. One consensus "root" group holds the node directory and the shard map.
3. Writes go to the group leader, with at most one forward.
4. Search fans out over a node-to-node request lane on the transport consensus already uses, with
   exact cross-node statistics.
5. Acknowledgement levels are `quorum` and `none`; read levels are `prefix` and `leader`.

Segment mode, the `memory` acknowledgement, gossip, placement groups, the fan-out tree and rolling
upgrades follow the release.

That build is 17 quests of roughly 300 to 1,500 lines each, about 17,000 lines with tests. They are
built on Detent's three remaining driver quests, journal-driver, consensus-driver and
consensus-transport, which start once task-wheel and completion-seam land. The critical path runs
through those drivers, then `group-core`, `group-install`, `shard-groups`, `distributed-search`,
`cluster-gate`, and finally `format-freeze`. Six questions need the owner (section 9).

## 1. What the Xapian-era cluster did

All citations in this section are at `5a517b052^`.

| Area | Behavior | Where |
|---|---|---|
| Discovery | UDP multicast through Kronuz/cluster's `cluster::Bus`, group `239.192.168.1`, port 58880, cluster name `Xapiand`. Messages: `CLUSTER_HELLO`, `WAVE`, `SNEER`, `ENTER` and `BYE`; Raft heartbeat, append and vote; `DB_UPDATED`, `SCHEMA_UPDATED`, `INDEX_SETTINGS_UPDATED`, `PRIMARY_UPDATED` and `ELECT_PRIMARY`. A node gets a generated or `--name` name, kept in `.xapiand/node` | `config.h.in:281-283`; `discovery.h:74-94`; `discovery.cc:328-424`, `831-885`; `manager.cc:289-339` |
| Liveness | 1 s heartbeat; a node is alive if heard from within 10 s. Quorum counts every known node, dead ones included | `node.h:47-48`, `209`, `376-386` |
| Raft | One cluster-wide, volatile UDP Raft. Its log holds node names only: it elects a leader and adds nodes. The leader holds the primary of every `.xapiand/*` database | `manager.cc:1606-1714`; `node.cc:256-275`; `index_resolver_lru.cc:543-577` |
| Settings | `--shards 5` and `--replicas 1` by default. `_settings.number_of_shards` and `number_of_replicas` are each 1 to 9,999 (replicas may be 0); the shard count is fixed once an index has data; strict mode requires both | `opts.cc:44-45`, `230-233`; `index_resolver_lru.cc:643-686` |
| Placement | Shards are `<index>/.__N`. The primary is `jump_consistent_hash(index_path, nodes)`, offset by the shard number, over nodes sorted by name; replicas take the next nodes, capped at the nodes known. The record in `.xapiand/indexes` lists the nodes, primary first. Nothing rebalances; the docs mark it unimplemented | `index_resolver_lru.cc:104-176`, `244-342`, `712`; `clustering.md:119-122` |
| Failover | After `--database-stall-time` (300 s), the Raft leader polls the shard's holders and picks the highest revision. Defects: 2 of 2 is unreachable with one replica; repeat votes are ignored; a holder without a copy never answers; the vote travels over forgeable UDP | `index_resolver_lru.cc:180-240`; `discovery.cc:669-815`; `docs/status/failover.md:23-31` on `fix/failover` |
| Replication | Asynchronous pull. A commit multicasts `DB_UPDATED`, and each replica pulls 0 to 3 s later: WAL changesets, or a full copy of the database files when the UUID or revision is missing. The write is acknowledged first | `shard.cc:990-1006`; `discovery.cc:534-541`; `replication_protocol_views.cc:466-657`; `ARCHITECTURE.md:339-343`, `533-537` |
| Remote | Xapian's remote protocol v47 on TCP 9880, replication on 7880, HTTP on 8880. No TLS or authentication anywhere between nodes | `config.h.in:284-286`; grep of the three services |
| Search | One endpoint per shard, local or remote, under one `engine::search`. A stats phase runs before the match. Aggregations scan every match (`CHECK_ALL`) and merge with `merge_results` (Xapiand:7efc3753e) | `handler.cc:1079-1083`, `1136-1178`; `docs/dev/cluster-agg.md` |
| Writes | Accepted at any node, sent to the primary through a writable remote database, 503 when the primary is down. Shard = `fnv1ah64(prefix + id) % shards`. `?commit` commits; otherwise a 1 s debounce. `?volatile` reads from the primary | `shard.cc:412-439`; `database/utils.cc:105-111`; `search_views.cc:2345-2368` |
| API | `GET /` shows `cluster_name` and `nodes[]`; `GET /.nodes`; `:info` lists the shards. Options: `--cluster`, `--name`, `--discovery-*`, `--replicas`, `--shards`, `--primary-node`, `--xapian-port`, `--replica-port`, `--database-stall-time`, `--solo` | `search_views.cc:715-760`, `1253-1370`; `opts.cc:213-301` |

The parity checklist is `harness/cluster_check.sh`, lines 144 to 321:

1. Every node sees every member, and one leader leads.
2. A write on node 1 is read on every node, polled.
3. A search from any node finds it.
4. Exact `_doc_count`, `_sum` and `_avg` over 40 documents on every node.
5. Kill the leader: a new one leads, a write through a survivor is read on another, and the
   killed node rejoins.
6. `kill -9` with uncommitted writes, then a restart: every node serves all 40 documents.

What a client could lose is computed from the debouncers in strata
`docs/design/durability-modes.md`, "What exists today", and was not measured. Under sustained
writes, a primary crash followed by failover loses up to about 13 s. A power loss on the primary
loses the unsynced WAL tail, up to 3.5 s plus the writer's queue.

**What remains on master** (`a9cfa2cba`):

- `--shards` (`opts.cc:43`).
- The `.__N` records, with `number_of_replicas` still saved and accepted
  (`index_resolver_lru.cc:177-262`, `666-696`), and the leftover placement code (`settle_replicas`
  `:101`, `calculate_shards` `:151`).
- Ids routed through 1,024 buckets, then `% shards` (`handler.cc:155-169`).
- The WAL (`src/database/wal.cc`, 1,200 lines), which logs the prepared `engine::Document`
  (`wal.cc:972`).
- `?commit` (`search_views.cc:2385`).
- The serialized aggregation partials (`src/aggregations/aggregations.h:72-74`).
- The batched multi-shard search seam (`src/engine/search.h:145-154`).

## 2. What exists to build on

| Piece | State | Evidence |
|---|---|---|
| consensus `Core` | Built, on main. An I/O-free Raft core with pre-vote, CheckQuorum, leadership transfer and quorum-probe reads (no leases). Single-step membership changes; voter, learner and witness roles with incarnations. C3 repair at or below the commit cursor. Checkpoint bundles that bind journal sequence to applied index; `RFTSNP02` snapshots over a credited `SnapshotChannel` | consensus `ARCHITECTURE.md:9-193`; `include/consensus/types.h:73`, `206-213` |
| consensus transport | Built, one `Peers` per group: TLS over TCP with control and bulk lanes, the `DTHL0001` hello, `DTC2` frames, byte credit, and the `DTFT0001` fetch lane (name to bytes, consensus:6c3018e). Pins are leaf-certificate SHA-256 | `transport/peers.h:16-21`; `transport/tls.h:12-18` |
| journal | Built. A per-group journal with 4 barriers per append, and `RFTW0001`, one turn log per core with one `sync_data` per turn across its groups | journal `ARCHITECTURE.md:19-25`, `173-206` |
| Late appends (D83) | On Detent's branch `quest/detent/consensus-late-append`, in review | Detent D92 |
| Coroutine drivers | Designed (Detent `docs/design/async-drivers.md`, "Designed, not built"). Task-wheel and completion-seam are active; journal-driver, consensus-driver and consensus-transport (D16 multiplexing, one connection per node pair) are planned; quiescence is its own quest after consensus-driver (D97) | async-drivers build plan, quests 1-7 |
| `strata::group` | Not written, on any branch. strata pins no consensus | strata `CMakeLists.txt:26-29` |
| Durability design | Parked at revision 10, approved with conditions; nothing built | strata `docs/design/durability-modes.md` on main |
| Simulator | A prototype on `design/simulator` (`sim/`: engine, network, history, checker), unmerged and 961 commits behind main | strata `docs/design/simulator.md` |
| Placement, gossip, peer-transport v2 | Designed and parked | strata `DISTRIBUTION.md`, `placement-strategies.md`, `peer-transport.md` |
| Multi-process harness | Detent's `tests/detent_system/`: kill the leader, partition and outage through a TLS-transparent TCP proxy (`network.py`). consensus's `peers_test.cc` runs three TLS `Peers` on loopback | Detent `tests/detent_system/README.md` |
| VMs | strata's single-VM gates: Linux on SSH port 18772, FreeBSD on 18774, both with user-mode networking, so the VMs cannot reach each other. Detent's VMs on 18762 and 18763 stay untouched | strata `tools/vm/linux.sh:9`, `48-51` |

This design changes the quests `phase3r`, `track-c`, `track-d`, `phase7b` and `format-freeze`, and
it cancels `drop-replicas` (section 5). It extends strata `docs/REPLICATION.md`'s Xapiand mapping
and answers durability-modes Q11 to Q14 with recommendations (section 9).

## 3. Decisions

**D1. Scope.** The release builds what section 1's table did for users, plus two correctness
properties: no `quorum`-acknowledged write is ever lost within the fault model, and failover never
produces two leaders. Section 4 lists everything else, with where it lands. Rejected: building
strata's whole Track D first, because its gates need 30 and 300 nodes that do not exist (D35).

**D2. One group per shard, every index, every `rf`.** A one-node index is a lone voter: no
transport and no hop, which also answers Q11's "a single-node index stays local". The journal
replaces the WAL everywhere (D13 ends), so `src/database/wal.*` and `src/xapiand_fsyncher.h` go.
`commit()` becomes the group's internal checkpoint, which answers Q13: every index has a journal,
so there is one way to make a write durable. Rejected: keeping the WAL at `rf = 1`, which means
two durability mechanisms and two recovery paths.

**D3. Document mode, with prepared commands.** The leader resolves ids, timestamps, Lua effects and
the schema version, then proposes one command:

```
op, id, expected version, schema version, engine::Document::serialise() bytes (wal.cc:972), blobs
```

Every voter applies it in log order: the strata write, then the blobs pushed into its own
`DataStorage`. The push moves from prepare (`shard.cc:754`) to apply, so blob locators are a pure
function of the log and replicas stay digest-equal. Followers skip schema resolution and term
generation, as the old WAL replay did. Rejected for the release: segment mode (durability-modes
D8, D17, D18). It costs a three-voter group an estimated 0.38× to 0.43× of document mode's CPU, but
it needs the file-set mover and the sampled rebuild, and it can arrive later per index without
changing the command.

**D4. Levels.**

- `ack`: `quorum` (the default, Q1) or `none` (acknowledged at `ProposalPlaced`).
- `read`: `prefix` (any replica's last refreshed snapshot, the default) or `leader`.
- `refresh`: `false` (periodic, 1 s), `wait_for` or `true`.
- `?commit` means `ack=quorum&refresh=true` (Q12). `POST /idx/:flush` waits for a checkpoint that
  covers the write: segments synced, Elasticsearch's flush.

`all`, `leader`, `memory`, `session`, `bounded` and `linearizable` follow the release. Each one adds
a driver event or the forgetful-voter machinery, and parity needs none of them.

**D5. One root authority.** A strict-consensus group, `strata::placement`, with 3 or 5 voters
chosen from the first nodes, holds three kinds of record:

- Node directory: id, name, addresses, certificate pin, attributes, state.
- Namespaces: index path to shard count, `rf` and settings version.
- Shard map: shard to opaque group id, incarnation, members and a leader hint.

Records are keyed by index path, so placement groups can later own ranges of it without a format
change. Nested paths (`twitter`, `twitter/user`) are distinct keys, and journal artifacts use
opaque group ids (D97 Q3), so neither can collide. It replaces `.xapiand/indexes`, the UDP Raft
and `ELECT_PRIMARY`.

**D6. Membership.** Nodes find the cluster from `--join` seeds (host names resolved by consensus's
DNS dialing) and present their node certificate (D97 Q7). Liveness is the authority leader's
volatile keepalive table, with the same 500 ms × 3 misses as track-c D12. Only state transitions
(`up`, `suspect`, `down`) are logged. Multicast is dropped, so the cluster works on Kubernetes
without a multicast network plugin (old `kubernetes.md:19-24`). SWIM gossip follows the release;
the keepalive table is linear in nodes at the authority leader (section 10).

**D7. Placement and repair.**

- Each shard takes `rf` distinct nodes with the fewest groups, and leaders are spread by
  `transfer_leadership`.
- When `rf` exceeds the nodes, the shard runs short and health reports yellow. The old cluster
  capped replicas the same way.
- A node `down` longer than `--replace-after` (default 300 s, the old stall time) is replaced:
  add a learner, install a checkpoint, promote it, remove the dead voter.
- `PUT _settings {number_of_replicas}` and `POST /:nodes/<n>/:drain` use the same steps.

The old cluster never re-replicated, so a second failure was fatal.

**D8. Write routing.** The receiving node reads the leader hint from its shard-map cache and
forwards at most once. A stale hint is answered with a redirect that carries the leader, and a
group with no quorum answers 503. A cancelled or timed-out proposal has an unknown outcome, so the
answer says so, and retries are idempotent by version (async-drivers X11). Rejected: a node-local
"accepted" level (Q11.3). Its writes have no group order and no version check, which D13 rejects.

**D9. One transport between nodes.** Xapiand's shard RPC runs on a request/response lane of the
host transport (consensus-transport, D16). The lane generalizes `DTFT0001` and is the same lane
Detent's history fetch needs (Detent msg-44, option 1). It carries method id, deadline,
cancellation and opaque msgpack bodies, with its own credit beside the group frames. Rejected: a
second TLS listener, or HTTP between nodes, which would mean two identities, two sets of ports and
two flow controls.

**D10. Distributed search.**

- The coordinator, the node that received the request, runs `search_batched` (`search.h:145-154`)
  with one batch per node: local shards in process, remote ones through `search_partial`.
- Statistics are exact. A `stats` round trip first gathers each index's document count, total
  length and per-term frequencies. The weights then read them, so answers equal one node's
  (`search.h:96-99`).
- prospect computes statistics from its snapshot today (`searcher.hh:104-151`), so it gains a
  supplied-statistics input.
- Aggregations merge through `serialise_results` and `merge_results`.
- Each request carries a deadline. A shard that misses it, after one retry on another replica, is
  reported in `_shards.failed` and the result is marked partial.
- Fan-out trees, cached statistics, threshold sharing and hedging follow the release.

**D11. Idle groups close.** Groups follow Xapiand's catalog: closed when idle, reopened by a frame
or a request, quiescent in between (D97 Q4). Two consequences:

- A node's loss costs elections only in groups that are in use.
- The first request to a closed group whose leader died pays one election, which is lazy
  failover.

**D12. Blobs stay in the node** (track-d D7, for the release). Each voter's `DataStorage` is
written by apply. A command carries a blob of at most 64 MiB, the journal's batch bound (journal
`ARCHITECTURE.md:27-49`). A streamed upload larger than that becomes a sequence of chunk commands
under one upload id, committed by its last chunk.

**D13. Migration.** No release has written strata data (D91 supersedes D9). Pre-release data
directories that hold a WAL are not converted; users reindex or restore, as D87 ruled for
pre-release bytes. phase7b's A7 becomes:

- A node with data seeds a cluster: its indexes become lone-voter groups, which raising
  `number_of_replicas` grows by install.
- A shrink, a drain and a restart past log truncation lose nothing.

**D14. Two harness tiers.**

1. Processes on one host: N `xapiand` on `127.0.0.1` with their own ports and data directories,
   faults from `kill -9`, `SIGSTOP` and a TLS-transparent proxy (Detent's `network.py` shape), and
   a history checker for linearizable writes and get-by-id. This tier runs in CI.
2. 3 to 5 Linux VMs under QEMU on a `-netdev socket` or `dgram` L2 between guests, SSH on ports
   18780 and up. Power loss is a `dm-log-writes` device under the data directory, replayed to every
   flush mark, so recovery is checked at each state a real power loss can leave.

Killing QEMU loses the guest's page cache but keeps writes it already issued, so it is not a power
loss; `dm-log-writes` is. VM work runs under quiet holds.

**D15. Ids and shards.** The 1,024-bucket id hash stays (`handler.cc:155-169`). The shard count is
fixed at create, as before. Resharding by split follows the release (track-d D4).

## 4. What stays after the release

| Item | Source | Why it can wait | Later cost |
|---|---|---|---|
| Segment mode, `verify: replica` | durability-modes D8, D17, D18 | Document mode is correct; the gain is CPU | Per index, no command change |
| `memory` ack, forgetful voters, recovery election | durability-modes D2, D3, D5, D6, D7 | `quorum` and `none` cover parity | Journal `RFTW0002`; a journal drains at a clean upgrade, so a version bump needs no long-lived reader (Q2) |
| `all`, `leader`, `session`, `bounded`, `linearizable` | durability-modes D10 | Not in the old cluster | One driver event each |
| SWIM gossip | track-d D2 | The keepalive table serves hundreds of nodes | Replaces the liveness source only |
| Placement groups, rack and zone awareness, strategies | track-d D3, `placement-strategies.md` | One root at 3 to 5 nodes | Ranges keyed by path (D5) |
| Aliases, resharding by split | track-d D4 | Not in the old cluster | |
| Fan-out trees, cached statistics, P10, hedging | track-d D5 | Flat fan-out at 3 to 5 nodes | |
| Storage tiers on object storage | track-d D6 | | |
| Mixed-version rolling upgrades | track-d D8 | The hello carries a version range now | The next release reads N − 1 |
| 100,000-node simulator; 30- and 300-node gates | track-d D1; phase7b A4, A5 | No hardware (D35) | |
| Bare-metal rows: quorum ingest against Elasticsearch, fsyncs at 1,000 groups | phase3r A5, A6; track-c A3 | No hardware (D35) | |
| Partition-tolerant writes | durability-modes D12 | | |

## 5. Quests

Sizes are lines of code plus tests. "After" means it must land first. Detent's ids are its own:
`detent:drivers-task-wheel` and `detent:drivers-completion-seam` exist, and journal-driver,
consensus-driver and consensus-transport are its build-plan quests 3 to 5, written below as
`detent:journal-driver`, `detent:consensus-driver` and `detent:consensus-transport` until Detent
names them.

| # | Id | Title | Risk | Repo | Files | Needs / after | Size |
|---|---|---|---|---|---|---|---|
| 1 | `cluster-harness` | Multi-process and VM cluster harness | high | Xapiand | `harness/cluster/**`, `harness/run_all.sh`, `tools/vm/cluster.sh` | none | 1,200 |
| 2 | `prospect-supplied-stats` | Weights from supplied statistics | normal | prospect | `include/prospect/searcher.hh`, `src/search*`, `test/stats*` | none | 300 |
| 3 | `consensus-quiescence` | Quiescent and closed groups (D97 Q4) | high | consensus | `include/consensus/core.h`, `driver.h`, `test/quiesc*` | after `detent:consensus-driver` | 1,000 |
| 4 | `consensus-rpc-lane` | A request lane on the host transport | high | consensus | `include/consensus/transport/**`, `test/transport/rpc*` | after `detent:consensus-transport` | 800 |
| 5 | `group-core` | `strata::group`: propose, apply, checkpoint, recover | critical | strata | `include/strata/group/**`, `src/group/**`, `test/group/**`, `CMakeLists.txt` | after `detent:journal-driver`, `detent:consensus-driver`; lands after 3 | 1,500 |
| 6 | `group-install` | Learners, install, catch-up, membership, repair, scrubber | critical | strata | as 5 | needs 5 | 1,300 |
| 7 | `group-sim` | Phase 3R gate rows in the simulator | high | strata | `sim/**`, `test/group_sim*`, `docs/gates/phase-3r.md` | starts now (port `design/simulator`); lands after 6 | 1,500 |
| 8 | `placement-root` | The root authority: nodes, namespaces, shard map | high | strata | `include/strata/placement/**`, `src/placement/**`, `test/placement/**` | after `detent:consensus-driver` | 900 |
| 9 | `placement-repair` | Re-replication, drain, leader balance, `rf` change | high | strata | as 8 | needs 6, 8 | 800 |
| 10 | `journal-replaces-wal` | Every shard a group at `rf = 1`; the WAL deleted | critical | Xapiand | `src/database/{shard,wal,handler}.*`, `src/xapiand_fsyncher.h`, `src/server/search_views.cc`, `harness/power_loss_check.sh` | needs 5 | 1,500 |
| 11 | `shard-rpc` | The node-to-node shard service | high | Xapiand | `src/cluster/rpc*`, `tests/rpc*` | needs 4 | 1,200 |
| 12 | `cluster-membership` | Node identity, `--join`, certificates, `/:nodes` | high | Xapiand | `src/cluster/{node,membership}*`, `src/opts.*`, `src/manager.*` | needs 8 | 900 |
| 13 | `shard-groups` | Shards across nodes: routing, members, replicas | critical | Xapiand | `src/index_resolver_lru.*`, `src/database/{pool,shard,handler}.*`, `src/cluster/**` | needs 6, 10, 11, 12 | 1,500 |
| 14 | `distributed-search` | Fan-out, exact statistics, merged aggregations | high | Xapiand | `src/engine/{search,strata/search}.*`, `src/database/handler.cc`, `src/server/search_views.cc` | needs 2, 13 | 1,300 |
| 15 | `cluster-api` | Settings, health, `:info`, docs, changelog | normal | Xapiand | `src/server/search_views.cc`, `src/index_resolver_lru.cc`, `docs/src/content/docs/**` | needs 13 | 800 |
| 16 | `cluster-migration` | One node to a cluster and back, without loss | high | Xapiand | `harness/cluster/migration*`, `docs/src/content/docs/upgrade.md` | needs 9, 13 | 500 |
| 17 | `cluster-gate` | The release's cluster gate | critical | Xapiand | `docs/gates/cluster-release.md`, `harness/cluster/gate*` | needs 1, 7, 14, 15, 16 | 400 |

**Contracts** (provided, consumed):

- 3 provides Core's `Wake`, `LeaderGone`, the quiescent follower and the closed state (hard-state
  bytes).
- 4 provides the lane's frames: method, call id, deadline, credit (wire).
- 5 provides the `strata::group` C++ API and the checkpoint bundle that names a manifest (applied
  position, term, CRC32C; REPLICATION.md G6), and consumes the drivers' `Driver<Sink>` (D97 Q2).
- 8 provides the authority's records and checkpoint (bytes).
- 10 provides Xapiand's command (D3, bytes) and the HTTP `ack`, `refresh` and `?commit` semantics.
- 11 provides the RPC methods `forward_write`, `stats`, `search_partial`, `get`, `count` and `info`
  (msgpack, versioned through the hello's range).
- 15 provides `number_of_replicas` back, `GET /:cluster/health`, and `GET /` `cluster_name` and
  `nodes`.

**Acceptance criteria and evidence:**

- **1.** A1: the six `cluster_check.sh` items (section 1), rewritten for N = 3 and 5 processes,
  run as a job; on master they pass at N = 1 and the cluster items fail as expected. A2: the
  history checker rejects a planted lost write and a planted stale read (`--fails-without`). A3:
  three VMs boot on one L2 and a `dm-log-writes` replay of a planted unsynced write finds it
  missing at the earlier flush mark (a job under an exclusive hold). A4 [artifact]
  `harness/cluster/README.md`.
- **2.** A1: weights and orders with supplied statistics equal those from the shards' own merged
  statistics on the release-rows corpus (a job). A2: a mutant that ignores the input fails A1.
- **3.** A1: an idle three-voter group sends no heartbeat frames after it quiesces, counted over
  10^6 simulated steps (a job). A2: a frame or a proposal reopens it, and with a live leader the
  first proposal commits with no election (simulation). A3: a leader lost while quiescent is
  detected through the node keepalive, and no term has two leaders (`simulation_test` with
  quiescence, mutants killed). A4: TSan and ASan.
- **4.** A1: calls with deadlines and cancellation over shared connections; a full group credit
  does not delay an RPC, and a full RPC credit does not delay a heartbeat (`reader_test` shape,
  control failing). A2: one CPU-hour of fuzzing on the frame decoder, zero findings. A3: Detent's
  history fetch expressible as one method.
- **5.** A1: a lone voter's create, propose, apply, checkpoint and recover through
  `HeldCompletion` at every crash point; recovery replays from the manifest's applied position and
  the logical-content digest equals the uncrashed run (a job). A2: three voters in process: every
  `quorum`-acknowledged entry survives any one crash, and digests are equal after every schedule
  (a job). A3: `ack=none` completes at `ProposalPlaced` and `quorum` at commit (a test). A4: an
  idle open group costs nothing beyond the driver's row, and a closed one zero descriptors (a
  measured job). A5: ASan, UBSan and TSan.
- **6.** A1: a learner installs a 64 KiB-chunked segment pack from a pinned manifest, catches up
  and is promoted; replacement and removal pass across leader changes (jobs). A2: a damaged
  segment fences the replica, which reinstalls at or below commit (C3) and serves no corrupt byte
  (a test per file class). A3: the scrubber finds planted damage within one pass, inside its
  budget (a job).
- **7.** phase3r A2 to A4 on this scope: at least 10^7 steps over at least 1,000 seeds for 3 and 5
  voters, with crashes, power loss at every barrier, partitions, clock skew, add, remove and
  replace, and corruption of every file class. The linearizability checker passes over write and
  get-by-id histories. Zero `quorum`-acknowledged loss, zero served corruption, and a rebuild after
  disk loss (simulator jobs; mutants killed).
- **8.** A1: create, read and delete of nodes, namespaces and shard records, with nested paths
  distinct (tests). A2: placement puts `rf` distinct nodes per shard and spreads leaders within one
  per node (a property test). A3: the authority survives the loss of any minority of its voters
  (simulation).
- **9.** A1: a node down past `--replace-after` is replaced with no acknowledged loss (a simulator
  and harness job). A2: a drain empties a node. A3: `rf` 1 to 3 to 1 on a live index (a harness
  job).
- **10.** A1: E2E and the docs suite green with every shard a group. A2: `harness/power_loss_check.sh`
  replaces `wal_recovery_check.sh`: `dm-log-writes` replay at every flush, with no acknowledged
  `quorum` write missing (a job). A3: T11, the journal costing at most half of the WAL per
  document in CPU and on the critical path (`profile_bulk.sh`, exclusive jobs, the lead's). A4:
  Q14, the journal at `none` at least as fast per acknowledged write as the WAL's async default,
  and at `quorum` as its synchronous WAL (exclusive jobs). A5: `git grep -n DatabaseWAL -- src` is
  empty (a job).
- **11.** A1: each method round-trips, with the error mapping, deadlines and cancellation tested.
  A2: one CPU-hour of fuzzing per decoder. A3: an N − 1 peer is refused with a named error.
- **12.** A1: three nodes form from one seed. A node from another cluster, or without a pinned
  certificate, is refused (tests and a harness job). A2: `GET /` and `/:nodes` match the authority.
  A3: plaintext only behind an explicit flag on loopback (Q5).
- **13.** A1: section 1's checklist passes on 3 and 5 processes. A2: a write to any node is
  forwarded at most once (counted in a job). A3: a 503 under no quorum, and a redirect on a stale
  hint (tests). A4: a cancelled proposal's retry is applied once (a test).
- **14.** A1: search, count and every aggregation kind equal over one node and over three, five
  and two-shards-per-node placements (agg-shard-parity's test, cross-node; a job). A2: a dropped
  partial fails A1 (`--fails-without`). A3: a killed replica mid-search is retried, or the result
  is marked partial within the deadline.
- **15.** A1: the API's routes and settings documented, with the docs E2E green. A2: GET
  `_settings` shows `number_of_replicas`; health is green, yellow or red by replica count.
  A3 [artifact]: the changelog.
- **16.** A1: a node with indexes seeds three nodes, `number_of_replicas` 0 to 2, digests equal, no
  document lost (a job). A2: a shrink and a drain lose nothing. A3: a node restarted after its log
  was truncated catches up by install.
- **17.** Section 7's rows, each with its commit and jobs, reviewed (lens `system`).

**Changes to existing quests** (the lead's to make):

- `phase3r`: A1 to A4 and A7 become quests 5 to 7; A5, A6 and A8 follow the release.
- `track-c`: A2 (T2 oracles over the shared turn log), A4 (the C3 gate report) and A8 (goal 5's
  audit) stay before the release. A5 and A7 become `detent:consensus-transport` under D97 Q7. A6
  is quest 7's checker. A3 follows the release.
- `track-d`: D4's minimal mapping (D5, D15) and D7's decision (D12) are here; the rest follows the
  release.
- `phase7b`: A1, A2, A3, A6, A7 (as D13) and A8 are quests 10 to 17; A4 runs at three nodes only
  (Q4); A5 follows the release.
- `format-freeze`: after quests 3, 4, 5, 8, 10 and 11, beside `sortable-encoding`.
- `drop-replicas` (planned, launchable): cancel it. It removes the setting this design brings back.

## 6. Order

```
task-wheel, completion-seam ─> journal-driver ─> consensus-driver ─┬─> group-core ─> group-install ─┐
  (detent)                      (detent)          (detent)         ├─> quiescence (lands with 5)     ├─> shard-groups ─> distributed-search ─> cluster-gate ─> format-freeze
                                                                   ├─> placement-root ─> membership ─┤
                                                                   └─> consensus-transport ─> rpc-lane ─> shard-rpc ─┘
```

The critical path is the drivers, then `group-core`, `group-install`, `shard-groups`,
`distributed-search`, `cluster-gate` and `format-freeze`: six of the seventeen.

- **Starts now:** `cluster-harness`, `prospect-supplied-stats` and `group-sim`'s port of the
  simulator.
- **Once the drivers land:** `group-core`, `placement-root`, `consensus-quiescence` and
  `consensus-rpc-lane` run side by side.
- **Then:** `journal-replaces-wal` beside `group-install`, and `shard-rpc` beside both.
- **Late:** `cluster-api` and `cluster-migration` go late but beside `distributed-search`.

Where it meets the two format quests:

- **`format-freeze`** reviews every new byte: `RFTW0001` with the per-host layout, the hard state,
  the checkpoint bundle, Xapiand's command, the authority's records and the two wire formats.
- **`sortable-encoding`:** the command embeds `engine::Document` bytes, so the value encoding is
  inside it. No code ordering is needed, because both change pre-release bytes freely (D87), but
  the freeze reviews the command only after `sortable-encoding` lands.

## 7. The release's cluster gate

| Row | Target | Procedure |
|---|---|---|
| R1 Parity | Section 1's checklist | On 3 and 5 processes, and on 3 and 5 VMs |
| R2 No acknowledged loss | Zero `quorum` writes lost; write and get-by-id histories linearizable | 200 seeded runs of 60 s per tier, with `kill -9`, `SIGSTOP`, proxy partitions and VM power loss through `dm-log-writes` |
| R3 No split brain | At most one leader per group per term; digests equal after healing | The leaders' logs and a digest per group, in R2's runs |
| R4 Simulation | Quest 7's rows | Quest 7's jobs |
| R5 Failover | Write unavailability after a leader kill, p99, stated against the old 300 s stall plus a 10 to 30 s election | 100 kills on VMs |
| R6 Migration | Quest 16 | Quest 16's jobs |
| R7 The journal's cost | T11 and Q14 | Quest 10's jobs |
| R8 Against the Xapian-era cluster | Ingest, search and aggregation on the same 3 VMs, beating the final Xapian release on every row (phase7b A4 at three nodes) | Exclusive jobs, five interleaved trials |
| R9 Every gate | Sanitizers, one CPU-hour of fuzzing per new decoder, fixtures, coverage | CI and jobs |
| R10 Scale statement | Memory, descriptors and idle CPU per group measured at 10^4 groups per node; the authority's state per node and keepalives per second stated as functions of the node count | A measured job, plus the statement |

## 8. Where this depends on Detent's drivers

| Detent D97 answer | What depends on it here |
|---|---|
| Q2 (a), the Sink | Quest 5 applies `Committed` from the Sink in a chore that leaves the group's loop (Xapiand X10) |
| Q3 (b), one directory per host | D5: groups are opaque ids, so nested paths never collide. The authority's shard record is the only path-to-group map. One lock shared with the cold tier. `format-freeze` reviews this layout |
| Q4, quiescence | Quest 3 is on the release path: D11's closed groups and their bytes come before `format-freeze` |
| Q7 (a), one certificate per node | D6 and quest 12: the node directory holds the pin, and the group-open exchange checks cluster, group and epoch |

Xapiand's requirements beyond the five recorded items (D21; `SHARED_LIBRARY_DECISIONS.md:251-261`):

1. **A request lane on the host transport** (D9, quest 4). The same frame mux and credit carry
   application calls. Detent's history fetch needs it too.
2. **The witness at the driver**, if Q3 is answered (a): a voter that stores the log, never
   applies, and leads only to hand off.
3. **Reopen latency.** A request to a closed group whose leader lives commits with no election,
   and the follower failure detector is the node keepalive shared by every group on the pair
   (track-c D12), not a timer per group.

## 9. Questions for the owner

**Q1. The default acknowledgement** (durability-modes Q11).

- (a) `quorum`: survives any `f` failures. A single sequential client pays a journal barrier and a
  round trip per write; REPLICATION.md measures 62 ms p50 per durable group append on the
  development Mac with 4 barriers, and the shared turn log takes one per turn.
- (b) `none`: the old async latency, and lost on a leader crash.

*Recommendation: (a).* Elasticsearch's default is stricter still: it acknowledges after every
in-sync copy. Bulk loaders choose `ack=none`. Deferring blocks quest 10's defaults and R7's rows.

**Q2. The release's preset.** The owner chose `replicated` as Xapiand's default on 2026-10-19
(durability-modes Q1).

- (a) Ship document mode with `quorum` and `none`, with `replicated` after. A later `RFTW0002` is a
  version bump the clean-upgrade drain absorbs.
- (b) Ship document mode, but land durability-modes step 1's `RFTW0002` bytes now: one quest of
  about 1,000 lines on the journal path.
- (c) Ship `replicated` itself: about six more quests (steps 1, 3, 5, 6 and 8), with forgetful
  voters on the critical path.

*Recommendation: (a).* Deferring the decision blocks quest 5's level set.

**Q3. `number_of_replicas: 1`** (the old default, two copies, two-node clusters).

- (a) `rf = 2 + witness` (REPLICATION.md). consensus has the witness role (`types.h:73`); the group
  needs a log-only voter, about 400 lines in quest 6. With a third node it tolerates one failure,
  and a two-node cluster is stated as tolerating none for writes.
- (b) 400, naming 0, 2 and 4.
- (c) Three copies.

*Recommendation: (a)*, with the default `number_of_replicas` 1. Deferring blocks quest 15.

**Q4. The release's cluster gate** (D91's open question). *Recommendation:* the lead's default,
3 to 5 local VMs, with R8 against the Xapian-era cluster on those VMs. The 30- and 300-node rows,
and the rows against Elasticsearch and Quickwit at scale, follow the release (D35). Deferring
blocks quest 17's plan only.

**Q5. Node certificates** (D97 Q7 (a)).

- (a) Operator-supplied PEM files only.
- (b) Kronuz/pki's offline CA, as Detent uses it, through a documented `pki` command, with
  `--peer-insecure` allowed only on loopback.
- (c) Built-in enrollment tokens, as Elasticsearch 8 has.

*Recommendation: (b).* The old cluster had no node authentication at all. Deferring blocks quest
12's options.

**Q6. D54's remaining answers** (durability-modes Q11.3, Q12 and Q13).

- No node-local "accepted" level (D8).
- `?commit` is `ack=quorum&refresh=true`, and `:flush` is the checkpoint (D4).
- `commit()` becomes internal (D2).

*Recommendation:* as written. Deferring blocks quest 10.

## 10. Limits

- **Two nodes cannot fail over writes** under any Raft design. With Q3 (a), reads continue from the
  survivor.
- **Lazy failover:** the first request to a closed group whose leader died pays an election, about
  the election timeout. It is silent apart from that latency.
- **`ack=none` and `read=prefix` lose and stale silently.** A leader crash loses unreplicated
  `none` writes, and a replica serves its last refresh (1 s by default) after a failover. Both are
  the caller's choice and are documented.
- **Document mode costs CPU:** each of `rf` voters indexes every document, until segment mode.
- **The authority leader is linear in nodes** (keepalives) and holds the whole shard map. That is
  fine at the release's scale; how far it goes is unmeasured, and R10 states it.
- **Blob locators:** replicas stay equal only while `DataStorage`'s volume rotation is a pure
  function of the log. A violation would be silent, so R3's digest covers stored blobs.
- **Power loss** is tested by flush-mark replay on Linux; macOS `F_FULLFSYNC` is not covered by the
  VMs.
- **One more round trip per search** for exact statistics.
- **No mixed-version cluster** in the first release.

## 11. Status, and what to review

Designed, not built. Every quest's evidence is still to be produced. Review, in order:

1. D2 and D3: the WAL removed at `rf = 1`, and blobs written at apply.
2. D5 and D6: one root group with keepalive liveness, against the release's scale.
3. D9's request lane, and the requirements to put to Detent's lead in section 8.
4. The quest boundaries and the critical path in sections 5 and 6.
5. The gate rows R2, R3 and R5, which decide what "works in a cluster" means.
