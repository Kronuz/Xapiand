# prospect-key-field: prospect callers off key_prefix (F3)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:44415b6 (2026-10-28); its ledger `docs/status/key-field.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-key-field`.

## Outcome

prospect routes the key leaf by the key field's role, not a prefix byte: `check` loses the prefix argument, every first byte 0..255 is a key that is also a text term, and only the format bytes move (files 0x80000007).

## Acceptance criteria

Carried from the ledger's Done and Proof, each shown by the ledger as an artifact.

- A1 [artifact] Callers, tests and tools use `keyed`; `inverted_plugin::check` and `values_plugin::check` lose the prefix: `docs/status/key-field.md`
- A2 [artifact] `test/format13_test.cc`: every first byte 0..255 as both key and term, through flush, merge, reopen, replace, erase: `docs/status/key-field.md`
- A3 [artifact] Mutants V and W fail `format13_test`: `docs/status/key-field.md`
- A4 [artifact] F20 manifest tests follow the rule; digest and `.sx` fixtures unchanged: `docs/status/key-field.md`
- A5 [artifact] Release, ASan+UBSan and TSan ctest 49 of 49; CI 8 of 8 on prospect:d00f682 and strata:d4e43a9: `docs/status/key-field.md`
- A6 [artifact] Decoder-safety review on strata's manifest change: `docs/status/key-field.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lands with strata's side (strata `docs/status/key-field.md`, history quest `key-field`).
- Risk high: key routing in a persistent format (the auditor's mapping).

## May land early

None.
