# phrase: journal

State: landed
Quest: .campaign/xapiand/quests/phrase.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:fade84b1e on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phrase.md`: "`build_positional` passes each PHRASE term's query position as its offset" |
| A2 | `docs/status/phrase.md`: "`validate` no longer refuses a PHRASE with a window; no PHRASE refusal remains." |
| A3 | `docs/status/phrase.md`: "Strata matches the terms at exactly those relative positions (Lucene's PhraseQuery)" |
| A4 | `docs/status/phrase.md`: "Mutant (always consecutive) fails `phrase(5, 7)`." |

## Result

Landed at Xapiand:fade84b1e on 2026-10-29. ctest 28 of 28. No review named in the ledger.

- Merge: Merge branch 'phase7a/phrase' (PHRASE at its terms' relative positions).
- Prospect:39d43f5b05c2482c6fc502342647ac8d06048228 adds per-term offsets to `phrase_scorer`.
- Docs E2E: Xapian 566 of 566; strata 566 of 566; 0 regressions, 0 status differences.
- Callbacks check 210 searches, 0 different.
- Open: review; land with prospect branch once CI is green.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
