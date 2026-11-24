# Xapiand in a cluster: the minimal path for the first Xapian-free release

The first Xapian-free release runs in a cluster again: an index spreads its shards across nodes,
each shard keeps replicas, a lost node fails over without losing acknowledged writes, and search
spans the cluster. All of it runs on strata's replication groups, before the format freeze.

- Status: design, revision 1, 2026-11-16, ready for review (lens `design`). It answers the design
  review at Xapiand:5dc60256a (`.campaign/xapiand/reviews/cluster-release-design.md`, CHANGES);
  section 12 maps each finding to its fix. Request: owner ruling D91 (2026-11-16,
  `.campaign/xapiand/PLAN.md`), which supersedes D9's single-node release.
- Revision 2, 2026-11-17: answers group-core's design review at strata:491a1bdc
  (`.campaign/xapiand/reviews/group-core-design.md`, CHANGES) in D3, D4, D11, D12, D16, D18 and
  Q10, with the review's two models extended in `cluster-release/` (section 12); and its re-check
  (Xapiand:edb610078, B7, B8, N10-N15) in D3, D18, Q10 and quest 5's criteria.
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
2. One consensus "root" group holds the node directory and one record per index with its shard
   map, as a strata index.
3. Writes go to the group leader, with at most one forward.
4. Search fans out over a node-to-node request lane on the transport consensus already uses, with
   exact cross-node statistics.
5. Acknowledgement levels are `quorum` and `none`; read levels are `prefix` and `linearizable`.

Segment mode, the `memory` acknowledgement, gossip, placement groups, the fan-out tree and rolling
upgrades follow the release.

That build is 16 quests of roughly 300 to 1,900 lines each, about 18,000 lines with tests. They are
built on Detent's driver quests `detent:journal-driver`, `detent:consensus-driver`,
`detent:consensus-transport` and `detent:consensus-quiescence`, which start once task-wheel and
completion-seam land. The critical path runs through those drivers, then `group-core`,
`group-install`, `shard-groups`, `distributed-search`, `cluster-api`, `cluster-gate`, and finally
`format-freeze`. Ten questions need the owner (section 9).

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
- Index patterns, expanded by walking the local data directory (`search_views.cc:2342-2362`).
- The largest document: a buffered body of 16 MiB (`search_service.h:50`), a non-stored blob of
  1 MiB (`handler.cc:94`), and a streamed stored upload of 4 GiB less 1 KiB, written to a volume of
  its own (`pool.h:160-161`, `shard.cc:1259`).
- RESTORE, which opens its shards with `DB_DISABLE_WAL` (`search_views.cc:1687`,
  `handler.cc:1159`), and `?volatile`, which read the primary (`search_views.cc:1109`).
- `DataStorage` volumes, numbered by a node-local counter (`shard.cc:1100-1130`, `pool.h:236`)
  that recovery sets to the newest volume plus one (`shard.cc:536`).

## 2. What exists to build on

| Piece | State | Evidence |
|---|---|---|
| consensus `Core` | Built, on main. An I/O-free Raft core with pre-vote, CheckQuorum, leadership transfer and quorum-probe reads (no leases). Single-step membership changes; voter, learner and witness roles with incarnations. C3 repair at or below the commit cursor. Checkpoint bundles that bind journal sequence to applied index; `RFTSNP02` snapshots over a credited `SnapshotChannel` | consensus `ARCHITECTURE.md:9-193`; `include/consensus/types.h:73`, `206-213` |
| consensus transport | Built, one `Peers` per group: TLS over TCP with control and bulk lanes, the `DTHL0001` hello, `DTC2` frames, byte credit, and the `DTFT0001` fetch lane (name to bytes, consensus:6c3018e). Pins are leaf-certificate SHA-256 | `transport/peers.h:16-21`; `transport/tls.h:12-18` |
| journal | Built. A per-group journal with 4 barriers per append, and `RFTW0001`, one turn log per core with one `sync_data` per turn across its groups | journal `ARCHITECTURE.md:19-25`, `173-206` |
| Late appends (D83) | On Detent's branch `quest/detent/consensus-late-append`, in review | Detent D92 |
| Coroutine drivers | Designed (Detent `docs/design/async-drivers.md`, "Designed, not built"). Task-wheel and completion-seam are active; journal-driver, consensus-driver and consensus-transport (D16 multiplexing, one connection per node pair) are planned; quiescence is Detent's own quest, `detent:consensus-quiescence`, after consensus-driver and consensus-transport (D97) | async-drivers build plan, quests 1-7; Detent:5e4903e8 |
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

