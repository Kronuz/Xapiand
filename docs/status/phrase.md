# Status: strata PHRASE with positions (phase7a/phrase)

## State
Done; waiting on review. Prospect branch `phase8/phrase-positions` (prospect:39d43f5b05c2482c6fc502342647ac8d06048228) adds per-term offsets to `phrase_scorer`.

## Done
- `build_positional` passes each PHRASE term's query position as its offset (relative to the first; gaps where stop words stood). Positions that do not strictly rise leave the terms consecutive.
- `validate` no longer refuses a PHRASE with a window; no PHRASE refusal remains.
- Strata matches the terms at exactly those relative positions (Lucene's PhraseQuery), not Xapian's ordered-within-a-window.
- Tests: strata-only gap cases in `test_fields_impl` (Q4); the window-5 refusal check became an accepted search. Mutant (always consecutive) fails `phrase(5, 7)`.

## Proof (Release, no LTO, prospect pinned to 39d43f5b)
- ctest 28 of 28; docs E2E (newman 6.2.3) Xapian 566 of 566, strata 566 of 566, 0 regressions, 0 status differences; callbacks check 210 searches, 0 different.

## Next
Review; land with the prospect branch once its CI is green.

## Blocked on
Nothing.
