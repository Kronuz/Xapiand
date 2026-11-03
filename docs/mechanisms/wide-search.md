> Status: DRAFT (2026-11-02) a search over many indexes as built on `phase7c/wide-search`; the owner's plan is `strata/docs/status/road-to-release.md`, "Wide searches". Not reviewed.

# Wide search: indexes in batches

## What

A search that names many indexes (a wildcard, a list, `_all`) holds at most K shards open at once. It searches
a batch of K, merges that batch's best hits into the top of the whole, releases the batch, and takes the next. Before this
a search kept every index it named open until it ended, so its width was the pool size (200 by default).

## How

```
search_view / count_view
  expand_paths                 `_all`, `test*`, `-test3` against the node's directories (a query chore); counted against the cap
  resolve_index_endpoints      each name to its shards; sorted, no repeats
DatabaseHandler::match
  check_search_fanout          runaway cap (--max-search-indexes, 100,000): 400
  engine::search_batched       walks batches of whole indexes, at most K shards; merges
    BatchRunner (the seam)     DatabaseHandler::search_batch: pin [begin, end), refresh each, engine::search
      strata::run              one term table and plan per index
```

- **Patterns.** `index_pattern::expand` lists the node's data directory: an index is a directory that holds `strata`
  (one shard) or a shard `.__N`; a directory that holds neither is searched for indexes below it. `*` matches any bytes,
  `/` included; `_all` is `*`; `-x` removes names wherever it stands; a name that starts with `.` is not listed. The
  names a pattern may expand to are counted as the walk goes, so a pattern over a million directories stops at the cap
  instead of listing them. The walk is blocking file-system work, so it runs as a `query` chore of the work pool, never on
  a loop. Plain names are not looked up: a missing one is skipped by the search, as before.
- **One walk, a budget of entries.** All the patterns of a request are found in one pass: each directory entry is tested
  against every wanted pattern and every exclusion, and the directory is read entry by entry (not listed first). Every
  entry read, in a directory the walk lists or inside an index directory it probes, counts against `--max-search-indexes`,
  matching or not; past it the request answers 400. The count is of the work, so an exclusion that removes everything (`*,-*`)
  or a pattern that matches nothing still pays for the tree. Before each entry the walk calls the request's cancel token
  (`Request::cancel`), so a client that goes away stops it. Equal items are one item.
- **Nothing nests.** The walk does not look inside an index, so an index path is never above or below another's:
  `PUT /tenant/d` while `tenant/a` is an index, `PUT /a/b/d` while `a` is one, and a name below the first that the layout
  keeps (`strata`, `.__N`) are refused with 400 when the index is created (`index_pattern::check_new_index`, run in a query
  chore before the settings of a new index are saved). The check reads this node's directories, as the walk does, and two
  creations racing each other can both pass it. Indexes made before the rule can be nested; a pattern does not find the
  inner one (a name does).
- **The seam.** `engine::BatchRunner` is the one place that decides how a batch's indexes are held: today
  `DatabaseHandler::search_batch` takes a `lock_shard` per endpoint (a use of the registry entry plus one view per index,
  taken by the search) and searches them with the reopen-and-retry loop. When the cold-tier catalog's wake lands, a batch pins
  its indexes through the catalog instead; only this function changes.
- **K.** `--concurrent-search-indexes`, else a reactor's share of the open indexes: the pool size over the HTTP loops, at
  least 1 (`search_batch_indexes`). `main()` caps it at the pool size. A batch holds whole indexes, so an index with more
  shards than K is a batch of its own.
- **Why that K.** The design's reader cap (`readers_per_index`, 16 searches per reactor) bounds the readers of one index,
  not the indexes of a search, and a batch takes one record per index it holds, so K does not touch it. What K bounds is the
  indexes pinned: one wide search per loop holds at most pool size / loops each, and the loops together hold the pool's
  budget, the number the file-descriptor limit sizes. More wide searches on a loop than one add their batches; the pool
  is a soft budget (an entry in use is never evicted, an overflowing idle one goes after 60 s), so that is a larger
  working set, not a failure, and `xapiand_search_pinned_indexes_peak` shows it.
- **No cross-index snapshot.** Each index is read as its batch starts, so the refresh that makes a read see the writes
  acknowledged before it happens per index at that moment. Per-index read-your-writes holds; two indexes are not read at the same instant.
