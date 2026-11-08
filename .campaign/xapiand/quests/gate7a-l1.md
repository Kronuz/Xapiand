# gate7a-l1: Gate fixes and search parity

Risk: critical. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:10e0b0aaf (2026-10-29); its ledger `docs/status/gate7a-l1.md` is the record. Its machine state is in `state.json`: `campaign status gate7a-l1`.

## Outcome

Strata serves collapse; `?fuzzy`, `?nearest`, and weight cutoffs are removed from both engines. Invalid query shapes are rejected when queries are built, while strata serves PHRASE/NEAR alternatives through the added multi-phrase scorer.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Serve `?collapse` and `collapse_max` on strata: `docs/status/gate7a-l1.md`
- A2 [artifact] Remove `?fuzzy` and `?nearest` from both engines: `docs/status/gate7a-l1.md`
- A3 [artifact] Delete weight cutoffs: `docs/status/gate7a-l1.md`
- A4 [artifact] C3: replace removed `.terms` assertions with search checks; port `terms_diff.py` and `vocab_diff.py`: `docs/status/gate7a-l1.md`
- A5 [artifact] Round 2: serve PHRASE/NEAR over alternatives on strata; remove recorded invalid query shapes from both engines: `docs/status/gate7a-l1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-29: serve collapse on strata; remove `?fuzzy` and `?nearest`; delete weight cutoffs.
- Lead ruling: strata collapse behavior above `collapse_max` 1 is exact; keep strata behavior.
- PHRASE/NEAR alternatives served on strata; Xapian answers a client error for those shapes.
- Invalid query shapes are refused at query construction with 400.
- Risk critical: removes search parameters and changes query behavior (the lead's mapping).

## May land early

None.
