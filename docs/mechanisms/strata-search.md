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
   one prospect::multi_snapshot = the segments of all shards (shard i = the i-th shard asked for)
   term_table over its segments: global statistics and document frequencies
   prospect::search(snapshot, build, options):  pruned top-k per shard merged by weight then interleaved docid,
   or, with a spy or descending ids, the exhaustive collector (every live match once, in a heap of k)
```

1. **One snapshot over the shards.** The segments of all shards go into one `term_table`, so a term's weight
   uses the whole index's document count, total length and document frequency, as Xapian's multi-database
   does. Deleted documents count in those statistics until a merge drops them (a prospect rule), where
   Xapian excludes them: weights drift slightly after deletes and replacements (T7).
2. **Ids.** A hit's id is Xapian's interleave, `(did - 1) * n + shard + 1`, for `n` shards in the order given
   (`multi_snapshot`, prospect's `shards.hh`). Ties in weight go to the lower id (the higher with
   `DocidOrder::descending`).
3. **Estimate.** The count of live matches, exact (G12), from prospect's search with `count`; the exhaustive
   collector counts as it visits. `check_at_least` is not needed.
4. **Refresh before search.** `Shard::pin()` flushes the write buffer if documents were applied since the
   last flush (D7), then holds the shard's lock for the search. A write, merge or close waits for it.
   Indexes are pinned in address order, so searches that share indexes cannot deadlock.
5. **Scorers.** A term is `term_scorer` with `weight::bm25_xapian(statistics, df, wqf)`; AND is
   `conjunction_scorer`, OR `disjunction_scorer`, AND_NOT `exclusion_scorer`, AND_MAYBE
   `required_optional_scorer`, FILTER an AND with the right side scaled by 0 (`scale_scorer`, prospect's
   `operators.hh`); PHRASE is a `phrase_scorer` over consecutive positions (window equal to the term count)
   and NEAR the same with a window. A scorer is null where a segment cannot match, and the operators treat a
   null as Xapian treats a subquery that matches nothing.
6. **Collection.** `prospect::search` keeps the `first + maxitems` best: the pruned collector for ascending ids
   without a spy, the exhaustive one (which scores every match) for a spy or `DocidOrder::descending`. A spy
   is the exhaustive collector's visitor and is shown each live match once, whatever the window.
   `percent` is the weight over the best weight, `rank` the position in the full order.
7. **One snapshot.** A search holds every pinned shard's lock from the first pin to the end, so the documents
   and statistics it reads are the index as of the last pin: no write, commit or merge lands in between.

## Tests

`tests/test_engine_search_strata.cc` writes one random corpus to a Xapian and a strata index, one, two and
five shards, in several commits (several segments), and runs random query trees through `engine::search` on
both. Each result is checked against a model of the corpus (documents and estimate exact) and against the
Xapian result (the same ranking up to ties, weights within a relative 1e-9).

`test_snapshot_consistency` searches (in order, in reverse with descending ids, and with a spy) while a
writer adds a document to shard 0, 1 and 2 in turn and commits: every search must find per-shard counts that
existed at one moment, and weights equal to a model's for exactly that many documents. The refusal,
long-chain (weights and order against the model), spy (ids, data and weights per match) and snapshot tests
each have a mutant that fails them (`docs/status/strata-engine-search.md`).
