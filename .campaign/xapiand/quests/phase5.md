# phase5: Phase 5, analysis and the query language's long tail

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase5`.

## Outcome

Analysis and query parsing cover what Xapian, Lucene and Tantivy users expect: every Snowball
language, CJK n-grams, an ICU plugin and a UAX #29 analyzer; `automata` for wildcard, prefix and fuzzy
queries; the full `queryparser`; and relevance-feedback expansion. The prefix, wildcard, fuzzy and
expansion classes beat Xapian and reach 1.0x of the faster of Lucene and Tantivy, per-language stemmer
vocabularies equal Xapian's, and the queryparser vectors pass. The Xapiand subset of `queryparser`
landed early for 7a (D3, D5); this quest is the rest and the gate. Repository: Kronuz/prospect, with
the libraries' own quests (automata, queryparser, snowball, analysis) ordered before it when they start.

## Acceptance criteria

From strata `docs/PLAN.md`, "Phase 5: analysis and query language long tail" (items and gate).

- A1 All Snowball languages, CJK n-grams, an ICU plugin and a UAX #29 analyzer, each with its tests: tests in jobs
- A2 `automata`: wildcard, prefix and fuzzy, checked against brute force (strata `docs/design/automata.md`, its review's AUT-R1 to AUT-R14): tests in jobs
- A3 `queryparser`: the full grammar beyond Xapiand's 7a subset, step 3b reviewed and landed: the queryparser vectors (T1 to T3) in jobs
- A4 Relevance-feedback expansion: tests in jobs
- A5 Gate: the prefix, wildcard, fuzzy and expansion classes win against Xapian and reach at most 1.0x the faster of Lucene and Tantivy: gate VM jobs
- A6 Gate: per-language stemmer vocabularies equal to Xapian's (the oracle's vectors): jobs
- A7 Gate: `queryparser` vectors pass: jobs
- A8 Every gate: ASan, UBSan, TSan; one CPU-hour of fuzzing for each parser and decoder added or changed (automata, queryparser, analyzers), zero findings; coverage and the phase's test-parity rows: jobs
- A9 [artifact] The gate report, every row with its commit and jobs: `docs/gates/phase-5.md`

## Contracts

Consumes `automata` and `queryparser` as libraries; Xapiand parses host-side for both engines once
`queryparser` passes T4 (owner, 2026-10-26, D5).

## Decisions

- Prior work, not to redo:
  - `queryparser`: Kronuz/queryparser, steps 1 to 3 on its main (queryparser:56d7e08), used by 7a's S4;
    step 3b on branch `step3b` awaits review (T1 528/528, T2 3,482 matched plus 6 excepted of 3,488, T3
    1,995/1,995); its first review REJECTed with fixes awaiting re-check (strata
    `docs/status/queryparser.md`, `docs/reviews/queryparser-step1.md`).
  - `automata`: design revision 1 with AUT-R1 to AUT-R14 folded, preliminary review APPROVE WITH
    CONDITIONS (strata `docs/reviews/automata-design.md`); code on strata `phase5/automata`
    (strata:219ded0d7, 2026-10-18, CI red at 4b9bad4 on expand_test's df premise), written before the
    repository split, so it moves to prospect or its own repository first (PLAN open question 11).
  - Snowball stemmers and CJK n-grams in Kronuz/snowball and Kronuz/analysis since Phase 2 (prospect
    `docs/gates/phase-2.md`: 966 analyzer vectors, snowball fuzzed one CPU-hour).
- Placed after 7c by the owner (2026-10-29, D11): wildcards, fuzzy and partial words inside phrases;
  queryparser phrase wildcards; "more like this" and `min_score` were proposed for after 7c.
- Follows the release (D39), except A6, which `release-rows` proves before it for the languages
  Xapiand serves; this gate re-runs A6 at its own commit.
- D40: "more like this" comes before the release (`more-like-this`, in Xapiand), and A4 may cite its
  tests for Xapiand's path; `_cjk_words` returns on A1's segmenter (`cjk-words`) and words with
  wildcards inside phrases and NEAR on A2's automata (`phrase-expansion`), both after the release.

## May land early

Each library's step with its tests.
