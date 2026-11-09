# x1-tests: Convert C++ tests to strata

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:0ae3505c9 (2026-10-29); its ledger `docs/status/x1-tests.md` is the record. Its machine state is in `state.json`: `campaign status x1-tests`.

## Outcome

The C++ unit tests exercise strata against models, counts, and literals rather than a second engine. The test targets build and pass; census-on and FUZZ-only targets remain unrun.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Convert the listed engine, WAL, pool, text, and offload tests in the committed changes: `docs/status/x1-tests.md`
- A2 [artifact] (no explicit criteria; from the Done list) Keep the converted test suite passing: ctest 25 of 25: `docs/status/x1-tests.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- A shard needs a key and refuses an empty field name; test documents set a key and name their fields.
- Test documents use version slots 21 and up because slot 1 belongs to the shard.
- No src/ fix was made for Document::field(""); tests avoid empty field names where they write or round-trip.
- Risk low: test conversion and coverage only (the lead's mapping).

## May land early

None.
