# cluster--search: journal

State: waiting on jobs 261123-103621-485427 (build + cluster_search_* + engine search tests at Xapiand:5d4bdc2ab), 261123-103626-bcec8f (control: cluster-search-drop fails cluster_search_equal), 261123-103630-da49c9 (control: cluster-search-per-shard fails cluster_search_expansions)
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

1. Read 261123-103621-485427 at Xapiand:5d4bdc2ab; fix what it shows.
2. Read the controls (queued after 1): `mutant/xapiand/cluster-search-drop` (Xapiand:016ae7a6e,
   `search_take` drops node 2's part; `cluster_search_equal` must fail) and
   `mutant/xapiand/cluster-search-per-shard` (Xapiand:ea804fed4, strata `expansion_of` ignores
   `plan.expansions`; `cluster_search_expansions` must fail). Each is the head plus its mutation:
   after a fix, merge the head into it (`git merge-tree --write-tree`, `commit-tree`,
   `update-ref`), never rebase. Now Xapiand:5fa737b96 (drop), Xapiand:626af8c74 (per-shard),
   jobs 261123-103626-bcec8f and 261123-103630-da49c9.
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
| A3 | pending: 261123-103621-485427, controls 261123-103626-bcec8f, 261123-103630-da49c9 |

Jobs examined:
- 261121-215150-cb25b8 (Xapiand:e09565f87) failed, exit 8: the build completed; two named tests
  failed, both test bugs. `cluster_search_aggregations` threw "Aggregation name '_count_age' is
  not valid" (a name starting with `_` is reserved, `aggregations.cc:255`).
  `cluster_search_failover` died of SIGFPE in 0.06 s: `const auto& placement = placements()[1]`
  dangled, `build()` read a zero shard count from freed memory and took `i % 0`. Fixed at
  Xapiand:f743c697b; this job is the run without the fix. equal, expansions and the engine
  search tests passed.
- 261122-230000-c5e1b6 (Xapiand:f743c697b) failed: the build completed; failover, equal,
  expansions and the engine search tests passed; `cluster_search_aggregations` failed only its
  guard `CHECK_GT(want_spy.get_aggregation().size(), 10)` (test_search.cc:545), every case, while
  every equality check passed. A test bug: `get_aggregation()` answers `{"_aggregations": {...}}`
  (aggregations.cc:491-495), a one-key map, so the outer size is always 1; no bucket is dropped.
  Fixed at Xapiand:5d4bdc2ab: the guard reads the inner map, whose size must be the request's
  aggregations plus `_doc_count` (33), over a nonzero `_doc_count`. This job is the run without
  the fix. Its controls 261122-230010-f469d2, 261122-230014-cd91c0 skipped behind it; rerun.
- 261121-215351-3a7a5b, 261121-215355-00d31e: skipped (after the failed job); resubmitted at the
  merged mutants.

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
- S7: master merged into the task branch (Xapiand:bedec0a6c), cleanly, before the reruns. The
  two failures were the test's own, fixed in the test; no engine or coordinator change.
- S8: The aggregations guard failure was the test's (it measured the wrapper map), fixed in the
  test: the code returns every aggregation, and the equality checks passed in the same run.
