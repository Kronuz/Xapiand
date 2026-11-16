# phrase: Positional phrase matching

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:fade84b1e (2026-10-29); its ledger `docs/status/phrase.md` is the record. Its machine state is in `state.json`: `campaign status phrase`.

## Outcome

Strata PHRASE queries use each term's query position as an offset, preserving gaps such as stop words. The phrase window is no longer refused; matching requires those exact relative positions.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `build_positional` passes each PHRASE term's query position as its offset; non-rising positions remain consecutive: `docs/status/phrase.md`
- A2 [artifact] `validate` no longer refuses a PHRASE with a window; no PHRASE refusal remains: `docs/status/phrase.md`
- A3 [artifact] Strata matches terms at exactly their relative positions, not Xapian's ordered-within-a-window behavior: `docs/status/phrase.md`
- A4 [artifact] Tests cover strata-only gap cases and the window-5 search; consecutive-only mutant fails `phrase(5, 7)`: `docs/status/phrase.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Strata positional phrase matching follows Lucene's PhraseQuery, not Xapian's ordered-within-a-window semantics.
- Risk normal: changes phrase-query matching behavior (the lead's mapping).

## May land early

None.