**D1. Scope.** The release builds what section 1's table did for users, plus three correctness
properties: no `quorum`-acknowledged write is ever lost within the fault model; a group has at most
one leader per term (R3); and a `linearizable` read is never served by a deposed leader (D4).
Section 4 lists everything else, with where it lands. Rejected: building
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
op, id, expected version, schema version, engine::Document::serialize() bytes (wal.cc:972), blobs
```

Every voter applies it in log order: first the blobs pushed into its own `DataStorage`, whatever
strata will answer, then the strata write carrying their locators. The push moves from prepare
(`shard.cc:754`) to apply, so blob locators are a pure function of the log and replicas stay
digest-equal. File effects and the application state's update never depend on strata's verdict:
replay skips strata at or below its head (D16), where the verdict is no longer known, and a
refused write whose bytes replay would add shifts every later locator in that volume
(`verdict-gated` and `appstate-gated` in D16's model). Followers skip schema
resolution and term generation, as the old WAL replay did. Rejected for the release: segment
mode (durability-modes D8, D17, D18). It costs a three-voter group an estimated 0.38× to 0.43× of
document mode's CPU, but it ships segments at every refresh and needs the sampled rebuild. The
release builds only the part of D17's file-set mover that install needs (D16), and segment mode
can arrive later per index without changing the command.

Apply's answers (strata's ten verdicts, `index.hh:88`, and a throw) split by whether a retry can
change them:

- Retryable, per replica, position unmoved: `flush_owed`, `drain_owed` and `map_owed` run their
  chore and apply again (document-gate D6); `no_space` stalls that replica's apply, reports it,
  and retries when space frees; a stage-1 throw is rolled back with the position unmoved
  (`index.hh:790-793`) and retried after the chore yields, and a third throw at one entry fences
  the replica (`application_failed`), which repairs by install (quest 6 A3). strata 491a1bdc moves
  the position on the allowance-path `no_space` (`index.hh:860`, `:878-880`), so its retry would
  answer `skipped`; `strata-nospace-position` fixes that, and quest 5 lands after it
  (`nospace-moves` in D16's model). Every file effect writes at the volume and offset the entry's
  starting application state names (it holds the current volume's length), a blob push included,
  so no effect appends: a failed one (ENOSPC before strata answers) stalls and retries like
  `no_space` and rewrites the same bytes, and a retry from the entry's start is idempotent
  (`retry-whole` passes, `append-on-retry` fails). The application state updates once, after the
  definite answer. The capture's mark
  (D16 step 1) is an entry like any other, so a capture, and a close, waits out `no_space` too.
- Definite, the same on every replica, position moved: `accepted`, `version_conflict`,
  `schema_conflict`, `fields_conflict` and `invalid`. Each is a function of the log, `invalid`'s
  size caps included because a group's replicas share strata's `buffered_docs_cap` and
  `lane_chunks_cap` (`index.hh:534-540`; the group sets them, never a replica). A refusal is a
  committed no-op, and the proposer's `quorum` waiter receives it (D4). The cap `invalid` returns
  before the move at 491a1bdc (`index.hh:822-824`); `strata-nospace-position` A4 makes it move,
  and until then the mark covers it. `skipped` occurs only in replay.

A replica stalled on `no_space` applies nothing past that entry. When it is the leader, every
`quorum` answer of its group waits with it, since each completes at its proposer's apply (D4),
while the followers apply on. After one election timeout of stall, with a follower applied past
the entry, the leader transfers leadership to it (`transfer_leadership`); its own waiting answers
end at their deadline as unknown outcomes (D8), and health reports the stall. A lone voter has no
one to hand to: its writes wait for space and time out the same way.

The leader prepares from its applied state plus its own unapplied proposals, and prepares nothing
before it has applied through its term's first entry (Core's NoOp, `core.h:1146`): the ready
point. Then a merge's expectations match apply in every schedule of `verdict_model.py`, so no log
entry is wasted. A client's own condition (`if_version`) is checked at prepare, answering 409
there as master does, and answers 409 from apply if it still fails there; `invalid` answers 400.
Neither is submitted again.

When a merge's expectations miss anyway (a check the predictor does not model, or a leader change
before its entry applied), apply answers `version_conflict`, `fields_conflict` or
`schema_conflict` against expectations the leader chose, and the request is submitted again
through the write path (D8) from the original request, not the prepared command: the leader
prepares it from the state it now has, and a proposer deposed meanwhile forwards it once to the
leader, which then owns the answer. The forward carries the request's placement count, so the
bound is the request's, not each leader's. A request gets at most three placements; after the third
refusal it answers a retryable 503 with `Retry-After` (Q10), so a merge is never answered 409.
`verdict_model.py` (two voters with their own applied state) reaches the deposed proposer's
forward, and under every prepare rule answers every request within the bound with no false
acknowledgement; with the ready point and pending versions it submits nothing twice.

**D4. Levels.**

- `ack`: `quorum` (the default, Q1) or `none` (acknowledged at `ProposalPlaced`). A `quorum`
  write completes when the proposer applies its placement (index, term) from its own `Committed`
  stream, and its answer carries apply's verdict: success means applied, and a refusal is a 409
  with strata's current version (Q10). Another term's entry at that index answers `superseded`,
  a definite failure the client may retry. The stream sees every entry the proposer applies, so a
  fast checkpoint never turns the answer into `committed()`'s `compacted` (`DRIVER.md`, "What
  completes when"); only an install past the placement leaves it unknown (D8). A `none` write's
  answer predates apply, so a refusal after it is silent, as its loss on a leader crash is.
- `read`: `prefix` (any replica's last refreshed snapshot, the default) or `linearizable`, both
  as durability-modes D10 defines them (`durability-modes.md:2047-2056`). A `linearizable` read
  goes to the leader (one forward, as D8), which confirms its leadership with Core's quorum-probe
  read, already a Driver call (`Driver::read`, async-drivers D2), and then reads at or past the
  commit index the probe captured. Get-by-id reads the live version map (strata
  `ARCHITECTURE.md:91-93`) through `group::read_applied` (quest 5), with no refresh; a search
  pays a refresh that covers that index. `?volatile`, master's read from the primary, means
  `read=linearizable`.
- `refresh`: `false` (periodic, 1 s), `wait_for` or `true`.
- `?commit` means `ack=quorum&refresh=true` (Q12). The per-index checkpoint, segments and blob
  volumes synced, is the explicit durability point; its route is Q8.

`all`, `memory`, `session` and `bounded` follow the release. Each one adds a driver event or the
forgetful-voter machinery, and parity needs none of them. There is no `leader` level: without
leases, the node that believes it leads serves stale reads after a partition until CheckQuorum
steps it down (up to `election_max`, 2 s by default, track-c D7), and D10's vocabulary names no
such level.

The gate's histories (R2) state their levels: writes at `quorum` and get-by-id at `linearizable`
are checked for linearizability; get-by-id at `prefix` is checked for consistent prefix only.

**D5. One root authority.** A strict-consensus group, `strata::placement`, with 3 or 5 voters
chosen from the first nodes (a lone voter on one node), holds two kinds of record:

- Node directory: id, name, addresses, certificate pin, attributes, state.
- Namespaces, one record per index: path, shard count, `rf`, settings version, and per shard its
  opaque group id, incarnation and members.

The root is a `strata::group` like any shard, and its application state is a strata index of
those records keyed by path (quest 8), not an in-memory map with whole checkpoints. Its checkpoint
is a strata manifest, its install is quest 6's, and its memory is strata's bounded caches. Records
keyed by path let placement groups later own ranges of it without a format change, and make a
pattern a prefix scan: index patterns resolve from these records at every node count, nested
paths included, and the local directory walk (`search_views.cc:2342-2362`,
`index_pattern::expand(Endpoint::cwd, ...)`) is deleted (quest 14). A scan reads the root at
`linearizable` where this node leads it, which on a lone voter costs no round trip, so one node
sees every index as master's walk does; elsewhere it reads the nearest root member at `prefix`, and
may miss an index created within that member's last refresh, unless the request asks
`read=linearizable`. Nested paths (`twitter`, `twitter/user`) are distinct keys, and journal artifacts
use opaque group ids (D97 Q3), so neither can collide. It replaces `.xapiand/indexes`, the UDP Raft
and `ELECT_PRIMARY`.

Membership is authoritative in each group's log (track-c D1). The namespace record follows a
committed membership change: the leader that commits it writes the record, and a node that finds
them apart after a crash (a group-open exchange naming members the record lacks) rewrites the
record from the group. Leader hints are never written to the root; they live in each node's cache
and are corrected by redirects (D8). A shard never gets a new group, nor a new incarnation, while a
member of its old group may live: a group that lost its quorum stays red until its members return.

Cost at 10^6 namespaces, estimated from the encoded fields and to be measured by quest 8 A4:

| Quantity | Estimate |
|---|---|
| Record bytes | a 32 B path plus 16 B of index fields, plus 51 B per shard (16 B group id, 8 B incarnation, 3 × 9 B members): about 300 B at 5 shards, 100 B at 1 |
| Root state | 10^6 × 300 B ≈ 300 MB on disk at 5 shards (100 MB at 1), plus strata's index overhead; resident memory is the cache bound, not the state |
| Checkpoint | a manifest naming the new segments: proportional to the records written since the last one, not to the state |
| Install of a new root voter | the whole pack once, ≈ 2.5 s at 1 Gb/s for 300 MB, plus its sync |
| Creation rate | one root command of under 1 KiB per index (indexes are created on first write), batched into the root's turns, then `rf` × shards group creates on the chosen nodes: estimated at least 1,000 creations per second per cluster |
| Creation latency | one root commit plus one group create, two barriers and one round trip, against master's create on first write |
| A node's shard-map cache | its own groups from their local group metadata (a node restarts without the root), plus an LRU of records filled on a miss and invalidated by a redirect, an unknown-group answer or an incarnation mismatch: the endpoint resolver cache (`RESOLVER_CACHE_SIZE`, `opts.cc`), at 10^4 entries ≈ 3 MB |

At master's default `--shards 5` (`opts.cc:43`), a million indexes are five million groups; the
default is Q9.

**D6. Membership.** Nodes find the cluster from `--join` seeds (host names resolved by consensus's
DNS dialing) and present their node certificate (D97 Q7). Liveness is the authority leader's
volatile keepalive table, with the same 500 ms × 3 misses as track-c D12. Each node keeps one
request-lane connection (D9) to the root leader, whether or not they share a shard group, and
sends a `keepalive` call on it every 500 ms. Only state transitions (`up`, `suspect`, `down`) are
logged. Multicast discovery goes, so the cluster works on Kubernetes without a multicast network
plugin (old `kubernetes.md:19-24`); that removal is the owner's (Q7). SWIM gossip follows the
release; the keepalive table is linear in nodes at the authority leader (section 10).

**D7. Placement and repair.**

- Each shard takes `rf` distinct nodes with the fewest groups, and leaders are spread by
  `transfer_leadership`.
- When `rf` exceeds the nodes, the shard runs short and health reports yellow, as the old cluster
  capped replicas. Short means one data voter and a learner on each other node, never a lasting
  two-voter group, which REPLICATION.md does not support (`REPLICATION.md:113-115`): on two nodes, `rf = 3`
  and `rf = 2 + witness` run as a voter and a learner, which serves `prefix` reads and is promoted
  in the order below when a third node joins.
- A node `down` longer than `--replace-after` (default 300 s, the old stall time) is replaced:
  add a learner, install a checkpoint, promote it, remove the dead voter. An `rf = 1` group whose
  node is down has no leader to add a learner, so it waits for its node.
- `PUT _settings {number_of_replicas}` and `POST /:nodes/<n>/:drain` use the same steps.
- Growth and shrink by `number_of_replicas` (track-c D9's mixes, plus one growth window):
  - 0 to 1, once three nodes are up: a data learner installs and is promoted, giving two data
    voters for the window, then the witness is added. The window tolerates no failure, no worse than the lone voter it grows
    from; it is added to track-c D9's windows.
  - 1 to 2: a third data learner installs and is promoted (three data voters and the witness,
    D9's replacement window), then the witness is removed.
  - 2 to 1 and 1 to 0 run the same steps backwards: add the witness, then remove a data voter; and
    remove the witness, then a data voter.

The old cluster never re-replicated, so a second failure was fatal.

**D8. Write routing.** The receiving node reads the leader hint from its shard-map cache and
forwards. A node that is not the leader answers a forward with a redirect that carries its hint;
the receiving node, never the client, follows at most one redirect, so a write takes at most two
forwards, and a second miss (an election in progress) answers 503 with `Retry-After`. A group with
no quorum answers 503. A cancelled or timed-out proposal has an unknown outcome, so the
answer says so, and retries are idempotent by version (async-drivers X11). Rejected: a node-local
"accepted" level (Q11.3). Its writes have no group order and no version check, which D13 rejects.

**D9. One transport between nodes.** Xapiand's shard RPC runs on a request/response lane of the
host transport (consensus-transport, D16). The lane generalizes `DTFT0001` and is the same lane
Detent's history fetch needs (Detent msg-44, option 1). It carries method id, deadline,
cancellation and opaque msgpack bodies, with its own credit beside the group frames. A body may be
streamed in chunks under that credit, which `get` of a stored blob (to 4 GiB) and `forward_write`
of a streamed upload need. The lane opens connections between any two nodes, not only those that
share a group (D6, D10). Rejected: a second TLS listener, or HTTP between nodes, which would mean
two identities, two sets of ports and two flow controls.

**D10. Distributed search.**

- Indexes come from the request's names and from patterns resolved by the root (D5).
- The coordinator, the node that received the request, runs `search_batched` (`search.h:145-154`):
  local shards in process, remote ones through `search_partial`. Its contract batches whole
  indexes for their statistics (`search.h:147-154`), so quest 14 keeps statistics per index and
  changes the batch key so that one call carries a node's shards.
- Statistics are exact. A `stats` round trip first gathers each index's document count, total
  length and per-term frequencies. The weights then read them (`search.h:96-99`).
- Each shard's statistics and its partial come from the same one replica per request. Answers then
  equal one node's holding those replicas. The residue: ranking statistics count dead ordinals
  (`durability-modes.md:395-397`), and document-mode replicas' physical segments differ
  (`REPLICATION.md:84-86`), so on a corpus with deletes or replaces two replica choices can rank
  differently. Counts and aggregations are logical and exact under any choice.
- prospect computes statistics from its snapshot today (`searcher.hh:104-151`), so it gains a
  supplied-statistics input.
- Aggregations merge through `serialize_results` and `merge_results`.
- Each request carries a deadline. A shard that misses it is retried once on another replica with
  the statistics already gathered (the residue above, for that shard only); a second miss is
  reported in `_shards.failed` and the result is marked partial.
- Fan-out trees, cached statistics, threshold sharing and hedging follow the release.

**D11. Idle groups close.** Groups follow Xapiand's catalog: closed when idle, reopened by a frame
or a request, quiescent in between (D97 Q4). strata's catalog owns the lifetime of a group and its
index together (X-POOL D1, D2: the catalog owns the index and its IO): a slot's open opens the
index and the group, its sweep closes both, and its commit chore for a group's index
(`catalog.hh:365-368`, `index.commit()` by default) is the group's checkpoint, so no strata commit
of a group's index happens outside one. One idle mechanism, not a second timer per group. Four
rules:

- A group closes only after a checkpoint covers its applied position, so a closed group leaves no
  live turn-log record and write-once indexes do not grow the per-host retained log (a closed group
  runs no timer that would checkpoint it later). Close refuses new proposals, drains the apply
  chore, checkpoints, and calls `Driver::close` only if the published bundle still covers applied;
  a `Committed` range that arrived meanwhile is applied and checkpointed again first.
- A reopened lone voter campaigns at `Start` (async-drivers D5, X5). Its term record and its first
  write share one turn, so the reopen adds no barrier beyond the write's own; R10 measures
  first-write latency after a reopen against master. This needs `detent:consensus-quiescence`
  (its A5), since Core persists its vote before appending its NoOp (`core.h:510`, `:1146`); the
  group links its first `propose` before it awaits `recover`.
- An idle open group holds no thread, timer, frame or descriptor of its own. It costs consensus's
  per-group row (consensus-driver's idle criterion: zero threads, frames, armed timers and
  descriptors, consensus state within 8 KiB at three voters) plus its open strata index (2
  descriptors, the directory and owner lock, and 5,410 B of heap, strata
  `docs/perf/small-indexes.md:276-284`) plus the group's own state, which quest 5 A6 states. A
  closed group holds zero descriptors.
- A node's loss costs elections only in groups that are in use. The first request to a closed
  group whose leader died pays one election, which is lazy failover.

**D12. Blobs stay in the node** (track-d D7, for the release). Each voter's `DataStorage` is
written by apply. The bound on a command is consensus's, set from the largest document master
accepts (section 1: a 16 MiB buffered body, a 1 MiB non-stored blob, a 4 GiB streamed upload; a
serialized document with its terms is larger than its JSON). Every Xapiand group sets consensus's
per-group limits (`types.h:55-67`, `ARCHITECTURE.md:41`):

| Limit | Value | Why |
|---|---|---|
| `Flow::frame_bytes` | 1 MiB | A control frame on the shared node-pair connection waits behind at most one append frame: 8.4 ms at 1 Gb/s and 84 ms at 100 Mb/s, against the 1 s minimum election timeout (async-drivers D10). At the default 0, an entry above the cap goes alone, and a 64 MiB entry holds the connection about 0.5 s at 1 Gb/s |
| `command_bytes` | 1 MiB (the default) | Equal to the frame, so no entry goes alone |
| `rpc_bytes` | 2 MiB (the default) | A frame plus its headers |
| `log_bytes` | 64 MiB (the default) | Four buffered bodies; checkpoints trigger by these bytes |

A command whose encoding exceeds 1 MiB, whatever it carries, is a **chunked command**: entries
`chunk(command id, offset, bytes)` and a final entry, the command itself, which applies it. A
chunk's apply writes its bytes at its offset (`pwrite`, so replay is idempotent): a blob's bytes go
straight into a volume of the upload's own, as master's streamed upload writes one
(`shard.cc:1259`), and other bytes into a staging file in the shard's directory, which the final
entry reads and retires. Chunks carry their proposer's term; a voter that applies an entry of a
higher term retires the staging and the volume of every open chunked command of a lower term, and
the client's upload fails, as a connection lost mid-upload fails it on master. A 16 MiB body and a
4 GiB upload then cost 16 and 4,096 entries, each under the frame cap.

Apply never unlinks: it retires a file, and the group unlinks it only after a bundle that omits it
publishes (D16). Volume rotation and a volume's CRC32C happen only at an entry, never at a
checkpoint or on a timer, so every replica allocates the same volumes.

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
2. 3 to 5 Linux VMs under QEMU on a `-netdev socket,mcast=...` L2 between guests, which also
   carries the multicast the Xapian-era cluster needs for R8, SSH on ports 18780 and up. Power
   loss is a `dm-log-writes` device under the data directory, replayed to sampled marks.

Killing QEMU loses the guest's page cache but keeps writes it already issued, so it is not a power
loss; `dm-log-writes` is. VM work runs under quiet holds.

Replay coverage: replaying to a flush mark gives a flush-prefix state, every write before the
flush durable and none after; replaying to an entry between marks gives a prefix of the issued
writes. Neither gives a device that persisted a non-prefix subset since its last flush; the
journal's checksummed records and C3 repair answer that by design, and quest 7's simulator
injects it (torn and reordered writes at every barrier). Marks are sampled: the marks just before
and after each of 20 acknowledged writes per run, plus 20 random entries, 60 replays per run. At
an estimated 5 to 10 s per replay (remount, recovery, digest; quest 1 A3 measures it), a 60 s run
costs about 10 minutes, so R2's power-loss tier is 20 runs, each a job of at most 15 minutes, and
its other fault tiers keep 200 runs.

**D15. Ids and shards.** The 1,024-bucket id hash stays (`handler.cc:155-169`). The shard count is
fixed at create, as before. Resharding by split follows the release (track-d D4).

**D16. `DataStorage` moves with install** (the review's F1). A volume is Xapiand's file, located by
a node-local counter and offset (`shard.cc:1100-1130`), and strata's checkpoint names only its own
segments (`REPLICATION.md:88-92`). Volume numbers are a function of the log: apply allocates them
in log order, and recovery and install set the counter from the bundle, never from the directory.

Recovery has two positions. The bundle's position P_b is where the group captured the file set.
strata's durable head P_H is at or past it: strata committed before the bundle published, and it
may commit again before the next one (a crash between the two, a capture Core rejects or a
cancelled checkpoint, `driver.h:851`). strata keeps no older generation to reopen at P_b
(`retained_predecessors = 0`, `index.hh:525`), and its open fails closed on a missing file
(`index.hh:677-689`). So the file set replays from P_b and strata from P_H.

The bundle (contract `strata:checkpoint-bundle`, consensus's application image, at most
`application_cap`):

| Field | Bytes |
|---|---|
| Magic and version | 8 |
| P_b and its term, equal to the capture's `through` and `term` | 16 |
| File count, then per file: name (relative to the group's file-set directory, no `/`), length at P_b, and CRC32C once immutable (rotated) | 4 + per file 2 + name + 8 + 1 + 4 |
| Application state: length, then the application's opaque bytes (Xapiand: the volume counter, the current volume, the open chunked commands with their terms) | 4 + its own |
| CRC32C of the above | 4 |

It names no manifest CRC32C: under P_H ≥ P_b no recovery compares one, and install's pin carries
the pinned manifest's (quest 6).

A checkpoint, in order:

1. Capture, on the apply chore between two entries: P_b is the applied position, each live file's
   length and the application state are read there, and apply continues after it. If strata's
   position is below P_b (the last entries had no strata operation: chunks, NoOps, membership), the
   capture applies an empty transaction at P_b, which moves it (`index.hh:877-880`).
2. Sync each file the bundle names and the file-set directory. A failed sync fences the replica,
   which reopens through recovery: the kernel may have marked the pages clean, so a retried sync
   can succeed with the bytes lost (`sync-retry` fails).
3. Commit strata, so P_H ≥ P_b. Steps 2 and 3 may run in either order; both precede step 4.
4. Publish through `Driver::checkpoint`; consensus may then compact the log at or below P_b.
5. Unlink each retired file the published bundle omits (D12), then sync the directory.

Recovery, on the group's chore after `Restore` only decoded the bundle on the loop:

1. Open strata at P_H; fail closed if P_b > P_H (document-gate D6's cross-check).
2. Truncate each named file to its length, failing closed when one is missing or short; delete
   every other file in the file-set directory; sync it.
3. Load the application state, then replay every committed entry above P_b: file effects always
   run, and strata skips at or below P_H (`index.hh:803-806`).
4. Serve no read until applied reaches P_H: strata's documents in (P_b, P_H] name bytes the replay
   has not rewritten yet.

A crash inside recovery repeats it; truncation and deletion converge. `bundle_model.py` checks
these rules at every crash point of seven scripts under power loss and process crash, a second
crash before and halfway through replay included: the rules fail at none, and each of nine
variants that breaks one rule fails at some (section 12).

Install moves the file set on the same transfer as the segment pack: durability-modes D17's file
sets (`durability-modes.md:2289`), built here for install only. The leader sends its latest
bundle, each named file's prefix to the bundle's length, and a segment pack pinned at its current
manifest, at or past P_b; the learner then recovers as above. Rotated volumes are immutable, so a
retried install sends only the files the learner lacks or holds short.

Without this, a learner from D7's replace, `rf` growth or migration holds locators naming volumes
it lacks, and two repairs in a row leave the blobs on no replica. Rejected: blobs inside strata
(track-d D7, a non-goal for the release and the owner's call); it removes the file set but moves
4 GiB blobs into segments. Rejected: replaying everything from P_H (`a1-literal` loses blob bytes
in (P_b, P_H]), and reopening strata at P_b (no generation is retained there).

**D17. RESTORE goes through the group, at every `rf`.** Master's RESTORE writes no WAL
(`DB_DISABLE_WAL`, `search_views.cc:1687`, `handler.cc:1159`). Here the RESTORE view prepares its
documents as today and proposes them as batch commands, each as many prepared documents as fit
the 1 MiB frame (D12), pipelined without waiting for each commit; the response waits for the last
batch at the request's `ack` level, and `?commit` adds the refresh. At `rf = 1` this adds a
sequential log write of the prepared bytes and one barrier per turn, shared by every group on the
host, which master's RESTORE never paid; at `rf > 1` every voter indexes every batch, as for any
write. T12 is measured on this path and stated even if it loses (quest 10 A6). Rejected:

- Around the journal at `rf = 1` (build segments, then checkpoint): a second durability path with
  nothing for replicas.
- Leader-built segments with an install per follower (REPLICATION.md's bulk-import command): the
  acknowledgement needs a quorum to hold the files before it commits, which is segment mode's
  machinery (durability-modes D8). It becomes RESTORE's path if segment mode lands and T12 asks.

**D18. Apply leaves the loop** (async-drivers X10). The Sink applies nothing inline: it posts each
`Committed` range to the index's chore, under the index's turn, and returns. Core holds that one
range until the chore calls `applied` (a `post_call` back to the loop), so a slow apply delays
only its own group. On the chore, in order per entry: file effects, the strata write (a
retryable answer repeats only this, D3), the application state, then the group's applied
position (published after strata's write is visible), then the `quorum` waiter of that placement
(D4). Everything that must see a boundary
between two entries runs on the same chore and needs no lock: the capture (D16 step 1), close's
drain (D11), and recovery's replay. `read_applied` runs on the reader's thread against strata's
visible state once the published position reaches the read index, so it never sees an entry's
strata write without its file bytes. Core rejects a read before its term's first commit
(`NotReady`, `core.h:785`) and on a step-down (`NotLeader`, `core.h:453`); the group answers both
with a redirect (D8). Rejected: applying in the Sink (it runs inside a driver step that D7's
budget does not bound, so one group's apply would stall every group on the loop).

## 4. What stays after the release

| Item | Source | Why it can wait | Later cost |
|---|---|---|---|
| Segment mode, `verify: replica` | durability-modes D8, D17, D18 | Document mode is correct; the gain is CPU | Per index, no command change |
| `memory` ack, forgetful voters, recovery election | durability-modes D2, D3, D5, D6, D7 | `quorum` and `none` cover parity | Journal `RFTW0002`; a journal drains at a clean upgrade, so a version bump needs no long-lived reader (Q2) |
| `all`, `session`, `bounded` | durability-modes D10 | Not in the old cluster | One driver event each |
| Background scrubber | revision 0's quest 6 A3 | Reads and installs already fail closed on a checksum (quest 6 A2) | A budgeted pass per replica |
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

Sizes are lines of code plus tests. "Needs" means it must land first; "after" also means it must
land first, but marks a file-set overlap or an outside dependency rather than an API. Detent's ids
are its own: `detent:drivers-task-wheel` and `detent:drivers-completion-seam` exist, and
`detent:journal-driver`, `detent:consensus-driver`, `detent:consensus-transport` and
`detent:consensus-quiescence` (the owner's ruling in D97: quiescence is its own consensus quest,
Detent's) are written here by the names Detent gives them.

| # | Id | Title | Risk | Repo | Files | Needs / after | Size |
|---|---|---|---|---|---|---|---|
| 1 | `cluster-harness` | Multi-process and VM cluster harness | high | Xapiand | `harness/cluster/**`, `harness/run_all.sh`, `tools/vm/cluster.sh` | none | 1,200 |
| 2 | `prospect-supplied-stats` | Weights from supplied statistics | normal | prospect | `include/prospect/searcher.hh`, `src/search*`, `test/stats*` | none | 300 |
| 3 | `detent:consensus-quiescence` | Quiescent and closed groups (D97 Q4); Detent's quest, listed for what Xapiand needs | high | consensus | Detent's | after `detent:consensus-driver`, `detent:consensus-transport` | Detent's |
| 4 | `consensus-rpc-lane` | A request lane on the host transport, streamed bodies | high | consensus | `include/consensus/transport/**`, `test/transport/rpc*` | after `detent:consensus-transport` | 900 |
| 5 | `group-core` | `strata::group`: propose, apply, checkpoint with a file set, read, close, recover | critical | strata | `include/strata/group/**`, `src/group/**`, `test/group/**`, `CMakeLists.txt` | after `detent:journal-driver`, `detent:consensus-driver`; lands after 3 | 1,600 |
| 6 | `group-install` | Learners, install of segments and file sets, catch-up, membership, repair, witness | critical | strata | as 5 | needs 5 | 1,400 |
| 7 | `group-sim` | Phase 3R gate rows in the simulator | high | strata | `sim/**`, `test/group_sim*`, `docs/gates/phase-3r.md` | starts now (port `design/simulator`); lands after 6 | 1,500 |
| 8 | `placement-root` | The root authority as a strata group: nodes and namespace records | high | strata | `include/strata/placement/**`, `src/placement/**`, `test/placement/**` | needs 5 | 1,100 |
| 9 | `placement-repair` | Re-replication, drain, leader balance, `rf` change, root install | high | strata | as 8 | needs 6, 8 | 900 |
| 10 | `journal-replaces-wal` | Every shard a group at `rf = 1`; chunked commands, volumes in checkpoints, RESTORE; the WAL deleted | critical | Xapiand | `src/database/{shard,wal,handler,pool}.*`, `src/xapiand_fsyncher.h`, `src/xapiand_storage_io.h`, `src/manager.*`, `src/opts.*`, `src/server/search_views.cc`, `harness/power_loss_check.sh`, `harness/profile_bulk.sh` | needs 1, 5 | 1,900 |
| 11 | `shard-rpc` | The node-to-node shard service | high | Xapiand | `src/cluster/rpc*`, `tests/rpc*` | needs 4 | 1,200 |
| 12 | `cluster-membership` | Node identity, `--join`, certificates, keepalives, `/:nodes` | high | Xapiand | `src/cluster/{node,membership}*`, `src/opts.*`, `src/manager.*` | needs 8; after `detent:consensus-transport` and 10 | 900 |
| 13 | `shard-groups` | Shards across nodes: routing, members, replicas, linearizable reads | critical | Xapiand | `src/index_resolver_lru.*`, `src/database/{pool,shard,handler}.*`, `src/cluster/**` | needs 1, 6, 10, 11, 12 | 1,700 |
| 14 | `distributed-search` | Fan-out, exact statistics, merged aggregations, patterns from the root | high | Xapiand | `src/engine/{search,strata/search}.*`, `src/database/handler.cc`, `src/server/search_views.cc`, `src/index_pattern.*`, `src/walk_gate.h` | needs 2, 13 | 1,500 |
| 15 | `cluster-api` | Settings, health, `:info`, docs, changelog | normal | Xapiand | `src/server/search_views.cc`, `src/index_resolver_lru.cc`, `site/src/content/docs/{options,installation,architecture,faq,changelog}.md`, `site/src/content/docs/{reference-guide,tutorials}/**` | needs 9, 13; after 14 | 800 |
| 16 | `cluster-migration` | One node to a cluster and back, without loss | high | Xapiand | `harness/cluster/migration*`, `site/src/content/docs/upgrade.md` | needs 9, 13 | 500 |
| 17 | `cluster-gate` | The release's cluster gate | critical | Xapiand | `docs/gates/cluster-release.md`, `harness/cluster/gate*` | needs 1, 7, 14, 15, 16 | 400 |

Quest 12 lands after 10 because both edit `src/opts.*` and `src/manager.*` (section 6 weighs the
cost); 15 after 14 because both edit `search_views.cc`. The changelog is also
`sortable-encoding`'s; 15 starts after it lands.

**Contracts** (provided, consumed):

- 3 (Detent's) provides Core's `Wake`, `LeaderGone`, the quiescent follower and the closed state
  (hard-state bytes).
- 4 provides the lane's frames: method, call id, deadline, credit, and a body streamed in chunks
  under that credit (wire).
- 5 provides the `strata::group` C++ API: the checkpoint bundle (D16's table: P_b and its term,
  the application file set with name, length and CRC32C when immutable, and an opaque
  application-state section; REPLICATION.md G6), the file-set rules (retire, never unlink at
  apply; rotation only at an entry), `quorum` completion with apply's verdict and the leader's
  ready point (D3, D4), `read_applied` after `Driver::read` (D4), apply off the loop (D18), and
  close after a checkpoint, with strata's catalog owning the lifetime (D11). It consumes the
  drivers' `Driver<Sink>` (D97 Q2).
- 6 provides install of the segment pack and the file set on one transfer (durability-modes D17's
  file sets, install only).
- 8 provides the authority's records (one per index, bytes), their prefix scan, and its checkpoint.
- 10 provides Xapiand's command (D3), the chunked command (D12), the volume file set (D16) and the
  RESTORE batch (D17), all bytes, and the HTTP `ack`, `read`, `refresh` and `?commit` semantics.
- 11 provides the RPC methods `forward_write`, `stats`, `search_partial`, `get`, `count`, `info`
  and `keepalive` (msgpack, versioned through the hello's range).
- 15 provides `number_of_replicas` back, `GET /:cluster/health`, and `GET /` `cluster_name` and
  `nodes` (routes per Q8).

**Acceptance criteria and evidence:**

- **1.** A1: the six `cluster_check.sh` items (section 1), rewritten for N = 3 and 5 processes,
  run as a job; on master they pass at N = 1 and the cluster items fail as expected. A2: the
  history checker rejects a planted lost write, a planted stale read, and a planted stale
  get-by-id answered by a partitioned old leader in a history checked as linearizable
  (`--fails-without`). A3: three VMs boot on one L2 that carries multicast between them, and a
  `dm-log-writes` replay of a planted unsynced write finds it missing at the earlier mark; the job
  states the seconds per replay (a job under an exclusive hold). A4 [artifact]
  `harness/cluster/README.md`.
- **2.** A1: weights and orders with supplied statistics equal those from the shards' own merged
  statistics on the release-rows corpus (a job). A2: a mutant that ignores the input fails A1.
- **3.** Detent's criteria; Xapiand needs these from it, at Detent's scale (Detent:5e4903e8): a
  quiescent group reopens on a frame or a request with no election while its leader is unchanged;
  one failure detector per node pair raises `LeaderGone` for all its groups; no term has two
  leaders with quiescence in the simulation; zero heartbeat frames and the idle CPU per group,
  measured at 10^6 groups per host.
- **4.** A1: calls with deadlines and cancellation over shared connections; a full group credit
  does not delay an RPC, and a full RPC credit does not delay a heartbeat (`reader_test` shape,
  control failing). A2: one CPU-hour of fuzzing on the frame decoder, zero findings. A3: Detent's
  history fetch expressible as one method. A4: a 4 GiB streamed body crosses under the lane's
  credit while a heartbeat on the same connection keeps its gaps under the 1 s minimum election
  timeout (a job). A5: a call between two nodes that share no group (a test).
- **5.** Numbered as the quest file. A1: a lone voter over strata's faultfs (unsynced sectors and
  directory entries lost) for strata, the file set and the journal host, with `HeldCompletion`
  choosing the cut: create, propose, apply, checkpoint and recover at every crash point, the
  windows between strata's commit and publication and between publication and the retired files'
  unlink included, and a second crash at every point of recovery. The run covers a staging file
  consumed above the bundle, an aborted upload, an upload open across a checkpoint, a cancelled
  checkpoint, a strata commit outside the group, a refused write, a checkpoint right after chunks
  or NoOps, a write whose first strata attempt answers `no_space`, a blob push that fails partway
  with ENOSPC, and a checkpoint whose file sync fails, with the application's
  counter, current volume and open uploads in the bundle's application state. Oracle: every
  acknowledged command is in the recovered log, and the digest (documents with versions resolved
  through their locators, the file set, the application state) equals a replay from empty of the
  recovered log's committed commands; reads open only at P_H and match that replay through it.
  Mutants, each `--fails-without`: replay the file set from P_H; unlink at apply; publish before
  the files sync; capture inside an entry's apply; file effects gated on strata's verdict; the
  application state gated on it; skip the capture's mark (`no-mark`); commit strata after
  publication; append on retry; retry a failed checkpoint sync and publish (the `crash` gate
  check, jobs). A2: three and
  five voters in process: every `quorum`-acknowledged entry survives any one crash (two at five),
  and digests, file sets included, are equal after every schedule; seeds and crash points stated;
  one replica's storage budget refuses at allowance growth and its file-set write fails once with
  ENOSPC, then both free, and digests stay equal, a mutant that appends on retry failing it (a
  job). A3: `ack=none` completes at
  `ProposalPlaced` and `quorum` when the proposer applies its placement, with apply's verdict;
  with the followers' appends held, `none` completes and `quorum` does not; two merges to one id,
  a conditional write and an invalid document with an election between, every schedule: no
  acknowledged success refused, no merge answered 409, the conditional write's answer matches its
  effect, the invalid one answered 400 and never submitted again, and no second submission; with a
  predictor that misses the fields mark, every request answered within three placements (503
  after the third), a deposed proposer's refused merge forwarded once and answered by the leader;
  a leader stalled on `no_space` hands leadership to a follower applied past it; an election that
  replaces an uncommitted entry answers its request `superseded`, never submitted again by the
  group; mutants that
  acknowledge at commit, that prepare before the ready point and that lift the bound fail
  (tests). A4: `read_applied` after `Driver::read` on a leader partitioned away and deposed never
  returns a state older than a committed write; mutants that skip the probe and that publish the
  applied position before strata's write is visible fail (simulation). A5: once a group closes,
  `TurnLog::read` finds none of its records, and a reopen recovers every acknowledged entry from
  the bundle; a close racing a `Committed` range ends with a bundle covering applied; a mutant
  that closes without a covering checkpoint fails (a test). A6: an idle open group holds no
  thread, timer, frame or descriptor of its own beyond consensus's row and its open strata index;
  memory, descriptors and idle CPU per group stated at 10^4 groups for open idle, quiescent and
  closed groups, with a close's cost (its checkpoint's syncs); a closed group holds zero
  descriptors; a catalog close of an idle group publishes a bundle covering applied (an
  `exclusive` job and a test). A7: `detent:consensus-quiescence` A5's consumer check: a reopened
  lone voter's term record and its first write share one turn, the barrier count of that write
  equal to a write to an open group (a test counting barriers, after that quest lands). A8, A9:
  as the quest file. A10: apply leaves the loop: with one group's apply chore held, another group
  on the same loop proposes, commits and applies; capture against apply, close against a
  `Committed` range and `read_applied` against apply, each with its scheduling points enumerated,
  under TSan (tests). ASan, UBSan and TSan over the suite.
- **6.** A1: a learner installs a 64 KiB-chunked segment pack and a file set from a pinned
  manifest, catches up and is promoted; replacement and removal pass across leader changes
  (jobs). A2: a mutant install that skips the file set fails A1's digest (`--fails-without`); a
  retried install sends only the files the learner lacks or holds short (counted). A3: a damaged
  segment or file fences the replica, which reinstalls at or below commit (C3) and serves no
  corrupt byte (a test per file class). A4: with Q3 (a), the witness stores the log, never
  applies, and leads only to hand off (simulation).
- **7.** phase3r A2 to A4 on this scope: at least 10^7 steps over at least 1,000 seeds for 3 and 5
  voters, with crashes, power loss at every barrier with torn and reordered writes, partitions,
  clock skew, add, remove, replace and D7's growth window, chunked commands cut by a leader
  change, and corruption of every file class. The linearizability checker passes over write and
  `linearizable` get-by-id histories. Zero `quorum`-acknowledged loss, zero served corruption, and
  a rebuild after disk loss (simulator jobs; mutants killed).
- **8.** A1: create, read and delete of nodes and namespace records, with nested paths distinct
  (tests). A2: placement puts `rf` distinct nodes per shard and spreads leaders within one per
  node (a property test). A3: the authority survives the loss of any minority of its voters
  (simulation). A4: at 10^6 namespace records of 5 shards each (records only): the root's resident
  memory at most 256 MiB, its checkpoint time and bytes, creations at least 1,000 per second from
  64 concurrent clients with p50 and p99 latency, and a node's cache size at its default, each
  stated (a job). A5: a prefix scan returns exactly the paths a directory walk returns over the
  same tree, nested paths included (a property test).
- **9.** A1: a node down past `--replace-after` is replaced with no acknowledged loss (a simulator
  and harness job). A2: a drain empties a node. A3: `number_of_replicas` 0, 1, 2, 1, 0 on a live
  index under writes, through D7's steps, with no acknowledged loss (a harness job). A4: a new
  root voter installs at 10^6 namespace records, the time stated (a job).
- **10.** A1: E2E and the docs suite green with every shard a group. A2: `harness/power_loss_check.sh`
  replaces `wal_recovery_check.sh`: `dm-log-writes` replay at D14's sampled marks, with no
  acknowledged `quorum` write missing (a job). A3: T11 against a baseline where the WAL writes:
  PUT from concurrent clients on master at the WAL's async default, and at `DB_SYNCHRONOUS_WAL`
  (`flags.h:42`; no request sets it, so a harness patch does, kept in `harness/`); the journal
  costs at most half of the WAL per document in CPU and on the critical path (`profile_bulk.sh
  --put`, exclusive jobs, the lead's). A4: Q14, the journal at `none` at least as fast per
  acknowledged write as the WAL's async default, and at `quorum` as its synchronous WAL (exclusive
  jobs). A5: `git grep -n DatabaseWAL -- src` is empty (a job). A6: T12, RESTORE docs/s on D17's
  path: at `rf = 1` at least 1.0× pre-7c master (x7-gate A12's bar), and at `rf = 3` on three
  processes, stated; a miss goes to the owner with its profile, not waived (exclusive jobs). A7:
  `kill -9` at each crash point of a stored-blob write and of a chunked upload around a checkpoint;
  every acknowledged blob reads back byte-equal, and a mutant that skips D16's truncation fails (a
  job). A8: a command above 1 MiB is chunked, and a mutant that proposes it whole is refused by
  `command_bytes` (a test).
- **11.** A1: each method round-trips, with the error mapping, deadlines and cancellation tested.
  A2: one CPU-hour of fuzzing per decoder. A3: an N − 1 peer is refused with a named error.
- **12.** A1: three nodes form from one seed. A node from another cluster, or without a pinned
  certificate, is refused (tests and a harness job). A2: `GET /` and `/:nodes` match the authority.
  A3: plaintext only behind an explicit flag on loopback (Q5). A4: a node that shares no group with
  the root leader keeps its keepalives, and its `down` is logged after 3 misses (a harness job).
- **13.** A1: section 1's checklist items 1, 2, 5 and 6 on 3 and 5 processes. A2: a write to any
  node takes one forward with a fresh hint and at most two with a stale one (counted in a job).
  A3: a 503 under no quorum and on a second miss, and the receiving node follows the redirect
  (tests). A4: a cancelled proposal's retry is applied once (a test). A5: through the proxy capped
  at 100 Mb/s, a 16 MiB buffered document with a stored blob and a 256 MiB streamed upload commit
  at `rf = 3` while another group on the same node pair keeps its heartbeat gaps under the 1 s
  minimum election timeout; on loopback, a 4 GiB upload does the same, with its disk (about
  24 GiB) stated; a control with `frame_bytes` 0 and `command_bytes` 64 MiB fails the capped case
  (`--fails-without`, jobs). A6: after a replace, `rf` 1 to 3, and a drain, every stored blob reads
  back byte-equal from the new replica, and a mutant install without the file set fails (a job).
  A7: with the old leader partitioned by the proxy, its stale get-by-id at `prefix` is flagged by
  the checker, and at `linearizable` the history passes (a job).
- **14.** A1: section 1's items 3 and 4, and search, count and every aggregation kind equal over
  one node and over three, five and two-shards-per-node placements (agg-shard-parity's test,
  cross-node; a job). A corpus with replaces is included: counts and aggregations exact, ranking
  compared within one replica choice. A2: a dropped partial fails A1 (`--fails-without`). A3: a
  killed replica mid-search is retried, or the result is marked partial within the deadline. A4: a
  wildcard search from any node of 3 and 5 returns the same indexes and hits as master's one-node
  walk over the same tree, nested paths included (a job), and `git grep -n
  "expand(Endpoint::cwd" -- src` is empty.
- **15.** A1: the API's routes and settings documented, with the docs E2E green. A2: GET
  `_settings` shows `number_of_replicas`; health is green, yellow or red by replica count. A3
  [artifact]: the changelog.
- **16.** A1: a node with indexes seeds three nodes, `number_of_replicas` 0 to 2, digests equal,
  stored blob bytes included, no document lost (a job). A2: a shrink and a drain lose nothing. A3:
  a node restarted after its log was truncated catches up by install.
- **17.** Section 7's rows, each with its commit and jobs, reviewed (lens `system`).

**Changes to existing quests** (the lead's to make):

- `phase3r`: A1 to A4 and A7 become quests 5 to 7; A5, A6 and A8 follow the release.
- `track-c`: A2 (T2 oracles over the shared turn log), A4 (the C3 gate report) and A8 (goal 5's
  audit) stay before the release. A5 and A7 become `detent:consensus-transport` under D97 Q7. A6
  is quest 7's checker. A3 follows the release. D9 gains D7's growth window.
- `track-d`: D4's minimal mapping (D5, D15) and D7's decision (D12, D16) are here; the rest
  follows the release.
- `phase7b`: A1, A2, A3, A6, A7 (as D13) and A8 are quests 10 to 17; A4 runs at three nodes only
  (Q4); A5 follows the release.
- `format-freeze`: after quest 17 (`cluster-gate`) and after `sortable-encoding`, so it reviews
  every byte below, quest 6's install and quest 8's records included.
- `drop-replicas` (planned, launchable): cancel it. It removes the setting this design brings back.

## 6. Order

```
drivers (detent: task-wheel, completion-seam, journal-driver, consensus-driver)
  ├─> group-core ─┬─> group-install ─────────────────────────┐
  │               ├─> journal-replaces-wal ──────────────────┤
  │               └─> placement-root ─> cluster-membership ──┼─> shard-groups ─> distributed-search ─> cluster-api ─> cluster-gate ─> format-freeze
  ├─> consensus-quiescence (detent; lands before group-core) │
  └─> consensus-transport (detent) ─> rpc-lane ─> shard-rpc ─┘
```

The critical path is the drivers, then `group-core`, `group-install`, `shard-groups`,
`distributed-search`, `cluster-api`, `cluster-gate` and `format-freeze`: seven of the sixteen.
`cluster-membership` also lands after `consensus-transport` and `journal-replaces-wal`. The
nearest path is `journal-replaces-wal` then `cluster-membership` (2,800 lines against
`group-install`'s 1,400); it stays off the critical path only if membership is built beside
`journal-replaces-wal` and rebased over its deletions in `opts.*` and `manager.*`, so that only its
landing waits. That overlap is the lead's call.

- **Starts now:** `cluster-harness`, `prospect-supplied-stats` and `group-sim`'s port of the
  simulator.
- **Once the drivers land:** `group-core` and `consensus-rpc-lane` run side by side, with Detent's
  `consensus-quiescence`.
- **Then:** `group-install`, `placement-root` and `journal-replaces-wal` side by side, and
  `shard-rpc` beside them.
- **Late:** `cluster-migration` beside `distributed-search`; `cluster-api` after it.

Where it meets the two format quests:

- **`format-freeze`** reviews every new byte: `RFTW0001` with the per-host layout, the hard state,
  the checkpoint bundle with its file set, Xapiand's command and chunked command, the authority's
  records and the two wire formats. It lands after `cluster-gate` and `sortable-encoding`.
- **`sortable-encoding`:** the command embeds `engine::Document` bytes, so the value encoding is
  inside it. No code ordering is needed, because both change pre-release bytes freely (D87), but
  the freeze reviews the command only after `sortable-encoding` lands.

## 7. The release's cluster gate

| Row | Target | Procedure |
|---|---|---|
| R1 Parity | Section 1's checklist | On 3 and 5 processes, and on 3 and 5 VMs |
| R2 No acknowledged loss | Zero `quorum` writes lost; histories of `quorum` writes and `linearizable` get-by-id linearizable, and `prefix` get-by-id a consistent prefix (D4) | 200 seeded runs of 60 s per fault tier (`kill -9`, `SIGSTOP`, proxy partitions); VM power loss through `dm-log-writes`, 20 runs at D14's sampled marks |
| R3 No split brain | At most one leader per group per term; digests equal after healing | The leaders' logs and a digest per group, in R2's runs |
| R4 Simulation | Quest 7's rows | Quest 7's jobs |
| R5 Failover | Write unavailability after a leader kill, p99, stated against the old 300 s stall plus a 10 to 30 s election | 100 kills on VMs |
| R6 Migration | Quest 16 | Quest 16's jobs |
| R7 The journal's cost | T11, T12 and Q14 | Quest 10's jobs |
| R8 Against the Xapian-era cluster | Ingest, search and aggregation on the same 3 VMs, beating the final Xapian release on every row (phase7b A4 at three nodes): ingest at `ack=none`, the old async replication's durability, with `quorum` reported beside it | Exclusive jobs, five interleaved trials; the old cluster on D14's multicast L2 |
| R9 Every gate | Sanitizers, one CPU-hour of fuzzing per new decoder, fixtures, coverage | CI and jobs |
| R10 Scale statement | Memory, descriptors and idle CPU per group measured at 10^4 groups per node; first-write latency for a new index and after a reopen, against master; the per-host retained turn log after 10^4 write-once indexes go idle; keepalives per second as a function of the node count; the authority's records, memory, checkpoint and install, a node's cache, and index creations per second as functions of the index and shard counts (quest 8 A4, quest 9 A4) | Measured jobs, plus the statement |

## 8. Where this depends on Detent's drivers

| Detent D97 answer | What depends on it here |
|---|---|
| Q2 (a), the Sink | Quest 5 applies `Committed` from the Sink in a chore that leaves the group's loop (Xapiand X10) |
| Q3 (b), one directory per host | D5: groups are opaque ids, so nested paths never collide. The authority's namespace record is the only path-to-group map. One lock shared with the cold tier. `format-freeze` reviews this layout |
| Q4, quiescence | `detent:consensus-quiescence` is on the release path: D11's closed groups and their bytes come before `format-freeze` |
| Q7 (a), one certificate per node | D6 and quest 12: the node directory holds the pin, and the group-open exchange checks cluster, group and epoch |

Xapiand's requirements beyond the five recorded items (D21; `SHARED_LIBRARY_DECISIONS.md:251-261`):

1. **A request lane on the host transport** (D9, quest 4). The same frame mux and credit carry
   application calls. Detent's history fetch needs it too.
2. **The witness at the driver**, if Q3 is answered (a): a voter that stores the log, never
   applies, and leads only to hand off.
3. **Reopen latency.** A request to a closed group whose leader lives commits with no election,
   and the follower failure detector is the node keepalive shared by every group on the pair
   (track-c D12), not a timer per group. A reopened lone voter's campaign at `Start` (X5) writes
   its term record in the same turn as its first write (D11).
4. **Streamed bodies on the lane** (D9, quest 4): `get` of a stored blob to 4 GiB and
   `forward_write` of a streamed upload, in chunks under the lane's credit, never one frame.
5. **Lane connections between nodes that share no group** (D6, D10): a coordinator fans out to
   any node, and every node keeps one to the root leader for its keepalives;
   `detent:consensus-transport` opens connections only while a group is shared (async-drivers
   D10).

## 9. Questions for the owner

Answered 2026-11-17 (PLAN D113): Q1 (a), Q2 (a), Q3 (a), Q4 as recommended, Q5 (b).

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
- `?commit` is `ack=quorum&refresh=true`, and the per-index checkpoint is the explicit
  durability point (D4); which route names it is Q8.
- `commit()` becomes internal (D2).

*Recommendation:* as written. Deferring blocks quest 10.

**Q7. Multicast discovery** (D6). The Xapian-era cluster found its nodes by UDP multicast
(section 1), with `--discovery-*` options; it carried Raft votes over forgeable UDP.

- (a) Remove it and the options: nodes join from `--join` seeds, host names or a DNS name with many
  addresses (a Kubernetes headless service, a Compose service, static hosts).
- (b) Keep multicast as a seed source only: a node announces on the old group and port, learns
  seeds, then joins over TLS as in (a). About 300 lines, and it works only on one L2.
- (c) Keep the old discovery messages. Rejected here: its Raft is gone, and its messages are
  unauthenticated.

*Owner's answer (2026-11-20, D141): (b),* for the zero-flag demo of nodes finding each other on one
network. Multicast only yields seeds; joining and membership stay TLS with node certificates, so an
announcement is a hint, never a member. `--join` seeds remain the way on Kubernetes and clouds.

*Recommendation: (a).* DNS seeds cover every deployment the docs describe, Kubernetes included
(old `kubernetes.md:19-24`), and a removed option is cleaner than one kept for LANs. Deferring
blocks quest 12's options.

**Q8. HTTP routes.** New or changed against master:

| Route | Master | This design |
|---|---|---|
| Node list | none (`GET /.nodes` in the Xapian-era cluster) | `GET /:nodes` |
| Cluster health | none | `GET /:cluster/health` |
| Drain | none | `POST /:nodes/<n>/:drain` |
| Per-index checkpoint | `COMMIT /idx/` commits the shard (`search_views.cc:609`, `:1563`) | see options |
| `FLUSH` | pathless admin command: closes and commits every open writable (`search_views.cc:1521-1543`) | unchanged |

- (a) The `:` routes above, and the per-index checkpoint is the existing `COMMIT /idx/`
  (durability-modes Q12): one verb, the same meaning of "make it durable", and `FLUSH` keeps its
  one admin meaning.
- (b) As (a), but the checkpoint is `POST /idx/:flush`, Elasticsearch's name; `:flush` then means
  two things by whether it has a path, and `COMMIT` needs a meaning of its own or goes.
- (c) Elasticsearch's names throughout: `_nodes`, `_cluster/health`, `_flush`.

*Recommendation: (a).* `/.nodes` is not kept as an alias. Deferring blocks quests 13 and 15.

**Q9. The default shard count.** Master creates indexes on first write with `--shards 5`
(`opts.cc:43`). With a group per shard, a host holding a million indexes holds five million groups,
and the root five shard entries per index (D5).

- (a) Keep 5.
- (b) Default to 1, as Elasticsearch has since 7.0; `number_of_shards` still sets more at create.

*Recommendation: (b)*, for the many-indexes workload; it changes no format. Deferring blocks
quest 15's defaults and R10's statement.

**Q10. What a `quorum` write's answer means when apply refuses it** (D3, D4; group-core's design
review B4, B8). A committed command can be refused at apply: a version, schema or fields
conflict, or an invalid document. `verdict_model.py` runs two voters, each with its own applied
state, through every schedule of two merges, a conditional write (and under (b) an invalid
document) to one id, a commit, each voter's apply and an election: 2,520 schedules under (a),
20,160 under (b), for each prepare rule.

- (a) Total apply plus the leader's ready point: `quorum` completes at commit, and a refusal of a
  committed command fences the replica (`application_failed`). Safe only while prepare predicts
  every check apply makes: with the ready point and pending versions it acknowledges no refused
  write (0 of 2,520), but a predictor that misses the fields mark acknowledges a refused write in
  1,813 schedules and fences every replica in them, a shard outage from a prediction gap.
- (b) The answer carries apply's verdict: `quorum` completes when the proposer applies its
  placement, a refusal is a committed no-op on every replica, a refused merge is submitted again
  (at most three placements, D3), a client's own failed condition answers 409 and an invalid
  document 400. Under every prepare rule: 0 false acknowledgements, 0 fences, 0 merges answered
  409, every request answered within the bound, the deposed proposer's forward reached (9,296
  forwards under the fields-mark gap). The ready point and pending versions remove the extra
  submissions (35,296 under the applied rule, 0 with both) and the answers after the bound (2,152
  schedules under the applied rule, 1,680 under the gap, 0 with both).

What a client sees, under (b): a 2xx at `quorum` means applied, never only committed, so a
get-by-id on the leader right after it reads the write; a 409 carries strata's current version,
answered at prepare as on master, or after commit when prepare's prediction missed; a
merge (no `if_version`) is never answered 409, and after its third refused placement answers a
retryable 503 with `Retry-After` (the alternative, a 409 there, breaks that rule for a conflict the
client did not ask about); `ack=none` is answered at placement, and a refusal
after that is silent, as its loss on a leader crash is. Each `quorum` answer also waits for the
leader's apply of its entry, which master's PUT already does before it answers (the strata write
is in the request). Under (a), the same, except that a missed prediction answers 2xx for a write
that never took effect and then fences the shard.

*Recommendation: (b)*, with the ready point and pending versions kept for zero re-submissions,
and the retryable 503 after the bound. It
is the lead's default for quest 5; it changes the reply only against this design's revision 1, not
against master. Deferring blocks quest 5's A3 and quest 10's HTTP semantics.

## 10. Limits

- **Two nodes cannot fail over writes** under any Raft design. With Q3 (a), reads continue from the
  survivor.
- **Lazy failover:** the first request to a closed group whose leader died pays an election, about
  the election timeout. It is silent apart from that latency.
- **`ack=none` and `read=prefix` lose and stale silently.** A leader crash loses unreplicated
  `none` writes, and a replica serves its last refresh (1 s by default) after a failover. Both are
  the caller's choice and are documented.
- **Document mode costs CPU:** each of `rf` voters indexes every document, until segment mode.
- **The authority leader is linear in nodes** (keepalives), and the root's state in indexes and
  shards (D5's arithmetic: about 300 MB at 10^6 indexes of 5 shards, on disk behind strata's
  caches). Every index creation is a root commit. Quest 8 A4 and R10 measure it.
- **`linearizable` reads pay a quorum round trip**, and a search at that level a refresh.
- **A pattern scan away from the root leader reads at `prefix`**, so on a multi-node cluster an
  index created within the last refresh may be missing from a wildcard search (D5).
- **Blob locators:** replicas stay equal only while `DataStorage`'s volume allocation is a pure
  function of the log (D16). A violation would be silent, so R3's digest covers stored blobs.
- **An upload cut by a leader change fails** and the client retries it whole (D12).
- **A refused write's blob bytes stay in its volume** (D3): file effects run before strata's verdict,
  so a refused stored blob leaves unreferenced bytes, the same on every replica; nothing in the
  release reclaims them.
- **Recovery replays from the last bundle**, not from strata's head: blob bytes in (P_b, P_H] are
  rewritten, and reads wait for it (D16).
- **Power loss** is tested by replay to sampled marks on Linux, prefix states only (D14); macOS
  `F_FULLFSYNC` is not covered by the VMs.
- **One more round trip per search** for exact statistics.
- **No mixed-version cluster** in the first release.

## 11. Status, and what to review

Designed, not built. Every quest's evidence is still to be produced. Review, in order:

1. D2, D3, D12, D16 and D17: the WAL removed at `rf = 1`, chunked commands, blobs written at
   apply and moved by install, and RESTORE through the group.
2. D4: `linearizable` in place of `leader`.
3. D5 and D6: one root group as a strata index, with keepalive liveness, against 10^6 indexes.
4. D9's request lane, and the requirements to put to Detent's lead in section 8.
5. The quest boundaries and the critical path in sections 5 and 6.
6. The gate rows R2, R3 and R5, which decide what "works in a cluster" means.
7. Revision 2: D3, D4, D11, D12, D16 and D18, with Q10, against group-core's design review.

## 12. Review answers

The design review at Xapiand:5dc60256a, finding by finding.

| ID | Fix | Where |
|---|---|---|
| F1 | The checkpoint bundle names `DataStorage`'s volumes, open uploads included; recovery truncates and replays them; install moves them with the segment pack (D17's file sets, install only) | D16; contracts 5, 6, 10; quests 5 A1, 6 A1-A2, 10 A7, 13 A6, 16 A1 |
| F2 | RESTORE goes through the group at every `rf`, as batch commands; T11's baseline is PUT with the WAL writing, async and synchronous; T12 on the new path, stated even if it loses | D17; quest 10 A3, A6; R7 |
| F3 | `read=linearizable` ships through Core's quorum-probe read and the live version map; `leader` is dropped; R2 states each history's level; D1 restated per term | D1, D4; section 4; quests 1 A2, 5 A5, 13 A7; R2 |
| F4 | Patterns resolve from the root's namespace records by prefix scan, nested paths included; the local walk is deleted | D5, D10; quests 8 A5, 14 A4 |
| F5 | Consensus's per-group limits, from master's largest document: 1 MiB frames and commands, larger commands chunked; heartbeat test under a maximal upload, with a control | D12; quests 10 A8, 13 A5, 4 A4 |
| F6 | The root is a strata index of one record per index; bytes, checkpoint, install, creation rate and cache stated with arithmetic, then measured at 10^6 namespaces | D5; quests 8 A4, 9 A4; R10; Q9 |
| F7 | One replica per shard per request for statistics and partial; the residue stated; a corpus with replaces | D10; quest 14 A1 |
| F8 | The 0-to-1 path through a growth window; 0, 1, 2, 1, 0 tested | D7; quest 9 A3; track-c change |
| F9 | Short means one voter plus learners, never two voters | D7 |
| F10 | Quest 10's files cover every WAL user; 12 after 10 and `detent:consensus-transport`; 15 after 14; `upgrade.md` is 16's alone; 10 and 13 need 1; 15 needs 9 | Section 5's table; section 6 |
| F11 | Checklist items 3 and 4 move to quest 14 | Quests 13 A1, 14 A1 |
| F12 | `format-freeze` after `cluster-gate` and after `sortable-encoding` | Section 5's changes; section 6 |
| F13 | Quiescence is `detent:consensus-quiescence` (D97); Xapiand's needs at Detent's scale, 10^6 groups | Section 5 row 3; section 8 |
| F14 | A group closes only after a checkpoint; R10 measures the retained log | D11; quest 5 A6; R10 |
| F15 | A reopened lone voter's term record shares its first write's turn; R10 measures first-write latency | D11; section 8 item 3; R10 |
| F16 | Streamed bodies on the lane, and lane connections between nodes sharing no group | D9; section 8 items 4, 5; quest 4 A4, A5 |
| F17 | The group log is authoritative; the record follows and is reconciled; no leader hints at the root; no new group while an old member may live | D5 |
| F18 | Coverage stated; marks sampled; cost estimated; the multicast L2 named | D14; quest 1 A3; R2 |
| F19 | Multicast's removal and the routes put to the owner | Q7, Q8; Q6 points its route to Q8 |
| F20 | The batch key changes; statistics stay per index | D10 |
| F21 | R8 at `ack=none`, with `quorum` beside it | R8 |
| F22 | Keepalives ride the request lane, one connection per node to the root leader | D6; quest 12 A4 |
| F23 | The receiving node follows at most one redirect; counted | D8; quest 13 A2, A3 |
| F24 | The scrubber is deferred: reads and installs fail closed already | Section 4; quest 6 |

The group-core design review at strata:491a1bdc, finding by finding. The models are
`cluster-release/bundle_model.py` and `cluster-release/verdict_model.py`, the review's two
extended to these rules; each exits 0 only when the rules pass and every broken variant fails.

| ID | Fix | Where |
|---|---|---|
| B1 | Two positions: the file set replays from P_b and strata skips at or below P_H; recovery fails closed if P_b > P_H; the capture runs on the apply chore between entries and moves strata's position to P_b; files sync and strata commits before publication; reads wait for P_H. New: file effects run whatever strata answers (`verdict-gated` fails), and an entry strata never saw cannot leave its position behind P_b (`no-mark` fails) | D3, D16, D18; quest 5 A1 |
| B2 | Apply retires; the group unlinks after a bundle without the file publishes, then syncs the directory | D12, D16 step 5; quest 5 A1 |
| B3 | An opaque, length-prefixed application-state section in the bundle | D16's table; contract 5 |
| B4 | Total apply for per-replica verdicts; semantic refusals are committed no-ops whose verdict the `quorum` answer carries; the leader's ready point and pending versions; the owner's call on the reply | D3, D4, Q10; quest 5 A3 |
| B5 | strata's catalog owns the group and index lifetime, and its commit chore is the group's checkpoint; the idle cost stated per part | D11; quest 5 A6 |
| B6 | faultfs under strata, the file set and the journal host; a second crash inside recovery; the oracle replays the recovered log from empty | Quest 5 A1 |
| N1 | A5 observes `TurnLog::read` and a reopen; the `close_group` guard is the lead's to route | Quest 5 A5 |
| N2 | A7 is `detent:consensus-quiescence` A5's consumer check; the first `propose` links before `recover` is awaited | D11; quest 5 A7 |
| N3 | A3 holds the followers' appends | Quest 5 A3 |
| N4 | `NotReady` and `NotLeader` answer a redirect; the applied position publishes after strata's write is visible | D18; quest 5 A4 |
| N5 | `Restore` only decodes; truncation, deletion and the strata open run on the chore | D16, recovery |
| N6 | Rotation and CRC32C only at an entry; names relative to the file-set directory, deletion inside it | D12, D16's table |
| N7 | Apply leaves the loop, with its interleavings | D18; quest 5 A10 |
| N8 | A2 runs five voters with seeds and crash points stated; the journal pin is the lead's | Quest 5 A2 |
| N9 | The bundle drops the manifest CRC32C; install's pin carries it | D16 |

The re-check at Xapiand:edb610078:

| ID | Fix | Where |
|---|---|---|
| B7 | `strata-nospace-position` keeps the position on every retryable answer, and quest 5 lands after it; a retry repeats only the strata write; a stage-1 throw retries, the third fences; D3 lists the position-moving answers | D3, D18; quest 5 A1, A2 |
| B8 | Re-submission from the original request for leader-chosen expectations only, at most three placements, a deposed proposer forwards once, a retryable 503 after the bound (owner, Q10); the model has per-voter state | D3, Q10; quest 5 A3 |
| N10 | `no-mark` and commit-after-publication join A1's mutants, with a checkpoint after chunks | Quest 5 A1 |
| N11 | The mark is an entry: its retryable answers retry, so a capture and a close wait out `no_space` | D3 |
| N12 | `invalid` is definite; replicas share strata's caps; the cap path's position is `strata-nospace-position` A4's | D3 |
| N13 | The application state's update is verdict-independent (`appstate-gated` fails) | D3 |
| N15 | A stalled leader holds its group's `quorum` answers; it hands leadership to a follower applied past the entry after one election timeout | D3; quest 5 A3 |

The round 2 re-check at Xapiand:4f0c74bdb:

| ID | Fix | Where |
|---|---|---|
| B10 | File effects are positional from the entry's starting application state; a failed one stalls and retries like `no_space`; a failed checkpoint sync fences | D3, D16 step 2; quest 5 A1, A2 |
| N16 | A forwarded re-submission carries its placement count | D3 |
| N17 | A3 answers an entry replaced by an election `superseded` | Quest 5 A3 |
