# xapiand-on-strata: Strata migration design

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4b37d3a67 (2026-11-03); its ledger `docs/status/xapiand-on-strata.md` is the record. Its machine state is in `state.json`: `campaign status xapiand-on-strata`.

## Outcome

The design defines the Xapiand-to-strata migration path, its seam inventory, decisions, ordered work, and gate rows. Phase 7a passed its gate; cross-platform gate work and subsequent phases remain open.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Seam inventory, coverage and gap table, decisions D1 to D12, ordered path, gate rows T1 to T10, and open questions Q1 to Q3: `docs/status/xapiand-on-strata.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner, 2026-10-20: single-node 7a, Q2 opaque `bytes` slots first, Q3 queryparser on the 7a path.
- Owner, 2026-10-29: phase gate split into 7a/7c, then Phase 8; 7b later.
- Owner, 2026-10-29: after 7c, queryparser accepts wildcards, fuzzy, and partial words inside phrases.
- Owner, 2026-10-29: remove Xapian path at 7c start; WAL stays until 7b; non-Apple storage keeps ordering barrier with warning.
- Lead, 2026-11-02: strict ISO 8601 dates; malformed booleans answer 400; retain recorded path normalization and path refusals.
- Risk normal: design study and planning only (the lead's mapping).

## May land early

None.
