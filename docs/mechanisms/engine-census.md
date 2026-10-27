# Engine census (how it works)

`-DENGINE_CENSUS=ON` makes the engine seam count the facilities each HTTP request uses. Off (the default) every hook is
`((void)0)` and `census.cc` compiles to nothing, so the default binary has no census code.

```
SearchService::handle ---- RequestScope("GET", target) ----+
   |                                                       |  one JSON line when it ends
   v                                                       v
 engine::prepare(query) -> census::query    ->  counters of the one request in flight
 engine::search(...)    -> census::search       (or "unattributed" when none or several are)
 Shard::add/replace/... -> count + census::document (T5)
 XapianDocView::value.. -> census::docview (+ "@spy:Name" / "@keymaker:Name" while a callback runs)
```

Run: `XAPIAND_ENGINE_CENSUS=/path/census.jsonl xapiand ...`. Without the variable the counters are kept and not written.

Files:

1. `src/engine/census.h`: the macros (`ENGINE_CENSUS_COUNT`, `_QUERY`, `_SEARCH`, `_DOCUMENT`, `_DOCVIEW`, `_CALLBACK`, `_REQUEST`, `_STATUS`).
2. `src/engine/census.cc`: counters, the query walk, the key prefix check, the JSON lines.
3. Hook lines in `src/engine/xapian/{search,shard,convert,callbacks}.cc` and `src/server/search_views.cc`; a strata engine adds the same lines.

Attribution: the engine runs on the scheduler's threads, not the request's, so there is no thread-local request. A count
belongs to the request when exactly one is in flight; otherwise it goes to an `unattributed` line, and overlapping requests
are marked `"tainted":true`. The docs suite runs requests one at a time.

Line format: `{"seq","method","target","status","threw","tainted","counts":{key:n},"samples":[...]}`. Counter keys:
`query.*`, `source.<name>`, `search.*`, `callback.<kind>:<name>`, `docview.<method>[@host|@spy:N|@keymaker:N]`,
`shard.<op>`, `write.*`, `t5.*`. Strings are escaped to printable ASCII.

T5: `census::document` runs on every document the shard writes. For each candidate prefix in `KEY_PREFIXES` (`Q`, Xapiand's
id term prefix, and `_id.`, the id field's name prefix) it counts `t5[prefix].*`: documents, documents with no term
leading with it, with one, and violations (a second term, a term that is not the write's key term, a key that does not
lead with it), with up to 8 samples per request. Nothing aborts.

Reading a document: `Document::Native` (under `ENGINE_CENSUS` only) has `census_terms(visit)`, which calls `visit(term, wdf,
positions)` for each term in term order, and `census_values()`; the Xapian document answers from its term list. `census::document`
collects the terms from that visit. Without `ENGINE_CENSUS` neither function exists (`nm` finds no census symbol and `size` shows
the same segments as a build without the census).

Numeric ids: a numeric `_id`'s accuracy terms lead with `A`, not `Q` (`acc_term_prefix`), so for every document exactly one term
leads with `Q`, the id term; the T5 count of documents with a second `Q` term is 0 over the docs suite.

Tests: `tests/test_engine_census.cc`. Report: `docs/dev/engine-census.md`.
