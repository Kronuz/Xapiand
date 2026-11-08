# engine-query: Engine query and callback abstractions

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:1ab0ce077 (2026-10-26); its ledger `docs/status/engine-query.md` is the record. Its machine state is in `state.json`: `campaign status engine-query`.

## Outcome

Query construction, posting sources, sort keys, and aggregation spies use engine-neutral interfaces with Xapian adapters. Query results and recorded corpora remain stable, with the X4 boundary and several extreme-value or malformed-input cases left as follow-ups.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Step 1: `engine::Query` and three callback bases exist without Xapian headers: `docs/status/engine-query.md`
- A2 [artifact] Step 1: Xapian implementation translates queries and adapts callbacks and registry: `docs/status/engine-query.md`
- A3 [artifact] Step 1: Tests cover named translation shapes, adapters, bounds, ownership, and serialization errors: `docs/status/engine-query.md`
- A4 [artifact] Step 1: Migrate query producers and remove the named Xapian query/parser/source types from them: `docs/status/engine-query.md`
- A5 [artifact] Step 1: `get_mset` and `get_rset` take `engine::Query`; translate text before checkout and outside retry loop: `docs/status/engine-query.md`
- A6 [artifact] Step 1: Remote server registers the three sources through adapters: `docs/status/engine-query.md`
- A7 [artifact] Step 1: Unit tests, 339 requests / 576 of 576 assertions, byte-identical 301-answer corpus: `docs/status/engine-query.md`
- A8 [artifact] Step 1: ASan+UBSan results show no reports beyond baseline third-party lines: `docs/status/engine-query.md`
- A9 [artifact] Step 1: Guard each observable translation with a caught revert check; document equivalent binary/list form: `docs/status/engine-query.md`
- A10 [artifact] Step 2: Migrate keymaker and aggregations to engine callback bases and `DocView`: `docs/status/engine-query.md`
- A11 [artifact] Step 2: `get_mset` wraps sorter and spy; remote server registers both through adapters: `docs/status/engine-query.md`
- A12 [artifact] Step 2: E2E, extended query corpus, and two-node cluster range behavior unchanged: `docs/status/engine-query.md`
- A13 [artifact] Step 2: ASan+UBSan and revert checks: `docs/status/engine-query.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Query uses a flat immutable node tree with named factories; binary and list combine forms remain distinct.
- `Query::empty()` follows Xapian simplification; text nodes are never empty without parsing.
- Text flags and stem strategies use engine-owned constants mapped by the Xapian implementation.
- PostingSource is a value-slot filter; a cursor-style source is a separate future base.
- QueryParser errors surface as Xapian errors until X4; verbose `#xapian_query` translates for display.
- Risk high: Adds shared query and callback APIs at the engine seam (the lead's mapping).

## May land early

None.
