# prospect-phrase-alternatives: PHRASE and NEAR over alternatives

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:b3ad3e3 (2026-10-29); its ledger `docs/status/phrase-alternatives.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-phrase-alternatives`.

## Outcome

`multi_phrase_scorer` matches a phrase whose positions hold alternatives, for Xapiand's gate 7a L1.

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] `multi_phrase_scorer` and `phrase_slot` (`docs/mechanisms/query-operators.md`): `docs/status/phrase-alternatives.md`
- A2 [artifact] `test/multi_phrase_test.cc`: fixed cases and 600 random phrases against a brute-force model: `docs/status/phrase-alternatives.md`
- A3 [artifact] Seven mutants killed: `docs/status/phrase-alternatives.md`
- A4 [artifact] ASan+UBSan: `multi_phrase_test` clean: `docs/status/phrase-alternatives.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Xapiand repins to it for gate 7a L1 (`gate7a-l1`).
- Risk normal: a new scorer, model-checked (the auditor's mapping).

## May land early

None.
