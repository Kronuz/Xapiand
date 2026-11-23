# Distributed search: one node's answer from many

A search that reaches any node of a cluster answers as one node holding every shard would: the
same hits in the same order with the same weights to the bit, the same counts and the same
aggregations (cluster-release D10). The node that received the request is the coordinator;
`src/cluster/search.{h,cc}` is its fan-out, over the shard RPC's `stats` and `search_partial`
(`docs/mechanisms/shard-rpc.md`).

## The two rounds

```
coordinator                                  one replica per shard
  shards(index) from the shard map  ──┐
  stats round ───────────────────────┼──>  gather(): N, total length, df/cf of the query's terms,
                                      │      and the terms each expanding node matches
  sum by index; finish(): each        │
  expansion's limit over the sum      │
  search round (same replicas) ──────┼──>  search() with the sums and the coordinator's terms
  Merge: hits by (key, weight, id),  <┘     hits with keys, count, the spy's serialized results
  counts summed, spies merged
```

1. **Shards.** For each index the request names, `ShardMapView::shards(index)` gives its shards in
   shard order, each with the nodes holding its data. Positions run over all of them, index by
   index, so hit ids are the ids one node's search gives (`(d - 1) * total + position + 1`).
2. **Replica.** `replica_order` asks this node first when it holds the shard, then the others up
   in the record's order, then the down ones. A node's shards go in one ask per round.
3. **Statistics.** `engine::gather` answers, per index of the node's shards, the document count
   and total length (deleted documents included, as weights count them), `df` and `cf` of every
   term the query names, and for each wildcard or edit-distance node the terms it matches there.
   The coordinator sums them by index (`IndexStatistics::operator+=`; expansions join as sorted
   sets).
4. **Expansion, once.** `engine::finish` applies each expanding node's limit to the sum:
   `error` refuses past `max_expansion` (a 400), `first` keeps the first terms, `most_frequent`
   the most frequent by the summed `df`. `engine::limit_expansion` is the one implementation, used
   by a node searching alone too, so the limit sees the whole index, never a shard. A share stops
   answers at most `max_expansion` terms (one more under `error`), since no later term of it can be kept.
5. **Search.** Each node searches its shards under `engine::Merge::part()`: the top of the whole
   window, or all a collapse keeps, with each hit's sort key and collapse value. The strata engine
   weighs through prospect's `term_table` over the supplied sum
   (`docs/mechanisms/supplied-statistics.md` in prospect), and an expanding node takes the
   coordinator's terms by its place in the query (`engine::expanding_nodes`, depth first, every
   place counted, so a fresh parse numbers them the same).
6. **Merge.** `engine::Merge` ranks the parts' hits by key, weight, then id in the request's
   order, collapses again across parts, sums the counts and takes the heaviest weight for the
   percentages. `search_batched` uses the same merge for its batches. Each part's aggregations
   arrive as its spy's `serialize_results()` and merge into the request's spy.

## Failures

A failure of an ask is the replica's, except `errc::syntax`, which is the request's and ends the
search. A failed replica's shards move to their next replica and are asked once more in that
round; a second failure, or no replica left, names the shard in `failed` (`_shards.failed`: index,
shard, last node asked, reason), and `total` and `successful` count the rest. A part is merged
only when its answer arrives whole, so a replica lost mid-search leaves no hits or aggregations
behind and its retry is counted once. A retried search reads the statistics gathered from the
first replica: on a corpus with replaces, ranking may differ for that shard alone (D10's residue);
counts and aggregations stay exact. A shard lost in the statistics round is left out of the sums,
so the rest answers as one node holding the other shards.

Each ask carries its own deadline (`FanoutOptions::attempt`, 30 s from when it is sent), which the RPC binding makes the call's.

## The shard map view

`src/cluster/shard_map_view.h` is what search reads of the shard map, the contract the shard-map
task implements over the root's namespace records (strata `include/strata/placement/records.hh`):

```cpp
struct ShardReplicas { GroupId group; std::uint64_t incarnation; std::vector<NodeId> replicas; };
class ShardMapView {
	virtual NodeId self() const = 0;
	virtual task::task<std::vector<ShardReplicas>> shards(std::string_view index) const = 0;
	virtual task::task<std::vector<std::string>> indexes(std::string_view prefix) const = 0;
	virtual bool up(NodeId node) const = 0;
};
```

`replicas` are a record's members less its witnesses, the preferred leader first; `indexes` is
the prefix scan an index pattern reads instead of the directory walk; `up` is false for a node the
directory holds `down`. `StaticShardMap` stands in until then.

## Serving a part

`serve_stats` and `serve_search` run a node's part in process over `LocalShards` (the shards this
node holds, by group and incarnation), for the coordinator's own shards and for a peer's ask
alike; a shard not held is `errc::not_found`, which the coordinator retries elsewhere. A part's
spy is a clone of the request's.

## Evidence

`tests/cluster/test_search.cc`, nodes in process holding real strata shards, indexes `a` (3, 5 or
6 shards) and `b` (2), 480 documents with replaces and deletes, against `engine::search` over the
same shards on one node: `equal` (10 queries, term to phrase, partial, wildcard, fuzzy and
match_all, in 7 shapes: windows, counts, sorted, collapsed, descending; placements of 3 nodes,
5 nodes at rf 2, and two shards per node), `aggregations` (every kind, nested, three queries),
`expansions` (`first` and `most_frequent` limits a shard alone would apply differently, and
`error` past the index's count but within each shard's), `failover` (each of 5 nodes lost in
either round after doing its work; a shard with both replicas lost, named; a down node asked
last).

## Not yet

- The RPC binding of `Nodes` (`stats` and `search_partial` over `src/cluster/rpc*`): the stats
  envelope carries the query, not only its terms, and answers the expansions; `search_partial`
  carries the finished statistics and the spy's serialized form.
- Index patterns from `ShardMapView::indexes` instead of the walk (`index_pattern::expand(Endpoint::cwd`
  in `src/server/search_views.cc`).
- Fan-out trees, cached statistics, threshold sharing and hedging follow the release (D10).
