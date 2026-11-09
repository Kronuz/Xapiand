# strata-engine-callbacks: Strata callbacks and aggregations

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:5c8b6651d (2026-10-28); its ledger `docs/status/strata-engine-callbacks.md` is the record. Its machine state is in `state.json`: `campaign status strata-engine-callbacks`.

## Outcome

Strata supports the recorded operators, expansions, posting sources, key makers, spies, and stored-value reads. `_terms` reads stored values where supported; term-list views and geo `_terms`/`_values` are refused.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1. SYNONYM, XOR, MAX, ELITE_SET, SCALE_WEIGHT with Xapian's scoring: `docs/status/strata-engine-callbacks.md`
- A2 [artifact] 2. WILDCARD and EDIT_DISTANCE expand over dictionaries once per search; field and pattern join through `interim::flat_term`: `docs/status/strata-engine-callbacks.md`
- A3 [artifact] 3. Posting sources scan the values plugin's column: `docs/status/strata-engine-callbacks.md`
- A4 [artifact] 4. Key makers order by key, then weight, then docid, through prospect's `key` hook: `docs/status/strata-engine-callbacks.md`
- A5 [artifact] 5. A spy sees every match with lazy `SegmentDocView`; `did()` is the id within the shard: `docs/status/strata-engine-callbacks.md`
- A6 [artifact] 6. Values read from the values plugin; term lists are not offered: `docs/status/strata-engine-callbacks.md`
- A7 [artifact] 7. Tests shared with the Xapian engine: `docs/status/strata-engine-callbacks.md`
- A8 [artifact] 8. `harness/numeric_range_check.py`: 797 ranges, 0 wrong; callbacks check compares 176 searches on both engines: `docs/status/strata-engine-callbacks.md`
- A9 [artifact] 9. `docs/mechanisms/strata-callbacks.md`: `docs/status/strata-engine-callbacks.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- R-P2: no term lists in strata or prospect.
- CR1: callback exceptions preserve their type; aggregation and serialisation errors remain 400.
- CR2: harness asserts 200 on compared searches; corrected aggregation names and `_filter`.
- Geo `_terms` and `_values` are refused with 400.
- Ties inside sort groups can differ between engines; not changed.
- Risk normal: changes callback and aggregation behavior (the lead's mapping).

## May land early

None.
