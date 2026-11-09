# phrase-expansion: Wildcards, prefixes and fuzzy words inside phrases and NEAR

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phrase-expansion`.

## Outcome

Phrases and NEAR take words with wildcards, prefixes and edit distances: each such word expands over
the term dictionary through `automata`, and the phrase matches any expansion at its position, as
Elasticsearch's `intervals` (prefix, wildcard and fuzzy rules) and `match_phrase_prefix`, and
Tantivy's and Quickwit's phrase prefix queries do. Xapiand's half, the queryparser's phrase wildcards
and `_synonym` over a wildcard, a fuzzy word or a group (refused on master, behavior-changes audit
#15), follows in a Xapiand quest the lead opens when this lands. Repository: Kronuz/prospect.

## Acceptance criteria

From strata `docs/PLAN.md`, Phase 5, and D40.

- A1 Phrases and NEAR over expanded words equal a brute-force evaluation, position by position, on
  random corpora and every segment shape `automata`'s tests name: tests in the `build-test` check
- A2 Expansion is bounded per word and per query (Elasticsearch's `max_expansions`), with a clear
  error past the bound: tests in the `build-test` check
- A3 One CPU-hour of fuzzing of the new query shapes with zero findings; ASan and UBSan builds pass
  the tests: jobs and the `asan` check
- A4 The class measured against Xapian and against the faster of Lucene and Tantivy (Phase 5's
  expansion rows), on the gate's machine: jobs
- A5 [artifact] How it works: `docs/mechanisms/phrase-expansion.md`

## Contracts

Consumes `prospect:term-expansion` (`automata`).

## Decisions

- D40: after the release; master refuses these shapes with a 400, so nothing changes silently.
- Needs `automata`.

## May land early

None.
