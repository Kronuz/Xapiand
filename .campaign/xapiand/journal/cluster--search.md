# cluster--search: journal

State: waiting on jobs 261121-215150-cb25b8 (build + cluster_search_* + engine search tests), 261121-215351-3a7a5b (control: mutant/xapiand/cluster-search-drop fails cluster_search_equal), 261121-215355-00d31e (control: mutant/xapiand/cluster-search-per-shard fails cluster_search_expansions)
Quest: .campaign/xapiand/quests/cluster.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 (fan-out and statistics) built at Xapiand:e09565f87: `src/cluster/search.*` (coordinator,
two rounds, retry once, `failed`), `src/cluster/shard_map_view.*` (the view and `StaticShardMap`),
engine `gather`/`finish`/`limit_expansion`/`Merge` and `SearchRequest::statistics`
(`src/engine/search.*`), strata scopes over supplied statistics and coordinator expansions
(`src/engine/strata/search.*`), `tests/cluster/test_search.cc` (cases equal, aggregations,
expansions, failover), `docs/mechanisms/distributed-search.md`. Job 261121-215150-cb25b8 builds it
and runs `cluster_search_*` with the engine search tests (wide_search, engine_search_strata,
aggregation_shards, for the `search_batched` refactor). Never compiled before that job.

## Next

1. Read 261121-215150-cb25b8; fix what it shows.
2. Read the controls (queued after 1): `mutant/xapiand/cluster-search-drop` (Xapiand:016ae7a6e,
   `search_take` drops node 2's part; `cluster_search_equal` must fail) and
   `mutant/xapiand/cluster-search-per-shard` (Xapiand:ea804fed4, strata `expansion_of` ignores
   `plan.expansions`; `cluster_search_expansions` must fail). Rebase them onto any fix.
3. Round 2, patterns (needs B1): `index_pattern::expand` over `ShardMapView::indexes(prefix)`
   candidates, the walk, `walk_gate.h` and `tests/fuzz_index_expand.cc` deleted, the no-walk check.
4. The RPC binding of `cluster::Nodes` (needs B2), and the coordinator's call from the handler
   (after shard-map, which takes `handler.cc`).

## Blocked on

- B1 (lead): patterns need `src/server/search_views.cc`, held by journal-replaces-wal and
  restore-stream: `expand_paths` (search_views.cc:2339-2401) resolves patterns from
  `pool.names()` (:2380-2386) or the walk `index_pattern::expand(Endpoint::cwd, ...)` (:2395)
  under `WalkGate` (:2390-2393). Round 2 replaces :2378-2398 with one call that reads
  `ShardMapView::indexes(prefix)` and matches the globs (`index_pattern`), which also needs the
  node's `ShardMapView` reachable there (the shard-map task's). Options: (a) grant this task
  search_views.cc:2339-2401 after journal-replaces-wal lands; (b) the api task makes the call.
  Recommendation: (a), the walk's deletion belongs with its replacement.
- B2 (lead): the RPC binding needs `src/cluster/rpc_methods.*` (rpc task's): `StatsRequest`
  carries `(field, term)` pairs, but the coordinator expansion needs the query itself (the DSL,
  as `SearchRequest`), and `StatsAnswer` needs each expanding node's terms per group;
  `SearchRequest.stats` must carry them back finished. Recommendation: widen this task to
  `src/cluster/rpc_methods.*` for those two messages, or the lead amends them.
- B3 (lead): `src/engine/strata/expand.cc:117-148`, the limit inside `expand()`, is now
  unreachable: strata search calls `expand()` with no limit and applies `engine::limit_expansion`
  (one implementation for one node and the cluster). Deleting it needs `src/engine/strata/expand.*`
  (with `max_expansion` dropped from that function's contract and its scan stopping at the cap).
  Recommendation: grant `src/engine/strata/expand.*` to this task.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- S1: The shard-map view the shard-map task implements (`src/cluster/shard_map_view.h`):
  `struct ShardReplicas { GroupId group; std::uint64_t incarnation; std::vector<NodeId> replicas; }`
  (a namespace record's members less witnesses, preferred leader first) and
  `class ShardMapView { NodeId self() const; task::task<std::vector<ShardReplicas>> shards(std::string_view index) const; task::task<std::vector<std::string>> indexes(std::string_view prefix) const; bool up(NodeId) const; }`,
  `NodeId` and `GroupId` `std::uint64_t`. `indexes` is a plain string-prefix scan, sorted, nested
  paths included; `shards` is empty for no such index. `StaticShardMap` is the placeholder.
- S2: Statistics are per index, not per group: a node answers one `IndexStatistics` per index of
  its part (its shards summed), the coordinator sums by index. Expansions ride the statistics
  round (one round trip, D10), numbered by `engine::expanding_nodes` (depth first, every place
  counted), so a node that re-parses the DSL numbers them the same.
- S3: A share of an expansion under `first` or `error` answers at most `max_expansion` (+1 for
  `error`) terms: the index's first N are within the union of each share's first N.
- S4: Retry is per round: a failed replica's shards go to their next replica once in that round;
  the search round starts from the replica that answered statistics. Only `errc::syntax` from a
  node ends the search; every other failure is the replica's. A take that throws (a malformed
  answer) ends the search, since a half-merged answer cannot be taken back.
- S5: Replica order: self first when it holds the shard, then up replicas in record order, then
  down ones (load spreading follows the release).
- S6: The coordinator serves its own shards through `serve_stats`/`serve_search` like a peer's
  ask, and every part's aggregations go through `serialize_results`/`merge_results`: one path.
