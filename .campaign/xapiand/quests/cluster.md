# cluster: Xapiand in a cluster: shard groups, membership, routing, search, API, migration

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster`.

Whole feature (replan-2026-11-20, section 1 F5). Folds `shard-rpc`, `cluster-membership`,
`shard-groups`, `distributed-search`, `cluster-api`, `cluster-migration` and `phase7b`'s release
scope (its A1 to A3 and A7; A4 and A5 are the scale rows after the release, D91, D113 Q4). About
6,600 lines by `docs/design/cluster-release.md` section 5. None of the six ships alone: the RPC has
no caller without groups across nodes, membership is wired by the shard map, and the shard map
without fan-out search returns partial results.

## Outcome

An index's shards live as groups on `rf` nodes the root chooses, and any node serves any request.
Nodes join from `--join` seeds with one certificate each (D113 Q5 (b)), and keepalives to the root
leader mark a node `suspect` then `down`. A write reaches the group leader with at most one forward;
`linearizable` reads at the leader after its quorum probe, `prefix` at any replica. A search from any
node fans out over the shard RPC with exact statistics and merged aggregations, and patterns run over
the root's records instead of the local directory walk. Users set `number_of_replicas`, read
`/:cluster/health`, `:info`'s groups and `/:nodes`, drain a node, and grow one node with data into a
cluster and back. The docs and changelog describe it.

## Acceptance criteria

Gate: the suite (`build-test`), the process tier (`e2e`, `cluster`), `no-walk` and CI (`asan`, `tsan`).

- A1 `docs/design/cluster-release.md` section 1's parity checklist, items 1 to 6, passes on 3 and 5
  processes (the `cluster` check)
- A2 Writes from any node: one forward with a fresh hint and at most two with a stale one, the
  receiving node following the redirect; 503 with `Retry-After` under no quorum or on a second miss;
  a cancelled proposal answers an unknown outcome and its retry applies once: tests with statuses and
  hop counts
- A3 Search, count and every aggregation kind equal over one node and over 3, 5 and
  two-shards-per-node placements, replaces included, and a dropped partial fails it; a replica killed
  mid-search is retried or named in `_shards.failed`; wildcard, partial and fuzzy terms expand once at
  the coordinator and match one merged index (a mutant expanding per shard fails); patterns from the
  root return what master's walk returns for the glob set, and the walk is gone (the `no-walk` check: `git grep -n "expand(Endpoint::cwd" -- src` is empty)
- A4 Three nodes form from one seed; a node of another cluster and one whose certificate is not pinned
  are refused with named errors; plaintext peers only behind the flag and only on loopback; a killed
  node is logged `down` within 2 s; with no `--join`, three nodes on one L2 find each other by
  multicast announcements and join over TLS, and a forged or other-cluster announcement adds no
  member: tests and the `cluster` check
- A5 Each shard-RPC method round-trips with values, its error mapping, deadlines and cancellation;
  `get` and `forward_write` stream their bodies; a peer outside the hello's range is refused: tests,
  a suite-sized body
- A6 `number_of_replicas` in `_settings`, changed 0 to 2 and back on a live index with health green,
  yellow and red as it should be; `:info`, `GET /` and `/:nodes` match the directory; a node with
  data, stored blobs included, seeds three nodes and shrinks back with digests equal; every route and
  setting documented with docs E2E examples, `upgrade.md` and the changelog's `[Unreleased]` written:
  the `cluster` and `e2e` checks

## Tasks

Parallel writers (`campaign start cluster --task T --files GLOBS`), one agent each, merged into the
quest branch on their own tests. Strong, high: wire, security and concurrency. Tasks that share a
file run in order, never together.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `rpc` | `src/cluster/rpc*`, `tests/cluster/test_rpc*`, `tests/cluster/fuzz_rpc*`, `docs/mechanisms/shard-rpc.md` | implementation, strong, high | Shard-RPC methods over consensus's request lane, streamed `get` and `forward_write`, error mapping, version range (A5). Needs `consensus-rpc-lane` landed |
| 1 | `membership` | `src/cluster/node*`, `src/cluster/membership*`, `tests/cluster/test_node*`, `tests/cluster/test_membership*`, `harness/cluster/membership*`, `docs/mechanisms/cluster-membership.md` | implementation, strong, high | `--join` seeds, one certificate per node, keepalives, `suspect` and `down`, node directory against the root's records (A4); builds on `rpc`'s contract |
| 2 | `shard-map` | `src/index_resolver_lru.*`, `src/database/{pool,shard,handler}.*`, `src/opts.*`, `src/manager.*`, rest of `src/cluster/**`, `tests/cluster/**`, `harness/cluster/groups*`, `harness/cluster/upload*`, `docs/mechanisms/shard-groups.md` | implementation, strong, high | Shards as groups on `rf` nodes, write routing with at most one forward, `linearizable` and `prefix` reads, options (A2, part of A1). Shares `shard`, `handler`, `pool`, `manager`, `opts` with `journal-replaces-wal`: starts when it lands, or narrowed to `src/cluster/**` before |
| 3 | `search` | `src/engine/search.*`, `src/engine/strata/search.*`, `src/database/handler.cc`, `src/index_pattern.*`, `src/walk_gate.h`, `tests/test_index_pattern.cc`, `tests/fuzz_index_{pattern,expand}.cc`, `tests/cluster/test_search*`, `harness/cluster/search*`, `docs/mechanisms/distributed-search.md` | implementation, strong, high | Fan-out search with exact statistics and merged aggregations, one coordinator expansion, patterns over the root's records, the walk removed (A3). Takes `handler.cc` after `shard-map` merged |
| 3 | `migration` | `harness/cluster/migration*`, `docs/src/content/docs/upgrade.md` | implementation, strong, high | One node with data seeds three nodes and shrinks back, stored blobs included (A6, last clause) |
| 4 | `api` | `src/server/search_views.cc`, `src/index_resolver_lru.cc`, `tests/cluster/test_api*`, `docs/src/content/docs/{options,installation,architecture,faq,changelog}.md`, `docs/src/content/docs/{reference-guide,tutorials}/**` | implementation, strong, high (documentation part economical, medium) | `number_of_replicas`, `/:cluster/health`, `:info` groups, `/:nodes`, drain, docs and changelog (A6). Takes `search_views.cc` and `index_resolver_lru.cc` after `search` and `shard-map` merged |

Order 1 runs together; each later order starts once the earlier contracts are merged. To start before
`journal-replaces-wal` lands, the lead opens the quest with its file set narrowed to `src/cluster/**`,
`tests/cluster/**`, `harness/cluster/**` and the mechanism notes, so `rpc` and `membership` run once
`consensus-rpc-lane` lands, and widens it to the shared files when `journal-replaces-wal` lands.

## Contracts

Provides `xapiand:shard-rpc`, `xapiand:node-directory`, `xapiand:shard-map`, `xapiand:search-fanout`
and `xapiand:cluster-routes`. Consumes `consensus:rpc-lane`, `strata:group-install`,
`strata:placement-records`, `strata:placement-repair`, `prospect:supplied-stats` and
`xapiand:command`. The shard RPC rides the transport's one connection per node pair (D9); no second
listener. `format-freeze` reviews these bytes.

## Decisions

- `docs/design/cluster-release.md` open questions on which this builds with the lead's defaults:
  Q6 as written (no node-local accepted level, `?commit` = `ack=quorum&refresh=true`), Q7 (b)
  multicast kept as a seed source only (owner, D141), from consensus's `multicast-seeds`, Q8 (a) the `:` routes with `COMMIT /idx/` as the checkpoint, Q9 (b)
  one shard by default, Q-F27 (a) read-your-writes kept on the leader's replica. The owner
  accepted them (D142).
- `needs consensus-rpc-lane` (F3, with its hello limits and version range); `after
  journal-replaces-wal, group-heal, placement` (the strata pin), so the early tasks can start before
  they land and only the last integration waits.

## When done

Runs once, when the feature is complete and before it lands.

- Review: one reviewer, lens `security`, carrying correctness (membership, certificates, the RPC's
  peer-refusal surface); findings fixed on the quest branch, each with a test that fails without it.
- The cluster gate (`cluster-gate`) is this feature's measurement and the milestone's review: it runs
  its rows R1 to R10 over `group-heal`, `placement`, `journal-replaces-wal` and this feature, once all
  have landed; its rows are not gates of this landing.
- A7 Zero key configuration, membership by command (D150, D152): `xapiand init NAME` creates the cluster
  CA and prints its token; `xapiand join TOKEN` (multicast finds the cluster; `--seed HOST:PORT`
  otherwise) saves the node's identity; plain `xapiand` runs from the data directory and refuses an
  empty one with a hint; every link is TLS; no flag disables authentication.
- A8 Each `init` creates a new cluster (D150, D152): two nodes initialized separately form two
  clusters with different ids, CAs and tokens, on the same multicast group; neither seeds from the
  other; a token for one is refused by the other; a joiner refuses a server whose CA fingerprint
  differs from its token's.
- A9 `xapiand join TOKEN` whose cluster is not reachable never creates one (D150, D152): it retries with
  backoff, logs the cluster id it waits for, and completes once a member appears; a malformed token
  exits nonzero with a named error; repeating a completed join is a no-op.
- A10 The founder is not special (D151): a 5-node cluster restarts with its founder's data directory
  deleted and the other four started as plain `xapiand`; they rejoin from their persisted
  state, elect, serve, and admit a new node with the original token through any surviving voter; `join`
  with another cluster's token on an existing data directory is refused.

## May land early

None: wire format and public behavior.
