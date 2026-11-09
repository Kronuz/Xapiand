# more-like-this: ?nearest, ?fuzzy and document terms, back on strata

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status more-like-this`.

## Outcome

Searches find documents like others again. `?nearest` and `?fuzzy`, with their `n_rset`, `n_eset`,
`n_term`, `field` and `type` parameters, work as they did in alpha.10, and the query DSL gains
Elasticsearch's `more_like_this`, which does the same work; `INFO` on a document lists its terms
again, as Elasticsearch's `_termvectors` does. One mechanism serves all three: a document's terms,
frequencies and positions, computed from its stored fields through the schema's analyzers and
weighted by prospect's term statistics (document frequency, document count). Nothing new is stored,
so R-P2 holds (D7). Master ignores both parameters silently (behavior-changes audit #10), which is
why this comes before the release (D40). Repository: Kronuz/Xapiand; a term-statistics call prospect
lacks lands in prospect first, in its own quest.

## Acceptance criteria

From alpha.10's behavior (Xapiand v1.0.0-alpha.10, `src/server/search_views.cc` and
`src/database/handler.cc`), Elasticsearch's `more_like_this` and `_termvectors`, and D40.

- A1 Document terms: for every field type the schema indexes, the terms computed from a stored
  document equal the terms indexing wrote for it, at the same positions: tests in the `build-test`
  check
- A2 `INFO` on a document lists its terms again, each with its frequency, positions and document
  frequency, with a docs example: the `e2e` check
- A3 `more_like_this` in the query DSL with Elasticsearch's parameters and defaults (`like` as
  document ids or text, `unlike`, `fields`, `max_query_terms`, `min_term_freq`, `min_doc_freq`,
  `max_doc_freq`, `min_word_length`, `max_word_length`, `minimum_should_match`, `include`); its
  selected terms equal a brute-force tf-idf ranking over the same statistics: tests in the
  `build-test` check
- A4 `?nearest` and `?fuzzy` with their parameters change the search as alpha.10 did (`?nearest`
  searches with the best `n_eset` terms of the query's top `n_rset` results; `?fuzzy` adds them to
  the query), with docs examples: the `e2e` check
- A5 An A/B against the final Xapian-only release (the `xapiand` branch) on the docs corpora: top-10
  overlap per query for `?nearest` and `?fuzzy`, every difference listed: jobs
- A6 Bounded: the terms a document or a `like` text contributes and the terms a query selects are
  capped, with a 400 past the caps, and analysis stays within the reactor's per-call bound
  (`harness/async_check.sh --strict`): tests in the `build-test` check
- A7 [artifact] How it works, with A5's table: `docs/mechanisms/more-like-this.md`
- A8 [artifact] The changelog records the return of `?nearest`, `?fuzzy` and `INFO`'s terms:
  `docs/src/content/docs/changelog.md`

## Contracts

Reads prospect's term statistics (document frequency, document count), which its scoring already
keeps; adds no format and stores nothing.

## Decisions

- D40: before the release, because master ignores `?nearest` and `?fuzzy` silently.
- R-P2 (D7): no term lists in strata or prospect; the terms come from the stored document.
- `_elite_set` (#7) stays refused: no peer exposes it, and `more_like_this` selects terms the same
  way inside.
- `phase5`'s A4 (relevance-feedback expansion) may cite this quest's tests for Xapiand's path.

## May land early

A1 with its tests.