- **Ids.** Hit ids number the documents across all the endpoints, `(d - 1) * total + position + 1`, as `get_document`
  reads them back (`split_hit_id`). They are 64 bits (`engine::hitid`); a shard's own docid stays 32. A batch numbers its
  own shards, so the merge renumbers by position. A shard that is missing (no index) takes no documents but keeps its slot.
  A single batch is renumbered the same way when a shard is missing.
- **Merge.** Each batch is asked for the top `first + maxitems` hits with their sort keys (`SearchRequest::report_keys`;
  the strata engine recomputes the key of the hits it returns, at most a window), and the merge ranks by key, then weight,
  then id, as one search does. Under a collapse each batch returns all it kept (with the collapse value), and the merge keeps the best
  `collapse_max` per value; the total is then the kept count. Otherwise the total is the sum of the batch counts. The spy is
  shared by the batches, so it sees every match once, in shard order. `percent` is recomputed against the heaviest weight of all batches.
- **Weights.** A relevance weight reads the statistics of the document's own index, as Elasticsearch reads a shard's by
  default: `SearchRequest::index_of` names the index of each shard, and `strata::run` builds one term table and plan per
  index over its segments (prospect's `term_table` takes any span of segments, so the batch's composition does not enter).
  The shards of one index share them, as they always did for one index. A document's weight, and so the order, depends on its
  index only, whatever else is searched with it and whatever K is. The cost is the same as before: a term is looked up in
  the segments of its own index instead of every segment of the batch.
- **Gauges.** `xapiand_search_pinned_indexes` and `xapiand_search_pinned_indexes_peak` count the indexes batches hold.
- **The cap is reachable.** A URL holds 64 KiB, a few thousand names, so a list of names cannot reach 100,000. A pattern
  can: `_all` or `wi*` is a few bytes whatever it expands to. An index list in the request body would be a second way to
  say what the path says, with a rule for the two together, and the Elasticsearch API that this follows takes indices in the
  path and as patterns only; so the request does not take one.

## Limits

| Limit | Value | Mechanism | Reason |
|---|---|---|---|
| Names a search reads | `--max-search-indexes` (100,000, at least 1) | the names are counted as the walk finds them, plain names first; 400 past it | a runaway `_all` over more indexes than a node should serve in one request |
| Directory entries the patterns read | the same number | `index_pattern::Walk` counts every entry it reads, matching or not; 400 past it | the work of a walk must have a number whatever the exclusions say |
| Steps of matching | 4,096 for each entry the cap allows (`index_pattern::steps_per_visit`) | the matcher counts a step for each byte of a name it walks, backtracking included, and the walk answers 400 past the total | a pattern that backtracks over a long name costs its size times the name's; 32 of them on every entry of a big tree would hold a work-pool thread for tens of seconds |
| Patterns in a request | 32 (`index_pattern::max_patterns`) | wildcards and exclusions together, after equal items are merged; 400 above | every entry is tested against every pattern, so patterns times entries is the match work |
| Length of a pattern | 256 bytes (`max_pattern_size`) | 400 above | the matcher's cost is the pattern's length times the name's |
| Plain names in a request | the names bound | they count as names | a plain name costs nothing to find, and the URL (64 KiB) bounds the list |

## Tests

`tests/test_wide_search.cc` compares a batched search with one search over all the shards, and with the engine's own
search, for 1, K-1, K, K+1 and 3K+2 shards (K = 4) and again for every K over the same 3K+2 (1, K-1, K, K+1, 3K+2, all at
once): hits, ids, order, weights, percents, totals, spy results; match-all, term, OR, AND, wildcard and synonym queries;
keyed sorts with ties; both docid orders; windows; collapse. Nine indexes of one to three shards differ in size and
vocabulary, a check that each document weighs as in a search of its own index alone, and a count of the queries whose
weights change when the indexes are weighed as one. Ids are checked at the old 32-bit edge (42,949 documents of each of
100,000 shards) and decoded back. `tests/test_index_pattern.cc` covers the glob and the expansion against a directory tree,
`tests/fuzz_index_pattern.cc` the glob against a reference, `tests/fuzz_index_expand.cc` the expansion and the nesting check against
a brute-force oracle on a fixed tree. `harness/wide_search_check.py check` does the comparison over HTTP between two servers,
and expands patterns and the cap over a live node.
