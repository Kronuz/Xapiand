> Status: CURRENT (2026-10-27) fix/cluster-agg from explore/cluster-agg (Xapiand:47d0db117); local, not landed.

# Aggregations see every match

## What

An aggregation is exact: it folds in every document the query matches, on every shard of every
node. A request with `_aggs` / `_aggregations` never aggregates a sample, whatever it sets for
`_check_at_least` or `?check_at_least` (Elasticsearch computes aggregations over all hits the
same way). The hit window (`_offset`, `_limit`) only trims the hits returned.

## How

```
get_mset(aggs)  ->  check_at_least = engine::CHECK_ALL  ->  SearchRequest  ->  Enquire::get_mset
                                                                (engine/xapian/search.cc)
   local shards: matcher walks every match     remote shards: same request, same walk
   match spy sees each checked document  ->  Aggregation::merge_results  ->  response
```

1. `DatabaseHandler::get_mset` (`src/database/handler.cc`) sets `check_at_least` to
   `engine::CHECK_ALL` (`src/engine/search.h`) whenever a spy is given. No user value is
   larger, so the user's value cannot shrink it.
2. The engine clamps the value to the database's document count.
3. The match spy sees only the documents the matcher checks. A matcher stops once it has the
   requested hits and `check_at_least` documents, so any smaller value leaves the spy with
   a prefix of the matches. In a cluster each node scans its local shards fully but a remote
   shard stops early, so every node returned a different partial result.

The older default was `check_at_least = 1` for aggregations (2019, so they would not be skipped):
two nodes, five shards, 40 documents returned `_doc_count` 19 and 26 against 40.

## Other paths

Only a request that passes an aggregation spy depends on the checked count (`SEARCH` with a body
in `src/server/search_views.cc`). Plain searches, counts and `SEARCH` without aggregations pass no
spy and keep `check_at_least` as the user gave it (default 0): their `total` is the matcher's
estimate, unaffected. Sorted queries pass a sorter, not a spy. `src/manager.cc` calls
`get_mset()` with no spy.

## Cost

A selective query with aggregations now visits every match instead of stopping after the
requested hits; a match-all query already did on a solo node. Exact aggregations need this.

## Check

`harness/cluster_check.sh 2` and `3`, step [3b]: 40 documents, `_doc_count`, `_sum` and `_avg`
asserted from every node. Without the change it fails on every node.
