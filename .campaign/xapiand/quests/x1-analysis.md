# x1-analysis: X1, index-time analysis without Xapian's term generator

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:0cf555fac (2026-10-20): step X1 of `docs/design/xapiand-on-strata.md` section 4, the first step of Phase 0.5, with the code at Xapiand:fe9307227 and its extraction-ledger row at Xapiand:0cf555fac. It had no ledger: `docs/dev/extraction-ledger.md` is the record. Its machine state is in `state.json`: `campaign status x1-analysis`.

## Outcome

Xapiand indexes text with `analysis::term_generator`, `snowball::stemmer` and `analysis::stopper` instead of Xapian's term generator, stemmer and stopper, so the document side of term generation no longer calls Xapian. The docs E2E is unchanged and the generated terms equal Xapian's but for documented stemmer and stop-list differences; the `cjk_words` path and the query side stay on Xapian until `queryparser` replaces them.

## Acceptance criteria

Carried from `docs/design/xapiand-on-strata.md` section 4, step X1's "Done when", each shown by the extraction ledger's row as an artifact.

- A1 [artifact] `GenerateTerms::text` uses `analysis::term_generator` and `snowball`: `docs/dev/extraction-ledger.md`
- A2 [artifact] `analysis::stopper` is built from the word lists behind `getStopper`; the schema's stem and stop strategies map onto it: `docs/dev/extraction-ledger.md`
- A3 [artifact] A baseline E2E is captured at the branch point, and a T2-style diff has zero regressions: `docs/dev/extraction-ledger.md`

## Contracts

None recorded: landed work; the analysis path is in `src/multivalue/generate_terms.cc` and `src/stopper.cc`.

## Decisions

- Kept on Xapian (the ledger row): the `cjk_words` path under `USE_ICU` (analysis has no word breaks); `Xapian::Stem` and the `Xapian::Stopper` adapter for the query side until `queryparser` replaces `Xapian::QueryParser` (S4).
- Risk normal: the terms every indexed document gets (this audit's mapping).

## May land early

None.
