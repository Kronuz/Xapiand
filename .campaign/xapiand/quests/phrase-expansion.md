# phrase-expansion: Wildcards, prefixes and fuzzy words inside phrases and NEAR

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phrase-expansion`.

Whole feature (replan-2026-11-20, section 1 F8), in two quests because they are two repositories:
this one in Kronuz/prospect, and a Xapiand quest for its half, which the lead opens when this lands.

## Outcome

Phrases and NEAR take words with wildcards, prefixes and edit distances: each such word expands over
the term dictionary through `automata`, and the phrase matches any expansion at its position, as
Elasticsearch's `intervals` (prefix, wildcard and fuzzy rules) and `match_phrase_prefix`, and
Tantivy's and Quickwit's phrase prefix queries do. Xapiand's half, the queryparser's phrase wildcards
and `_synonym` over a wildcard, a fuzzy word or a group (refused on master, behavior-changes audit
#15), follows in the Xapiand quest (its criterion: phrase wildcards in query strings and `_synonym`
over expanded words are accepted and return what prospect's evaluation returns, docs E2E examples).

## Acceptance criteria

From strata `docs/PLAN.md`, Phase 5, and D40. Gate: the suite (`build-test`) and CI (`asan`).

- A1 Phrases and NEAR over expanded words equal a brute-force evaluation, position by position, on
  random corpora and every segment shape `automata`'s tests name: tests in the `build-test` check
- A2 Expansion is bounded per word and per query (`max_expansions`), with a clear error past the
  bound: tests in the `build-test` check
- A3 [artifact] How it works: `docs/mechanisms/phrase-expansion.md`

## Tasks

One agent each, merged into the quest branch on its own tests. Economical, medium (normal risk);
needs `automata` landed (active in prospect).

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `phrase` | the phrase evaluation under `include/**`, `test/phrase_expansion*`, `bench/**` | implementation, economical, medium | Expansion of a word at its position in a phrase through `automata` (A1) |
| 2 | `near` | the NEAR evaluation, `test/near_expansion*` | implementation, economical, medium | The same for NEAR (A1), on `phrase`'s contract |
| 2 | `bounds` | `test/expansion_bounds*`, `fuzz/**` | implementation, economical, medium | `max_expansions` per word and per query, the error past it (A2); the fuzz target for the new shapes |
| 3 | `note` | `docs/mechanisms/phrase-expansion.md`, `docs/perf/**` | documentation, economical, medium | The note (A3) |

## Contracts

Consumes `prospect:term-expansion` (`automata`).

## Decisions

- D40: after the release; master refuses these shapes with a 400, so nothing changes silently. It
  shares no file with the cluster path, so the lead may start it when `automata` lands and a slot is
  free (D39 lets the lead move a row before the release).
- Needs `automata`.

## When done

Normal risk: the lead reads the diff. Runs once, before landing: `--later` one CPU-hour of fuzzing of
the new query shapes with zero findings (ASan and UBSan builds pass the tests). The expansion class
against Xapian and the faster of Lucene and Tantivy runs with Phase 5's rows (parked), on the gate's
machine.

## May land early

None.
