# more-like-this: ?nearest, ?fuzzy and document terms, back on strata

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status more-like-this`.

Whole feature (replan-2026-11-20, section 1 F7); before the release (D40).

## Outcome

Searches find documents like others again. `?nearest` and `?fuzzy`, with their `n_rset`, `n_eset`,
`n_term`, `field` and `type` parameters, work as they did in alpha.10, and the query DSL gains
Elasticsearch's `more_like_this`, which does the same work; `INFO` on a document lists its terms
again, as Elasticsearch's `_termvectors` does. One mechanism serves all three: a document's terms,
frequencies and positions, computed from its stored fields through the schema's analyzers and
weighted by prospect's term statistics (document frequency, document count). Nothing new is stored,
so R-P2 holds (D7). Master ignores both parameters silently (behavior-changes audit #10), which is
why this comes before the release (D40). A term-statistics call prospect lacks lands in prospect
first, in its own quest.

## Acceptance criteria

From alpha.10's behavior (`src/server/search_views.cc` and `src/database/handler.cc`),
Elasticsearch's `more_like_this` and `_termvectors`, and D40. Gate: the suite (`build-test`, `e2e`).

- A1 For every field type the schema indexes, the terms computed from a stored document equal the
  terms indexing wrote for it, at the same positions: tests in the `build-test` check
- A2 `INFO` on a document lists each term with its frequency, positions and document frequency: a
  docs E2E example, the `e2e` check
- A3 `more_like_this` in the query DSL takes Elasticsearch's parameters and defaults (`like` as
  document ids or text, `unlike`, `fields`, `max_query_terms`, `min_term_freq`, `min_doc_freq`,
  `max_doc_freq`, `min_word_length`, `max_word_length`, `minimum_should_match`, `include`), and its
  selected terms equal a brute-force tf-idf ranking over the same statistics: tests in the
  `build-test` check
- A4 `?nearest` and `?fuzzy` with `n_rset`, `n_eset`, `n_term`, `field` and `type` change the search
  as alpha.10 did (`?nearest` searches with the best `n_eset` terms of the query's top `n_rset`
  results; `?fuzzy` adds them to the query): docs E2E examples, the `e2e` check
- A5 Terms per document and per query are capped with a 400 past the caps, and analysis stays inside
  the reactor's per-call bound (`harness/async_check.sh --strict`): tests in the `build-test` check
- A6 [artifact] `docs/mechanisms/more-like-this.md`, and the changelog records the return of
  `?nearest`, `?fuzzy` and `INFO`'s terms: `docs/src/content/docs/changelog.md`

## Tasks

One agent each, merged into the quest branch on its own tests. Economical, medium (normal risk).
Before starting, the lead narrows `src/**` and `harness/**` to the files the feature touches (the
query DSL, the search and info views, the handler, a new `src/more_like_this.*`): today `src/**`
holds every Xapiand quest.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `terms` | `src/more_like_this.*`, `tests/test_more_like_this*`, the info view | implementation, economical, medium | Terms of a stored document through the schema's analyzers, weighted by prospect's statistics; `INFO`'s term list (A1, A2, A5's per-document cap) |
| 2 | `mlt` | the query DSL, `tests/test_more_like_this_dsl*` | implementation, economical, medium | `more_like_this` and the brute-force tf-idf check (A3, A5's per-query cap) |
| 2 | `nearest` | `src/server/search_views.cc`, the handler, `harness/**` examples | implementation, economical, medium | `?nearest`, `?fuzzy` and their parameters (A4) |
| 3 | `docs` | `docs/mechanisms/more-like-this.md`, `docs/src/content/docs/**` | documentation, economical, medium | The note and the changelog (A6) |

## Contracts

Reads prospect's term statistics, which its scoring already keeps; adds no format and stores nothing.

## Decisions

- D40: before the release, because master ignores `?nearest` and `?fuzzy` silently.
- R-P2 (D7): no term lists in strata or prospect; the terms come from the stored document.
- `_elite_set` (#7) stays refused: no peer exposes it, and `more_like_this` selects terms the same
  way inside.
- `phase5`'s relevance-feedback expansion (parked) may cite this quest's tests for Xapiand's path.

## When done

Normal risk: the lead reads the diff (the security question is A5's caps, a test). Runs once, before
landing. One light job: top-10 overlap per query for `?nearest` and `?fuzzy` against the final
Xapian-only release on the docs corpora, every difference listed in
`docs/mechanisms/more-like-this.md` (needs the `xapiand` branch's binary, `xapiand-final-release`).

## May land early

`terms` with its tests (internal code behind tests).
