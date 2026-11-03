> Status: DRAFT (2026-11-02) a search over many indexes as built on `phase7c/wide-search`; the owner's plan is `strata/docs/status/road-to-release.md`, "Wide searches". Not reviewed.

# Wide search: indexes in batches

## What

A search that names many indexes (a wildcard, a list, `_all`) holds at most K of them open at once. It searches
a batch of K, merges that batch's best hits into the top of the whole, releases the batch, and takes the next. Before this
a search kept every index it named open until it ended, so its width was the pool size (200 by default).

## How

```
DatabaseHandler::match
  check_search_fanout          runaway cap (--max-search-indexes, 100,000): 400
  engine::search_batched       walks [0, K), [K, 2K), ...; merges
    BatchRunner (the seam)     DatabaseHandler::search_batch: pin [begin, end), refresh each, engine::search
```

- **The seam.** `engine::BatchRunner` is the one place that decides how a batch's indexes are held: today
  `DatabaseHandler::search_batch` takes a `lock_shard` per endpoint (a use of the registry entry plus one view per index,
  taken by the search) and searches them with the reopen-and-retry loop. When the cold-tier catalog's wake lands, a batch pins
  its indexes through the catalog instead; only this function changes.
- **K.** `--concurrent-search-indexes`, else a reactor's share of the open indexes: the pool size over the HTTP loops, at
  least 1 (`search_batch_indexes`). `main()` caps it at the pool size.
- **No cross-index snapshot.** Each index is read as its batch starts, so the refresh that makes a read see the writes
  acknowledged before it happens per index at that moment. Per-index read-your-writes holds; two indexes are not read at the same instant.
- **Ids.** Hit ids number the documents across all the endpoints, `(d - 1) * total + position + 1`, as `get_document`
  reads them back. A batch numbers its own shards, so the merge renumbers by position. A shard that is missing (no index) takes
  no documents but keeps its slot. A single batch is renumbered the same way when a shard is missing. An id past 32 bits (a
  `docid`) is refused with `errc::internal`, as before.
- **Merge.** Each batch is asked for the top `first + maxitems` hits with their sort keys (`SearchRequest::report_keys`;
  the strata engine recomputes the key of the hits it returns, at most a window), and the merge ranks by key, then weight,
  then id, as one search does. Under a collapse each batch returns all it kept (with the collapse value), and the merge keeps the best
  `collapse_max` per value; the total is then the kept count. Otherwise the total is the sum of the batch counts. The spy is
  shared by the batches, so it sees every match once, in shard order. `percent` is recomputed against the heaviest weight of all batches.
- **Weights.** A relevance weight is read against the statistics of its batch (document count, term frequencies), as
  Elasticsearch reads a shard's by default. A search that fits in one batch is unchanged; a sort by key is unaffected.
- **Gauges.** `xapiand_search_pinned_indexes` and `xapiand_search_pinned_indexes_peak` count the indexes batches hold.

## Tests

`tests/test_wide_search.cc` compares a batched search with one search over all the indexes, and with the engine's own
search, for 1, K-1, K, K+1 and 3K+2 indexes (K = 4): hits, ids, order, totals, spy results; match-all and term queries; keyed sorts
with ties; both docid orders; windows; collapse. `harness/wide_search_check.py check` does the same over HTTP between two servers.
