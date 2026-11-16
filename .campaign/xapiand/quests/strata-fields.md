# strata-fields: Field-aware strata index

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:9fa890ecc (2026-10-28); its ledger `docs/status/strata-fields.md` is the record. Its machine state is in `state.json`: `campaign status strata-fields`.

## Outcome

The strata index stores field declarations and field-term postings, with key data separate from inverted terms. The WAL document format is 3; formats 1 and 2 are refused.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] SF1 Fields declared through field table, (field, term) postings: `docs/status/strata-fields.md`
- A2 [artifact] SF2 `_id` routes through key field; no host-built key term, no id term in `inverted`: `docs/status/strata-fields.md`
- A3 [artifact] SF3 / C2 `interim_terms.h` and `keys.h` deleted; specified adapter grep is empty: `docs/status/strata-fields.md`
- A4 [artifact] SF4 Engine tests on both engines: ctest 28 of 28: `docs/status/strata-fields.md`
- A5 [artifact] SF5 callbacks check: 208 searches, 0 different; numeric range: 797 ranges, 0 wrong; query-depth check results as recorded: `docs/status/strata-fields.md`
- A6 [artifact] SF6 Docs E2E: Xapian 576 of 576; strata 507 of 576, with recorded refusals: `docs/status/strata-fields.md`
- A7 [artifact] SF7 ASan+UBSan engine tests; TSan tests with 0 warnings: `docs/status/strata-fields.md`
- A8 [artifact] SF8 Mutants M1-M7 all killed: `docs/status/strata-fields.md`
- A9 [artifact] SF9 `check_fields(table_size, key_field)` runs on every segment dictionary at shard open: `docs/status/strata-fields.md`
- A10 [artifact] SF10 Address review follow-ups CR2, CR4, CR6: `docs/status/strata-fields.md`
- A11 [artifact] SF11 `docs/mechanisms/strata-fields.md`; other named mechanism notes updated: `docs/status/strata-fields.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead ruling, 2026-10-28: F3 deletion of `index_options::key_prefix` is separate.
- Lead ruling, 2026-10-28: `_id` is a key-role field; key bytes carry type character and encoded id.
- A-SF1: key field is `engine::KEY_FIELD`; key terms have no postings but count in document length.
- `Query::field(name)` uses field-name prefix matching; `Query::field(KEY_FIELD)` lists keys.
- WAL document format 3; formats 1 and 2 are refused.
- Risk high: changes persistent index and WAL formats (the lead's mapping).

## May land early

None.
