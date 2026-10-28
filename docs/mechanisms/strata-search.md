# Searching the strata engine: how it works

## What

`engine::search` (`src/engine/search.cc`) sends a search to the engine that owns the shards: Xapian's
`Enquire`, or `engine::strata::search` (`src/engine/strata/search.cc`), which runs the query over the
segments of every shard as one index. The strata side covers terms, `match_all`, AND, OR, AND_NOT,
AND_MAYBE, FILTER, SCALE_WEIGHT, PHRASE and NEAR over terms, and append chains. Sorting, collapse,
aggregations, similar documents, cutoffs, the other operators, expansions, posting sources and query text
are refused with a client error that names them (`errc::syntax`); they arrive with S3 and S4.

## How

```
engine::prepare(Query) -> Prepared (Xapian query + the neutral Query, PreparedQuery::source())
engine::search(shards, request)
   all Xapian -> xapian::search          all strata -> strata::search       mixed -> errc::invalid
strata::search
   pin each distinct index once, in address order:  flush the write buffer, lock, segments()
   one snapshot = the segments of all shards:  term_table (global statistics and document frequencies)
   per segment:  build scorers from the Query  ->  count (estimate)  ->  top-k with pruning
   merge into one heap:  weight, then interleaved docid
```

1. **One snapshot over the shards.** The segments of all shards go into one `term_table`, so a term's weight
   uses the whole index's document count, total length and document frequency, as Xapian's multi-database
   does. Deleted documents count in those statistics until a merge drops them (a prospect rule), where
   Xapian excludes them: weights drift slightly after deletes and replacements (T7).
2. **Ids.** A hit's id is Xapian's interleave, `(did - 1) * n + shard + 1`, for `n` shards in the order given.
   Ties in weight go to the lower id (the higher with `DocidOrder::descending`).
3. **Estimate.** The count of live matches, exact (G12): the segment's scorer is counted in bit windows,
   wrapped in `live_scorer` where documents are deleted. `check_at_least` is not needed.
4. **Refresh before search.** `Shard::pin()` flushes the write buffer if documents were applied since the
   last flush (D7), then holds the shard's lock for the search. A write, merge or close waits for it.
   Indexes are pinned in address order, so searches that share indexes cannot deadlock.
5. **Scorers.** A term is `term_scorer` with `weight::bm25_xapian(statistics, df, wqf)`; AND is
   `conjunction_scorer`, OR `disjunction_scorer`, AND_NOT `exclusion_scorer`, AND_MAYBE
   `required_optional_scorer`, FILTER an AND with the right side scaled by 0 (`scale_scorer`, prospect's
   `operators.hh`); PHRASE is a `phrase_scorer` over consecutive positions (window equal to the term count)
   and NEAR the same with a window. A scorer is null where a segment cannot match, and the operators treat a
   null as Xapian treats a subquery that matches nothing.
6. **Top-k.** Each segment feeds one heap of `first + maxitems` candidates; once it is full the scorer is
   told to skip what scores below the k-th (one ulp under it, so a tie with a lower id can still enter).
   `percent` is the weight over the best weight, `rank` the position in the full order.

## Tests

`tests/test_engine_search_strata.cc` writes one random corpus to a Xapian and a strata index, one, two and
five shards, in several commits (several segments), and runs random query trees through `engine::search` on
both. Each result is checked against a model of the corpus (documents and estimate exact) and against the
Xapian result (the same ranking up to ties, weights within a relative 1e-9).
