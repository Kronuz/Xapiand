# Searching the strata engine: how it works

## What

`engine::search` (`src/engine/search.cc`) sends a search to the engine that owns the shards: Xapian's
`Enquire`, or `engine::strata::search` (`src/engine/strata/search.cc`), which runs the query over the
segments of every shard as one index. The strata side covers terms, `match_all`, AND, OR, AND_NOT,
AND_MAYBE, FILTER, SCALE_WEIGHT, PHRASE and NEAR over terms and over alternatives, append chains, sorting,
collapse, aggregations through the spy, wildcard and edit-distance expansions, posting sources and query text.
Every shape a `Query` factory builds is searched: a shape that is not a valid query is refused when the query is
built (`src/engine/query.h`), by both engines, and the Xapian engine alone does not search a PHRASE or NEAR over
alternatives.

## How

```
engine::prepare(Query) -> Prepared (Xapian query + the neutral Query, PreparedQuery::source())
engine::search(shards, request)
   all Xapian -> xapian::search          all strata -> strata::search       mixed -> errc::invalid
strata::search
   hold one view of each distinct index (Shard::view()): its segments and field table, as of the last refresh
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
4. **Refresh before search.** A host refreshes an index when documents were applied since the last refresh
   (`engine_for_read`, D7), and the search then holds `Shard::view()`, the view that refresh published
   (`docs/mechanisms/reader-views.md`). Nothing waits for the search: a write, refresh, commit or merge goes on.
5. **Scorers.** A term is `term_scorer` with `weight::bm25_xapian(statistics, df, wqf)`; AND is
   `conjunction_scorer`, OR `disjunction_scorer`, AND_NOT `exclusion_scorer`, AND_MAYBE
   `required_optional_scorer`, FILTER an AND with the right side scaled by 0 (`scale_scorer`, prospect's
   `operators.hh`); PHRASE is a `phrase_scorer` over consecutive positions (window equal to the term count)
   and NEAR the same with a window. A PHRASE or NEAR with a position that holds alternatives (a wildcard or
   edit-distance expansion, an OR or SYNONYM of words) is a `multi_phrase_scorer`: each position is the scorer
   of its alternatives, and its terms' positions are merged to find the phrase (the positions are consecutive,
   as an expansion carries none of its own); a document scores the positions' scores summed. The query text
   parser (`queryparser`) takes only words as the operands of NEAR and PHRASE, so only a `Query` built by its
   factories reaches this scorer. A scorer is null where a segment cannot match, and the operators treat a
   null as Xapian treats a subquery that matches nothing.
6. **Collection.** `prospect::search` keeps the `first + maxitems` best: the pruned collector for ascending ids
   without a spy, the exhaustive one (which scores every match) for a spy or `DocidOrder::descending`. A spy
   is the exhaustive collector's visitor and is shown each live match once, whatever the window.
   `percent` is the weight over the best weight, `rank` the position in the full order.
7. **Collapse.** With `collapse_key` and `collapse_max` set, the search visits every match and offers it to
   prospect's `collapse::collapser`, keyed by the slot's value as the host wrote it (a document without the
   value is never collapsed). Hits rank by sorter key, then weight, then id in the request's order, as in
   Xapian; the window applies after the collapse and the estimate is the number of hits kept. The collapser
   holds one entry per distinct value, so a collapse on a field with millions of values holds millions of
   entries for the length of the search.
8. **One snapshot per shard.** A search holds a view of each shard from the start to the end, so the documents
   and statistics it reads are those the views hold, whole; a write that lands meanwhile is not seen. The views are
   taken one after the other, so two shards may be at different moments.

## Tests

`tests/test_engine_search_strata.cc` writes one random corpus to a Xapian and a strata index, one, two and
five shards, in several commits (several segments), and runs random query trees through `engine::search` on
both. Each result is checked against a model of the corpus (documents and estimate exact) and against the
Xapian result (the same ranking up to ties, weights within a relative 1e-9).

`test_collapse` runs the same collapsed searches on both engines (one, two and five shards, `collapse_max` 1 to 3,
both id orders, with and without a sorter, five windows, a spy) and requires the same hits, order, ranks,
percents, estimate and spied documents.

`test_snapshot_consistency` searches (in order, in reverse with descending ids, and with a spy) while a
writer adds a document to shard 0, 1 and 2 in turn and commits: every search must find per-shard counts that
existed at one moment, and weights equal to a model's for exactly that many documents. The refusal,
long-chain (weights and order against the model), spy (ids, data and weights per match) and snapshot tests
each have a mutant that fails them (`docs/status/strata-engine-search.md`).
