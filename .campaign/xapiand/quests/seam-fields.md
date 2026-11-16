# seam-fields: Field-aware engine seam

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:8e1183c1a (2026-10-28); its ledger `docs/status/seam-fields.md` is the record. Its machine state is in `state.json`: `campaign status seam-fields`.

## Outcome

The engine seam carries field identity alongside terms and queries. The Xapian adapter joins field and term into the existing term representation, while strata uses an interim field-term mapping.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] X1 `engine::Document` and query nodes carry a field; Xapian joins (field, term) into today's term: `docs/status/seam-fields.md`
- A2 [artifact] X2 Every D12 count site migrated; `git grep 'prefixed('` is empty: `docs/status/seam-fields.md`
- A3 [artifact] X3 SEARCH dumps, `:info` hashes, and shard term dump equal master's on Xapian: `docs/status/seam-fields.md`
- A4 [artifact] X4 Docs E2E 576 of 576: `docs/status/seam-fields.md`
- A5 [artifact] X5 Engine census unchanged, with one explained counter: `docs/status/seam-fields.md`
- A6 [artifact] X6 `harness/seam_check.sh` green: files=7 lines=158, equal to master: `docs/status/seam-fields.md`
- A7 [artifact] X7 Engine tests on both engines, ctest 25 of 25: `docs/status/seam-fields.md`
- A8 [artifact] X8 ASan+UBSan on engine tests: `docs/status/seam-fields.md`
- A9 [artifact] X9 Wrong-prefix mutant fails a test: `docs/status/seam-fields.md`
- A10 [artifact] X10 Interim mapping is `src/engine/strata/interim_terms.h`; adapter grep is 9 lines on master and 9 here: `docs/status/seam-fields.md`
- A11 [artifact] X11 `docs/mechanisms/seam-fields.md`: `docs/status/seam-fields.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead note: no third stem-marker copy; the analysis change removes the duplicated marker.
- Open decisions recorded: term-list API, key field representation, `Query::field(name)`, stem handling, 245-byte limit, and Xapian joined-term ambiguity.
- Risk normal: changes shared public engine and query APIs (the lead's mapping).

## May land early

None.
