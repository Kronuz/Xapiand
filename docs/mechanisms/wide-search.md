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

- **Patterns.** A pattern is a glob over the segments of an index path (the parts between `/`): `*` and `?` and
  `[...]` match inside one segment and never a `/`, a segment that is `**` stands for any number of segments (none
  included, and any number of `**` in a pattern), and `_all` is `**`. `-x` removes names wherever it stands; a name that
  starts with `.` is not listed. `index_pattern::expand` lists the node's data directory: an index is a directory that holds
  `strata` (one shard) or a shard `.__N`; a directory that holds neither is searched for indexes below it, and so is an
  index (see "Indexes nest"). The names a pattern may expand to are counted as the walk goes, so a pattern over a million
  directories stops at the cap instead of listing them. The walk is blocking file-system work, so it runs as a `query` chore
  of the work pool, never on a loop. Plain names are not looked up: a missing one is skipped by the search, as before.
- **A pattern is an automaton over segments.** `index_pattern::Glob` makes a position of each segment of the pattern (a run
  of `**` is one) and a set of live positions of the path read so far: a segment moves each live position to the next if it
  matches the segment (a `**` stays and may be skipped). The set is a bitset carried down the walk with each directory, so an
  entry costs one step of each pattern, O(the pattern's segments), however many `**` it holds and however deep the tree is.
  Matching inside a segment is the two-pointer match (the last `*` to retry from, `?` and a class one character each), at most
  the segment's length times the name's, and every position tried and byte walked is a step against the steps bound. A
  directory is entered only while a wanted pattern is live in its set, so a pattern with no `**` reads the directories its
  segments name and no others: `logs/2026/*` reads the top, `logs` and `logs/2026`. A directory is listed when a wanted pattern
  accepts and no exclusion does, and the directory is an index.
- **One walk, a budget of entries.** All the patterns of a request are found in one pass: each directory entry is tested
  against every wanted pattern and every exclusion, and the directory is read entry by entry (not listed first). Every
  entry read, in a directory the walk lists or inside an index directory, at any depth, counts against `--max-search-indexes`,
  matching or not (an index costs its own entry and the entries of its directory: `strata` or its shards, and its children); past it the request answers 400. The count is of the work, so an exclusion that removes everything (`*,-*`)
  or a pattern that matches nothing still pays for the tree. One walk runs at a time on the node; the others wait their turn without a thread, at most `--search-walk-queue` of them and each for `--search-walk-wait` at most (`index_pattern::WalkGate`, below). Before each entry the walk calls the request's cancel token
  (`Request::cancel`), so a client that goes away stops it. Equal items are one item.
- **Indexes nest.** `twitter` and `twitter/user` can both exist, each a directory, the child inside the parent (owner
  ruling 2026-11-03: nested indexes are the heart of Xapiand). The walk goes into an index directory like any other and
  tells a child from the parent's own files by the engine's markers: in an index directory, `strata` (the engine's data
  directory of the index) and the shard directories `.__N` are the index itself and are never listed or entered; any other
  directory is a child, listed when it holds `strata` or a `.__N` itself and searched for children whatever it holds. So
  creating `tenant` never hides `tenant/a` from `_all` or a prefix pattern (the attack the review found, SR2, was a
  walk that stopped at an index directory). Names that start with `.` are never entered, nested or not. One name stays
  invisible to patterns: a child called `strata`, whose path in the parent's directory is the engine's own; it is found by
  its name.
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
| Directory entries the patterns read | the same number | `index_pattern::Walk` counts every entry it reads, at every depth and inside an index directory, matching or not; it goes only into directories a wanted pattern is live in; 400 past it | the work of a walk must have a number whatever the exclusions say |
| Steps of matching | 4,096 for each entry the cap allows (`index_pattern::steps_per_visit`) | the matcher counts a step for each byte of a name it walks, backtracking included, and the walk answers 400 past the total | a pattern that backtracks over a long name costs its size times the name's; 32 of them on every entry of a big tree would hold a work-pool thread for tens of seconds |
| Walks at once | 1 per node | `index_pattern::WalkGate` holds a FIFO `task::async_mutex` (cancellable) taken before the chore; a waiting request holds no thread | however many clients send walks, they hold one worker of the work pool, so the chores of writes and opens run beside them |
| Walks waiting | `--search-walk-queue` (8, at least 0) | the gate counts the walk that runs and those that wait; a walk that finds the queue full answers 429 with `Retry-After: 1` at once, taking no place | the queue must have a length: the wait of an honest search is the walks ahead of it times a walk (1.4 s warm and 5 s cold at the default cap), so an unbounded queue let 40 held connections hold a wildcard search for 56 s and 150 for 240 s (SR8) |
| Wait for a walk | `--search-walk-wait` (10 s, above 0) | the wait for the mutex runs under a `task::deadline`; a walk that waits that long gives up its place and answers 503 with `Retry-After: 1` | the wait is bounded whatever the walks ahead do or how fast they run; the walk that runs is never interrupted by it, only the work of reading is (the cap above) |
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
`tests/fuzz_index_pattern.cc` the glob (segments, `*`, `?`, classes, `**`) and its automaton against a reference that tries every way,
`tests/fuzz_index_expand.cc` the expansion against a brute-force oracle on a fixed tree of nested indexes. `harness/wide_search_check.py check` does the comparison over HTTP between two servers,
and expands patterns and the cap over a live node.
